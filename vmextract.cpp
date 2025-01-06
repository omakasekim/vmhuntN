// main.cpp (vmextract.cpp)
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <map>
#include <stack>
#include <vector>
#include <set>
#include <cstdint>    // for uint64_t
#include <cstdio>     // for printf, FILE, etc.

using namespace std;

#include "core.hpp"
#include "parser.hpp"

/*
 * Global list of instructions from the trace.
 */
list<Inst> instlist;

/*
 * Data structures for identifying functions (placeholders).
 */
struct FuncBody {
    int start;
    int end;
    int length;             // end - start
    uint64_t startAddr;     // changed to 64-bit
    uint64_t endAddr;       // changed to 64-bit
    int loopn;
};

struct Func {
    uint64_t callAddr;      // changed to 64-bit
    list<FuncBody*> body;
};

/*
 * Jump instruction names in x86-64 assembly (plus some older ones).
 */
static const string jmpInstrName[33] = {
    "jo","jno","js","jns","je","jz","jne","jnz","jb","jnae","jc","jnb","jae",
    "jnc","jbe","jna","ja","jnbe","jl","jnge","jge","jnl","jle","jng","jg",
    "jnle","jp","jpe","jnp","jpo","jcxz","jecxz","jmp"
};

/*
 * We'll store jump-opcode IDs and an instruction-enumeration map globally.
 */
static set<int> jmpset;          // Changed from pointer to actual object
static map<string, int> instenum; // Changed from pointer to actual object

/*
 * Return the mnemonic string for a given numeric opcode.
 */
string getOpcName(int opc, const map<string, int>& m)
{
    for (const auto &kv : m) {
        if (kv.second == opc) {
            return kv.first;
        }
    }
    return "unknown";
}

/*
 * Print instructions (for debugging).
 */
void printInstlist(const list<Inst>& L, const map<string, int>& m)
{
    for (const auto &ins : L) {
        cout << ins.id << " "
             << hex << ins.addrn << " "
             << ins.addr << " "
             << ins.opcstr << " "
             << getOpcName(ins.opc, m) << " "
             << dec << ins.oprnum << endl;
        for (const auto &op : ins.oprs) {
            cout << op << endl;
        }
    }
}

/*
 * Build a function map (placeholder logic).
 */
map<uint64_t, list<FuncBody*>*> buildFuncList(const list<Inst>& L)
{
    map<uint64_t, list<FuncBody*>*> funcmap;
    stack<list<Inst>::const_iterator> stk;

    for (auto it = L.begin(); it != L.end(); ++it) {
        if (it->opcstr == "call") {
            // push the iterator
            stk.push(it);
            // see if function is in the map
            auto pos = funcmap.find(it->addrn);
            if (pos == funcmap.end()) {
                // parse call operand (hex) as 64-bit
                if (!it->oprs.empty() && ishex(it->oprs[0])) {
                    uint64_t calladdr = stoull(it->oprs[0], nullptr, 16);
                    funcmap[calladdr] = nullptr;
                } else {
                    cerr << "[buildFuncList] Invalid call operand at instruction ID " << it->id << endl;
                }
            }
        }
        else if (it->opcstr == "ret") {
            if (!stk.empty()) {
                stk.pop();
            }
        }
    }
    return funcmap;
}

/*
 * Print only the keys in the function map.
 */
void printFuncmap(const map<uint64_t, list<FuncBody*>*>& funcmap)
{
    for (const auto &kv : funcmap) {
        cout << hex << kv.first << endl;
    }
}

/*
 * Build a map (mnemonic -> unique int).
 */
map<string, int> buildOpcodeMap(const list<Inst>& L)
{
    map<string, int> mp;
    for (const auto &ins : L) {
        if (mp.find(ins.opcstr) == mp.end()) {
            mp[ins.opcstr] = static_cast<int>(mp.size()) + 1;
        }
    }
    return mp;
}

/*
 * Lookup numeric opcode from a mnemonic (returns 0 if not found).
 */
int getOpc(const string &s, const map<string, int>& m)
{
    auto it = m.find(s);
    if (it != m.end()) {
        return it->second;
    }
    return 0;
}

