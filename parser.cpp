//
// vmextract.cpp / parser.cpp
// -------------------------------------------------------------
// A single-file example that:
//  - Skips "nop" instructions so they don't appear as an operand
//  - Recognizes rip-based addresses ("rip+0x189b5")
//  - Supports extended x86-64 registers (r8..r15, XMM, YMM, ZMM)
//  - Provides parseTrace(...) and parseOperand(...) etc.
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <regex>
#include <cstdio>  // for printf, FILE*, etc.
#include "core.hpp"
#include "parser.hpp"
using namespace std;
/*
// ---------------------------------------------------------------------------
// Suppose your "core.hpp" has something like:
//   struct Operand { ... };
//   struct Inst { ... };
// For demo, we define minimal placeholders here if you want to test in isolation
// (Remove or comment out if you already have a real core.hpp)
#ifndef DEMO_CORE_HPP
#define DEMO_CORE_HPP

enum class OperandType { IMM, REG, MEM, UNK };
struct Operand
{
    OperandType ty;
    bool issegaddr = false;
    int  bit       = 0;
    int  tag       = 0;
    std::string segreg;
    std::string field[5];
};

struct Inst
{
    int id;
    std::string addr;
    uint64_t addrn;
    std::string assembly;
    std::string opcstr;
    int oprnum   = 0;
    std::vector<std::string> oprs;    // raw operand strings
    Operand*     oprd[3]   = {nullptr,nullptr,nullptr};

    uint64_t ctxreg[8];
    uint64_t raddr;
    uint64_t waddr;
};
#endif // DEMO_CORE_HPP

// ---------------------------------------------------------------------------
// Suppose your "parser.hpp" declares parseTrace, parseOperand, etc.
// We'll inline them here for a single-file demonstration:
#ifndef DEMO_PARSER_HPP
#define DEMO_PARSER_HPP

void parseTrace(std::ifstream *infile, std::list<Inst> *L);
void parseOperand(std::list<Inst>::iterator begin,
                  std::list<Inst>::iterator end);

// Some optional print functions:
void printfirst3inst(std::list<Inst> *L);
void printTraceHuman(std::list<Inst> &L, std::string fname);
void printTraceLLSE(std::list<Inst> &L, std::string fname);

#endif // DEMO_PARSER_HPP
*/
// ---------------------------------------------------------------------------
// createDataOperand(...) - handle GPR (8,16,32,64), SSE/AVX, immediate
// ---------------------------------------------------------------------------
static Operand* createDataOperand(const std::string &s)
{
    // Patterns for GPR
    static const char* gpr64 = R"((?:rip|r(1[0-5]|[8-9])|rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp))";
    static const char* gpr32 = R"((?:rip|r(1[0-5]|[8-9])d|eax|ebx|ecx|edx|esi|edi|ebp|esp))";
    static const char* gpr16 = R"((?:rip|r(1[0-5]|[8-9])w|ax|bx|cx|dx|si|di|bp|sp))";
    static const char* gpr8  = R"((?:rip|r(1[0-5]|[8-9])b|al|ah|bl|bh|cl|ch|dl|dh|spl|bpl|sil|dil))";

    // SSE, AVX, AVX-512
    static const char* xmm = R"((?:xmm(3[0-1]|[0-2]?\d)))";
    static const char* ymm = R"((?:ymm(3[0-1]|[0-2]?\d)))";
    static const char* zmm = R"((?:zmm(3[0-1]|[0-2]?\d)))";

    // FPU st(0..7), MMX mm(0..7)
    static const char* st  = R"((?:st([0-7])))";
    static const char* mmx = R"((?:mm([0-7])))";

    // immediate
    static const std::regex immPat(R"(0x[[:xdigit:]]+)", std::regex::icase);

    std::regex gpr64_pat(gpr64, std::regex::icase);
    std::regex gpr32_pat(gpr32, std::regex::icase);
    std::regex gpr16_pat(gpr16, std::regex::icase);
    std::regex gpr8_pat (gpr8,  std::regex::icase);

    std::regex xmm_pat(xmm, std::regex::icase);
    std::regex ymm_pat(ymm, std::regex::icase);
    std::regex zmm_pat(zmm, std::regex::icase);

    std::regex st_pat (st,  std::regex::icase);
    std::regex mmx_pat(mmx, std::regex::icase);

    Operand* opr = new Operand();
    std::smatch m;

    // XMM, YMM, ZMM
    if      (std::regex_match(s, m, zmm_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 512;
        opr->field[0] = m[0];
    }
    else if (std::regex_match(s, m, ymm_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 256;
        opr->field[0] = m[0];
    }
    else if (std::regex_match(s, m, xmm_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 128;
        opr->field[0] = m[0];
    }
    // 64-bit GPR
    else if (std::regex_match(s, m, gpr64_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 64;
        opr->field[0] = m[0];
    }
    // 32-bit GPR
    else if (std::regex_match(s, m, gpr32_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 32;
        opr->field[0] = m[0];
    }
    // 16-bit GPR
    else if (std::regex_match(s, m, gpr16_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 16;
        opr->field[0] = m[0];
    }
    // 8-bit GPR
    else if (std::regex_match(s, m, gpr8_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 8;
        opr->field[0] = m[0];
    }
    // st(0..7)
    else if (std::regex_match(s, m, st_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 80;  // or 128 if you prefer
        opr->field[0] = m[0];
    }
    // mm(0..7)
    else if (std::regex_match(s, m, mmx_pat)) {
        opr->ty  = OperandType::REG;
        opr->bit = 64;
        opr->field[0] = m[0];
    }
    // immediate
    else if (std::regex_match(s, m, immPat)) {
        opr->ty  = OperandType::IMM;
        opr->bit = 64; // or 32 as default
        opr->field[0] = m[0];
    }
    else {
        std::cerr << "[createDataOperand] Unknown data operand: " << s << "\n";
        opr->ty  = OperandType::UNK;
        opr->bit = 64;
        opr->field[0] = s;
    }
    return opr;
}

// ---------------------------------------------------------------------------
// createAddrOperand(...) - parse memory expressions, inc. "rip+0x189b5"
// ---------------------------------------------------------------------------
static Operand* createAddrOperand(const std::string &s)
{
    // We handle forms like: 
    //   reg64 + R"(\+)" + reg64 + R"(\*[[:digit:]]+)" + R"([+-]0x[[:xdigit:]]+)"
    // etc., to match r8+rax*1+0x10 or rip+0x189b5
    // 
    // Our 'reg64' pattern includes "rip" to allow rip-relative addresses.

    static const std::string reg64 = R"((?:rip|r(1[0-5]|[8-9])|rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp))";

    // Patterns for different combos
    std::regex addr7(
        reg64 + R"(\+)"       // base + 
      + reg64 + R"(\*[[:digit:]]+)"  // index * scale
      + R"([+-]0x[[:xdigit:]]+)",    // displacement
        std::regex::icase
    );
    std::regex addr6(
        reg64 + R"(\*[[:digit:]]+)" 
      + R"([+-]0x[[:xdigit:]]+)",
        std::regex::icase
    );
    std::regex addr5(
        reg64 + R"(\+)" 
      + reg64 + R"(\*[[:digit:]]+)",
        std::regex::icase
    );
    std::regex addr4(
        reg64 + R"([+-]0x[[:xdigit:]]+)",
        std::regex::icase
    );
    std::regex addr3(
        reg64 + R"(\*[[:digit:]]+)",
        std::regex::icase
    );
    std::regex addr2(
        reg64,
        std::regex::icase
    );
    std::regex addr1(
        R"(0x[[:xdigit:]]+)",
        std::regex::icase
    );

    Operand* opr = new Operand();
    std::smatch m;

    if      (std::regex_match(s, m, addr7)) {
        opr->ty = OperandType::MEM;
        opr->tag = 7;
        opr->field[0] = s;
    }
    else if (std::regex_match(s, m, addr6)) {
        opr->ty = OperandType::MEM;
        opr->tag = 6;
        opr->field[0] = s;
    }
    else if (std::regex_match(s, m, addr5)) {
        opr->ty = OperandType::MEM;
        opr->tag = 5;
        opr->field[0] = s;
    }
    else if (std::regex_match(s, m, addr4)) {
        opr->ty = OperandType::MEM;
        opr->tag = 4;
        opr->field[0] = s;
    }
    else if (std::regex_match(s, m, addr3)) {
        opr->ty = OperandType::MEM;
        opr->tag = 3;
        opr->field[0] = s;
    }
    else if (std::regex_match(s, m, addr2)) {
        opr->ty = OperandType::MEM;
        opr->tag = 2;
        opr->field[0] = s;
    }
    else if (std::regex_match(s, m, addr1)) {
        opr->ty = OperandType::MEM;
        opr->tag = 1;
        opr->field[0] = s;
    }
    else {
        std::cerr << "[createAddrOperand] Unknown extended mem operand: " << s << "\n";
        opr->ty  = OperandType::UNK;
        opr->tag = 0;
        opr->field[0] = s;
    }

    return opr;
}

// ---------------------------------------------------------------------------
// createOperand(...) - decides if it's memory “ptr …” or data (reg/imm).
// ---------------------------------------------------------------------------
static Operand* createOperand(const std::string &s)
{
    // If it has "ptr" or bracket, treat as memory
    static std::regex memHint(R"((ptr\s*\[)|(\[))", std::regex::icase);

    // We'll do a quick approach:
    if (std::regex_search(s, memHint)) {
        // memory operand
        // parse out the bracket content if you want
        size_t start = s.find('[');
        size_t end   = s.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string inside = s.substr(start+1, end - (start+1));
            // pass to createAddrOperand
            Operand* opr = createAddrOperand(inside);
            // guess bit size based on "byte ptr" etc. if you want
            opr->bit = 64; 
            return opr;
        } else {
            // fallback
            Operand* opr = new Operand();
            opr->ty  = OperandType::MEM;
            opr->bit = 64;
            opr->field[0] = s;
            return opr;
        }
    } else {
        // register or immediate
        return createDataOperand(s);
    }
}

// ---------------------------------------------------------------------------
// parseOperand(...) - parse each Inst's raw oprs[] strings into Operand
// ---------------------------------------------------------------------------
void parseOperand(std::list<Inst>::iterator begin,
                  std::list<Inst>::iterator end)
{
    for (auto it = begin; it != end; ++it) {
        for (int i = 0; i < (int)it->oprs.size() && i < 3; i++) {
            it->oprd[i] = createOperand(it->oprs[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// parseTrace(...) - read instructions from *infile
//   - skip instructions like "nop" entirely if they appear
// ---------------------------------------------------------------------------
void parseTrace(std::ifstream *infile, std::list<Inst> *L)
{
    std::string line;
    int num = 1;

    while (std::getline(*infile, line)) {
        if (line.empty()) continue;

        std::istringstream strbuf(line);
        std::string temp, disas;

        // Build a new Inst
        Inst ins;
        ins.id = num++;

        // 1) Instruction address
        std::getline(strbuf, ins.addr, ';');
        // if ins.addr is blank => skip
        if (ins.addr.empty()) continue;
        // convert to 64-bit
        ins.addrn = std::stoull(ins.addr, nullptr, 16);

        // 2) Disassembly
        std::getline(strbuf, disas, ';');
        ins.assembly = disas;

        // parse out the opcode from the first token
        {
            std::istringstream dbuf(disas);
            std::getline(dbuf, ins.opcstr, ' ');
            // If the opcode is "nop", skip the rest
            if (ins.opcstr == "nop") {
                // ignore or push anyway? If you truly want to skip, do:
                continue;
            }
            // Else gather potential operands
            while (dbuf.good()) {
                std::getline(dbuf, temp, ',');
                if (!temp.empty()) {
                    // trim
                    auto st = temp.find_first_not_of(" \t");
                    if (st != std::string::npos) temp = temp.substr(st);
                    auto en = temp.find_last_not_of(" \t");
                    if (en != std::string::npos) temp = temp.substr(0, en+1);

                    if (!temp.empty()) {
                        ins.oprs.push_back(temp);
                    }
                }
            }
        }
        ins.oprnum = ins.oprs.size();

        // 3) Next 8 context registers
        for (int i = 0; i < 8; i++) {
            if (!std::getline(strbuf, temp, ',')) break;
            ins.ctxreg[i] = std::stoull(temp, nullptr, 16);
        }
        // 4) read/write addresses
        if (std::getline(strbuf, temp, ',')) {
            ins.raddr = std::stoull(temp, nullptr, 16);
        }
        if (std::getline(strbuf, temp, ',')) {
            ins.waddr = std::stoull(temp, nullptr, 16);
        }

        L->push_back(ins);
    }
}

// ---------------------------------------------------------------------------
// printfirst3inst(...)
// ---------------------------------------------------------------------------
void printfirst3inst(std::list<Inst> *L)
{
    int i = 0;
    for (auto it = L->begin(); it != L->end() && i < 3; ++it, ++i) {
        std::cout << it->opcstr << '\t';
        for (auto &op : it->oprs) {
            std::cout << op << '\t';
        }
        // print ctx regs
        for (int j = 0; j < 8; j++) {
            printf("%llx, ", (unsigned long long)it->ctxreg[j]);
        }
        // print read/write
        printf("%llx,%llx,\n",
               (unsigned long long)it->raddr,
               (unsigned long long)it->waddr);
    }
}

// ---------------------------------------------------------------------------
// printTraceHuman(...)
// ---------------------------------------------------------------------------
void printTraceHuman(std::list<Inst> &L, std::string fname)
{
    FILE *fp = fopen(fname.c_str(), "w");
    if (!fp) {
        std::cerr << "[printTraceHuman] Cannot open " << fname << "\n";
        return;
    }
    for (auto &ins : L) {
        fprintf(fp, "%s %s  \t", ins.addr.c_str(), ins.assembly.c_str());
        fprintf(fp, "(%llx, %llx)\n",
                (unsigned long long)ins.raddr,
                (unsigned long long)ins.waddr);
    }
    fclose(fp);
}

// ---------------------------------------------------------------------------
// printTraceLLSE(...)
// ---------------------------------------------------------------------------
void printTraceLLSE(std::list<Inst> &L, std::string fname)
{
    FILE *fp = fopen(fname.c_str(), "w");
    if (!fp) {
        std::cerr << "[printTraceLLSE] Cannot open " << fname << "\n";
        return;
    }
    for (auto &ins : L) {
        fprintf(fp, "%s;%s;", ins.addr.c_str(), ins.assembly.c_str());
        for (int i = 0; i < 8; i++) {
            fprintf(fp, "%llx,", (unsigned long long)ins.ctxreg[i]);
        }
        fprintf(fp, "%llx,%llx,\n",
                (unsigned long long)ins.raddr,
                (unsigned long long)ins.waddr);
    }
    fclose(fp);
}

// ---------------------------------------------------------------------------
// main for testing
// ---------------------------------------------------------------------------
#ifdef DEMO_MAIN
int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <tracefile>\n";
        return 1;
    }
    std::ifstream infile(argv[1]);
    if (!infile.is_open()) {
        std::cerr << "Cannot open: " << argv[1] << "\n";
        return 1;
    }

    std::list<Inst> instlist;
    parseTrace(&infile, &instlist);
    infile.close();

    parseOperand(instlist.begin(), instlist.end());

    // Print first 3 instructions
    printfirst3inst(&instlist);

    return 0;
}
#endif

