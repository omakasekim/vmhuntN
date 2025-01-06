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

using namespace std;

#include "core.hpp"
#include "parser.hpp"

// A helper: big pattern for "64-bit registers" (r0..r15 plus classic ones)
static const char* reg64_pattern = R"(r(1[0-5]|[8-9])|rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)";
// If you want to exclude, say, r0 or something, adjust accordingly.

// Create memory-operand from string s
Operand *createAddrOperand(string s)
{
    // We'll build one big pattern for 64-bit registers:
    //   e.g. "r8|r9|r10|...|rax|rbx|..." etc.
    // Then we reuse it inside the subpatterns.
    // For clarity, I'll store the string in reg64_pattern above.

    // Let’s build subpatterns that use `(...)` for the 64-bit reg:
    // Example:   "(rax|rbx|rcx|...)" → "(" + string(reg64_pattern) + ")"
    string r64 = string("(") + reg64_pattern + string(")");
    
    // Now define your regex patterns with this expanded set:
    regex addr1(R"(0x[[:xdigit:]]+)");
    regex addr2(r64);  // single 64-bit reg
    // e.g.  rax*2 or r8*4
    regex addr3((r64 + R"(\*([[:digit:]]+))").c_str());

    // e.g.  rax+0x10 or r8-0x123
    regex addr4((r64 + R"((\+|-)(0x[[:xdigit:]]+))").c_str());
    // e.g.  rax+rbx*2 or r8+rcx*4
    regex addr5((r64 + R"(\+)" + r64 + R"(\*([[:digit:]]+))").c_str());
    // e.g.  rax*2+0x10 or r8*4-0x123
    regex addr6((r64 + R"(\*([[:digit:]]+))(\+|-)(0x[[:xdigit:]]+))").c_str());
    // e.g.  rax+rbx*2+0x10
    regex addr7((r64 + R"(\+)" + r64 + R"(\*([[:digit:]]+))(\+|-)(0x[[:xdigit:]]+))").c_str());

    Operand *opr = new Operand();
    smatch m;

    if      (regex_search(s, m, addr7)) {
        // e.g. rax+rbx*2+0x10
        opr->ty = Operand::MEM;
        opr->tag = 7;
        opr->field[0] = m[1]; // first reg
        opr->field[1] = m[2]; // second reg
        opr->field[2] = m[3]; // scale
        opr->field[3] = m[4]; // + or -
        opr->field[4] = m[5]; // displacement
    }
    else if (regex_search(s, m, addr4)) {
        // e.g. rax+0x10
        opr->ty = Operand::MEM;
        opr->tag = 4;
        opr->field[0] = m[1]; // reg
        opr->field[1] = m[2]; // + or -
        opr->field[2] = m[3]; // displacement
    }
    else if (regex_search(s, m, addr5)) {
        // e.g. rax+rbx*2
        opr->ty = Operand::MEM;
        opr->tag = 5;
        opr->field[0] = m[1]; // first reg
        opr->field[1] = m[2]; // second reg
        opr->field[2] = m[3]; // scale
    }
    else if (regex_search(s, m, addr6)) {
        // e.g. rax*2+0x10
        opr->ty = Operand::MEM;
        opr->tag = 6;
        opr->field[0] = m[1]; // reg
        opr->field[1] = m[2]; // scale
        opr->field[2] = m[3]; // + or -
        opr->field[3] = m[4]; // displacement
    }
    else if (regex_search(s, m, addr3)) {
        // e.g. rax*2
        opr->ty = Operand::MEM;
        opr->tag = 3;
        opr->field[0] = m[1]; // reg
        opr->field[1] = m[2]; // scale
    }
    else if (regex_search(s, m, addr1)) {
        // e.g. 0x12345678
        opr->ty = Operand::MEM;
        opr->tag = 1;
        opr->field[0] = m[0];
    }
    else if (regex_search(s, m, addr2)) {
        // e.g. rax
        opr->ty = Operand::MEM;
        opr->tag = 2;
        opr->field[0] = m[0];
    }
    else {
        cout << "Unknown addr operands: " << s << endl;
    }

    return opr;
}