/*
 * Check if integer opcode i is in the jump set.
 */
bool isjump(int i)
{
    return jmpset.find(i) != jmpset.end();
}

/*
 * Count how many indirect jumps by checking if operand[0] is not IMM.
 */
void countindjumps(const list<Inst>& L)
{
    int indjumpnum = 0;
    for (const auto &ins : L) {
        if (isjump(ins.opc)) {
            // If operand[0] is not IMM => indirect jump
            if (ins.oprd[0].ty != Operand::IMM) { // Corrected: Operand accessed by value
                ++indjumpnum;
                cout << ins.addr << "\t" << ins.opcstr << " " << ins.oprs[0] << endl;
            }
        }
    }
    cout << "number of indirect jumps: " << indjumpnum << endl;
}

/*
 * Simple peephole optimization that removes canceling pairs
 * (push/pop same, add/sub same, inc/dec same, etc.).
 * 
 * In 64-bit, there's no pushad/popad, so we omit them.
 */
void peephole(list<Inst>& L, const map<string, int>& m)
{
    auto it = L.begin();
    while (it != L.end()) {
        auto nxt = next(it);
        if (nxt == L.end()) {
            break;
        }
        bool erased = false;

        // Check pairs
        if ((it->opcstr == "push" && nxt->opcstr == "pop"
             && !it->oprs.empty() && !nxt->oprs.empty()
             && it->oprs[0] == nxt->oprs[0]) )
        {
            nxt = L.erase(nxt);
            it  = L.erase(it);
            erased = true;
            if (it != L.begin() && it != L.end()) {
                --it;
            }
        }
        else if ((it->opcstr == "pop" && nxt->opcstr == "push"
                  && !it->oprs.empty() && !nxt->oprs.empty()
                  && it->oprs[0] == nxt->oprs[0]) )
        {
            nxt = L.erase(nxt);
            it  = L.erase(it);
            erased = true;
            if (it != L.begin() && it != L.end()) {
                --it;
            }
        }
        else if ((it->opcstr == "add" && nxt->opcstr == "sub"
                  && it->oprs.size() == 2 && nxt->oprs.size() == 2
                  && it->oprs[0] == nxt->oprs[0]
                  && it->oprs[1] == nxt->oprs[1]) )
        {
            nxt = L.erase(nxt);
            it  = L.erase(it);
            erased = true;
            if (it != L.begin() && it != L.end()) {
                --it;
            }
        }
        else if ((it->opcstr == "sub" && nxt->opcstr == "add"
                  && it->oprs.size() == 2 && nxt->oprs.size() == 2
                  && it->oprs[0] == nxt->oprs[0]
                  && it->oprs[1] == nxt->oprs[1]) )
        {
            nxt = L.erase(nxt);
            it  = L.erase(it);
            erased = true;
            if (it != L.begin() && it != L.end()) {
                --it;
            }
        }
        else if ((it->opcstr == "inc" && nxt->opcstr == "dec"
                  && !it->oprs.empty() && !nxt->oprs.empty()
                  && it->oprs[0] == nxt->oprs[0]) )
        {
            nxt = L.erase(nxt);
            it  = L.erase(it);
            erased = true;
            if (it != L.begin() && it != L.end()) {
                --it;
            }
        }
        else if ((it->opcstr == "dec" && nxt->opcstr == "inc"
                  && !it->oprs.empty() && !nxt->oprs.empty()
                  && it->oprs[0] == nxt->oprs[0]) )
        {
            nxt = L.erase(nxt);
            it  = L.erase(it);
            erased = true;
            if (it != L.begin() && it != L.end()) {
                --it;
            }
        }

        if (!erased) {
            ++it;
        }
    }
}

/*
 * A structure capturing a block of instructions (push or pop) we consider a context save/restore.
 */
struct ctxswitch {
    list<Inst>::iterator begin;
    list<Inst>::iterator end;
    uint64_t sd;  // stack depth or pointer (64-bit)
};

/*
 * Global lists storing context saves and restores, then pairing them up.
 */
list<ctxswitch> ctxsave;
list<ctxswitch> ctxrestore;
list<pair<ctxswitch, ctxswitch>> ctxswh;

/*
 * Check if a string is one of our recognized 64-bit regs (rax, rbx, etc.).
 * If your code only needs minimal GPR set, keep it short.
 * If you want r8..r15, add them too.
 */
bool isreg(const string &s)
{
    static const set<string> gprs64 = {
        "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        "rip",
        "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
        "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15",
        "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7",
        "ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15",
        "zmm0","zmm1","zmm2","zmm3","zmm4","zmm5","zmm6","zmm7",
        "zmm8","zmm9","zmm10","zmm11","zmm12","zmm13","zmm14","zmm15"
    };
    return (gprs64.find(s) != gprs64.end());
}

/*
 * Check if [i1..i2) are all "push <reg>", and no repeated regs.
 */
bool chkpush(list<Inst>::iterator i1, list<Inst>::iterator i2)
{
    int opcpush = getOpc("push", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpush) return false;
        if (it->oprs.empty())   return false;
        if (!isreg(it->oprs[0])) {
            return false;
        }
    }
    // ensure no repeated registers
    set<string> used;
    for (auto it = i1; it != i2; ++it) {
        const string &rname = it->oprs[0];
        if (!used.insert(rname).second) {
            return false;
        }
    }
    return true;
}

/*
 * Check if [i1..i2) are all "pop <reg>", and no repeated regs.
 */
bool chkpop(list<Inst>::iterator i1, list<Inst>::iterator i2)
{
    int opcpop = getOpc("pop", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpop) return false;
        if (it->oprs.empty())  return false;
        if (!isreg(it->oprs[0])) {
            return false;
        }
    }
    set<string> used;
    for (auto it = i1; it != i2; ++it) {
        const string &rname = it->oprs[0];
        if (!used.insert(rname).second) {
            return false;
        }
    }
    return true;
}

/*
 * Search the instruction list L and extract "VM" snippets:
 * sequences of 7 pushes or 7 pops in a row.
 */
void vmextract(list<Inst>& L)
{
    // We'll look for exactly 7 consecutive pushes or pops.
    // If you want a different count, change "7" to something else.
    const int VM_COUNT = 7;

    for (auto it = L.begin(); it != L.end(); ++it) {
        // Ensure we have at least 7 instructions from [it..end).
        if (distance(it, L.end()) < VM_COUNT) {
            break; // not enough instructions
        }
        auto it7 = it;
        advance(it7, VM_COUNT);

        // Check 7 pushes
        if (chkpush(it, it7)) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = it7;
            // Assume ctxreg[6] is 'rsp' if your trace sets that
            // For a real 64-bit code, it might be index 7 or so, adjust if needed
            // Ensure that ctxreg index corresponds to 'rsp'
            cs.sd    = it7->ctxreg[6];  // 64-bit stack pointer
            ctxsave.push_back(cs);
            cout << "[vmextract] push found at instruction ID " << it->id << " (" << it->addr << ")\n";
            // Skip the next 6 instructions as they are part of this VM snippet
            it = it7;
            continue;
        }
        // Check 7 pops
        else if (chkpop(it, it7)) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = it7;
            cs.sd    = it->ctxreg[6];
            ctxrestore.push_back(cs);
            cout << "[vmextract] pop found at instruction ID " << it->id << " (" << it->addr << ")\n";
            // Skip the next 6 instructions as they are part of this VM snippet
            it = it7;
            continue;
        }
    }

    // Pair up saves and restores by matching sd
    for (const auto &sv : ctxsave) {
        for (const auto &rs : ctxrestore) {
            if (sv.sd == rs.sd) {
                ctxswh.emplace_back(make_pair(sv, rs));
                // If you only want to pair each save with its first matching restore, uncomment the next line
                // break;
            }
        }
    }
}

/*
 * Write the extracted VM snippets to separate files (vm1.txt, vm2.txt, etc.).
 */