Operand *createDataOperand(string s)
{
    // Expand your register detection beyond the 8 "classic" ones:
    // e.g. also include r8, r8d, r8w, r8b, etc. if needed.
    // For simplicity, here’s a direct approach:

    // 64-bit GPR:  r8..r15 plus rax..rbp
    // (we skip partial names like r8d, r8w, etc. below—add them if you want)
    static const char* full_reg64 = R"(r(1[0-5]|[8-9])|rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)";
    // 32-bit GPR:   r8d..r15d plus eax..ebp
    static const char* full_reg32 = R"(r(1[0-5]|[8-9])d|eax|ebx|ecx|edx|esi|edi|esp|ebp)";
    // 16-bit GPR:   r8w..r15w plus ax,bx,dx,si,di,sp,bp
    static const char* full_reg16 = R"(r(1[0-5]|[8-9])w|ax|bx|cx|dx|si|di|sp|bp)";
    // 8-bit GPR:    r8b..r15b plus al,ah,bl,bh,cl,ch,dl,dh,spl,bpl,sil,dil
    static const char* full_reg8  = R"(r(1[0-5]|[8-9])b|al|ah|bl|bh|cl|ch|dl|dh|spl|bpl|sil|dil)";

    // We'll also keep your immediate pattern:
    regex immvalue(R"(0x[[:xdigit:]]+)");
    
    // Build regex objects:
    regex reg64(full_reg64);
    regex reg32(full_reg32);
    regex reg16(full_reg16);
    regex reg8 (full_reg8);

    Operand *opr = new Operand();
    smatch m;

    // Check 64-bit first:
    if (regex_search(s, m, reg64)) {
        opr->ty    = Operand::REG;
        opr->bit   = 64;
        opr->field[0] = m[0];
    }
    else if (regex_search(s, m, reg32)) {
        opr->ty    = Operand::REG;
        opr->bit   = 32;
        opr->field[0] = m[0];
    }
    else if (regex_search(s, m, reg16)) {
        opr->ty    = Operand::REG;
        opr->bit   = 16;
        opr->field[0] = m[0];
    }
    else if (regex_search(s, m, reg8)) {
        opr->ty    = Operand::REG;
        opr->bit   = 8;
        opr->field[0] = m[0];
    }
    else if (regex_search(s, m, immvalue)) {
        // Immediate value
        opr->ty    = Operand::IMM;
        opr->bit   = 64;
        opr->field[0] = m[0];
    }
    else {
        cout << "Unknown data operands: " << s << endl;
    }

    return opr;
}

void parseTrace(ifstream *infile, list<Inst> *L)
{
    string line;
    int num = 1;

    // Use safer reading pattern: while we can actually get a line
    while (getline(*infile, line)) {
        // Skip blank lines
        if (line.empty()) {
            continue;
        }

        istringstream strbuf(line);
        string temp, disasstr;

        Inst *ins = new Inst();
        ins->id = num++;

        // 1) Instruction address (string) up to ';'
        getline(strbuf, ins->addr, ';');
        ins->addrn = stoull(ins->addr, 0, 16); // convert to 64-bit

        // 2) Disassembly string
        getline(strbuf, disasstr, ';');
        ins->assembly = disasstr;

        // Extract opcode (ins->opcstr) from the first token in disasstr
        istringstream disasbuf(disasstr);
        getline(disasbuf, ins->opcstr, ' ');

        // Extract the comma-separated operands
        while (disasbuf.good()) {
            getline(disasbuf, temp, ',');
            // Trim leading/trailing spaces
            if (!temp.empty()) {
                // remove leading spaces
                auto startpos = temp.find_first_not_of(" \t");
                if (startpos != string::npos) {
                    temp = temp.substr(startpos);
                }
                // remove trailing spaces
                auto endpos = temp.find_last_not_of(" \t");
                if (endpos != string::npos) {
                    temp = temp.substr(0, endpos + 1);
                }
                if (!temp.empty()) {
                    ins->oprs.push_back(temp);
                }
            }
        }
        ins->oprnum = ins->oprs.size();

        // 3) Next 8 comma-separated hex strings => context registers
        for (int i = 0; i < 8; ++i) {
            if (!getline(strbuf, temp, ',')) {
                // not enough columns => break or handle error
                break; 
            }
            ins->ctxreg[i] = stoull(temp, 0, 16);
        }

        // 4) Next 2 for read/write addresses
        if (getline(strbuf, temp, ',')) {
            ins->raddr = stoull(temp, 0, 16);
        }
        if (getline(strbuf, temp, ',')) {
            ins->waddr = stoull(temp, 0, 16);
        }

        // Finally push the instruction into the list
        L->push_back(*ins);
    }
}