void outputvm(const list<pair<ctxswitch, ctxswitch>>& ctxswh)
{
    int n = 1;
    for (const auto &pairCS : ctxswh) {
        auto i1 = pairCS.first.begin;
        auto i2 = pairCS.second.end;
        string vmfile = "vm" + to_string(n++) + ".txt";
        FILE* fp = fopen(vmfile.c_str(), "w");
        if (!fp) {
            cerr << "[outputvm] Failed to open " << vmfile << endl;
            continue;
        }
        // Dump instructions from [i1..i2)
        for (auto it = i1; it != i2; ++it) {
            fprintf(fp, "%s;%s;", it->addr.c_str(), it->assembly.c_str());
            // print context registers
            for (int j = 0; j < 8; ++j) {
                fprintf(fp, "0x%llx,", static_cast<unsigned long long>(it->ctxreg[j]));
            }
            // print read/write addresses
            fprintf(fp, "0x%llx,0x%llx\n",
                    static_cast<unsigned long long>(it->raddr),
                    static_cast<unsigned long long>(it->waddr));
        }
        fclose(fp);
        cout << "[outputvm] Written VM snippet to " << vmfile << endl;
    }
}

/*
 * Check if a string is hex ("0x...").
 */
bool ishex(const string &s)
{
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return true;
    }
    return false;
}

/*
 * Structures for a CFG, if you wish to build/print it in 64-bit style.
 */
struct Edge;

struct BB {
    vector<Inst> instvec;
    uint64_t beginaddr;
    uint64_t endaddr;
    vector<int> out;
    int ty;  // 1: end with jump; 2: no jump

    BB() : beginaddr(0), endaddr(0), ty(0) {}
    BB(uint64_t b, uint64_t e) : beginaddr(b), endaddr(e), ty(0) {}
    BB(uint64_t b, uint64_t e, int t) : beginaddr(b), endaddr(e), ty(t) {}
};

struct Edge {
    int from;
    int to;
    bool jumped;
    int ty;       
    int count;
    uint64_t fromaddr;
    uint64_t toaddr;

    Edge() : from(0), to(0), jumped(false), ty(0), count(0),
             fromaddr(0), toaddr(0) {}
    Edge(uint64_t a1, uint64_t a2, int t, int c)
        : from(0), to(0), jumped(false), ty(t), count(c),
          fromaddr(a1), toaddr(a2) {}
};

class CFG {
    vector<BB> bbs;
    vector<Edge> edges;
    map<uint64_t,int> bbmap;  // address -> index of BB, if used

public:
    CFG() {}
    CFG(const list<Inst>& L);
    void checkConsist();
    void showCFG();
    void outputDot();
    void outputSimpleDot();
    void showTrace(const list<Inst>& L);
    void compressCFG();
};

/*
 * Build opcode map, fill in numeric opcodes in Inst, build jump set.
 */
void preprocess(list<Inst>& L)
{
    // 1) Build opcode map
    instenum = buildOpcodeMap(L);
    // 2) Fill in numeric opcodes
    for (auto &ins : L) {
        ins.opc = getOpc(ins.opcstr, instenum);
    }
    // 3) Build the jump set
    for (const auto &mn : jmpInstrName) {
        int code = getOpc(mn, instenum);
        if (code != 0) {
            jmpset.insert(code);
        }
    }
}

/*
 * MAIN (64-bit version).
 */
int main(int argc, char** argv)
{
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " <tracefile>\n";
        return 1;
    }

    ifstream infile(argv[1]);
    if (!infile.is_open()) {
        cerr << "Open file error: " << argv[1] << endl;
        return 1;
    }

    // parseTrace should store 64-bit addresses in Inst::addrn, etc.
    parseTrace(infile, instlist); // Changed to pass by reference
    infile.close();

    // Convert string operands -> structured 'Operand'
    parseOperand(instlist.begin(), instlist.end());

    // Build opcode map, fill in numeric opcodes, build jump set
    preprocess(instlist);

    // Simple optimization pass
    peephole(instlist, instenum);

    // Extract sequences of 7 push/pop
    vmextract(instlist);

    // Output them
    outputvm(ctxswh);

    // If you want more CFG building, you can do it here
    // e.g.,
    // CFG cfg(instlist);
    // cfg.checkConsist();
    // cfg.showCFG();
    // cfg.outputDot();

    return 0;
}
