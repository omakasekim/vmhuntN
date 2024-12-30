#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <map>
#include <stack>
#include <vector>
#include <set>
#include <algorithm>  // for std::find, std::next, etc.
#include <cstdint>    // for uint64_t

using namespace std;

/*
 * You will need to ensure that core.hpp and parser.hpp
 * define Inst and Operand structures with 64-bit addresses
 * and registers. For example:
 *
 *   struct Inst {
 *       int id;
 *       string addr;
 *       uint64_t addrn;    // 64-bit instruction address
 *       string assembly;
 *       int opc;
 *       string opcstr;
 *       vector<string> oprs;
 *       int oprnum;
 *       Operand *oprd[3];
 *       uint64_t ctxreg[8]; // 64-bit context registers
 *       uint64_t raddr;     // 64-bit read address
 *       uint64_t waddr;     // 64-bit write address
 *       vector<Parameter> src, dst, src2, dst2;
 *   };
 *
 *   struct Operand {
 *       enum Type { IMM, REG, MEM } ty;
 *       int tag;
 *       int bit;
 *       bool issegaddr;
 *       string segreg;
 *       string field[5];
 *   };
 *
 *   void parseTrace(ifstream *infile, list<Inst> *L);
 *   // etc.
 *
 * Make sure those headers are included before using this file.
 */
#include "core.hpp"
#include "parser.hpp"

/*
 * Global container of instructions read from the trace.
 */
list<Inst> instlist;

/*
 * Data structures for identifying functions.
 */
struct FuncBody {
    int start;
    int end;
    int length;
    uint64_t startAddr;
    uint64_t endAddr;
    int loopn;
};

struct Func {
    uint64_t callAddr;
    list<FuncBody *> body;
};

/*
 * Jump instruction names in x86/x86-64 assembly.
 */
string jmpInstrName[33] = {
    "jo","jno","js","jns","je","jz","jne","jnz",
    "jb","jnae","jc","jnb","jae","jnc","jbe","jna",
    "ja","jnbe","jl","jnge","jge","jnl","jle","jng",
    "jg","jnle","jp","jpe","jnp","jpo","jcxz","jecxz",
    "jmp"
};

/*
 * A set of opcodes corresponding to the above jump instructions
 * (populated in preprocess()).
 */
set<int> *jmpset = nullptr;

/*
 * A map of instruction mnemonics to integer IDs, built dynamically.
 */
map<string, int> *instenum = nullptr;

/*
 * Return the mnemonic (string) corresponding to a numeric opcode.
 */
string getOpcName(int opc, map<string, int> *m) {
    for (auto &kv : *m) {
        if (kv.second == opc) {
            return kv.first;
        }
    }
    return "unknown";
}

/*
 * Utility function that prints instructions in a debug-friendly format.
 */
void printInstlist(list<Inst> *L, map<string, int> *m) {
    for (auto &in : *L) {
        cout << in.id << " "
             << hex << in.addrn << " "
             << in.addr << " "
             << in.opcstr << " "
             << getOpcName(in.opc, m) << " "
             << dec << in.oprnum << endl;
        for (auto &op : in.oprs) {
            cout << op << endl;
        }
    }
}

/*
 * Build a simple map from call-site addresses to function bodies.
 * (An illustrative placeholder function.)
 */
map<uint64_t, list<FuncBody *> *> *buildFuncList(list<Inst> *L) {
    auto *funcmap = new map<uint64_t, list<FuncBody *> *>;
    stack<list<Inst>::iterator> stk;

    for (auto it = L->begin(); it != L->end(); ++it) {
        if (it->opcstr == "call") {
            stk.push(it);

            // Convert the call operand (hex) to a 64-bit address
            uint64_t calladdr = stoull(it->oprs[0], nullptr, 16);
            auto pos = funcmap->find(calladdr);
            if (pos == funcmap->end()) {
                funcmap->insert({calladdr, nullptr});
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
 * Print the keys (function addresses) of funcmap.
 */
void printFuncmap(map<uint64_t, list<FuncBody *> *> *funcmap) {
    for (auto &kv : *funcmap) {
        cout << hex << kv.first << endl;
    }
}

/*
 * Build a map from instruction mnemonic (string) to unique integer ID.
 */
map<string, int> *buildOpcodeMap(list<Inst> *L) {
    auto *instenum = new map<string, int>;
    for (auto &in : *L) {
        if (instenum->find(in.opcstr) == instenum->end()) {
            (*instenum)[in.opcstr] = instenum->size() + 1;
        }
    }
    return instenum;
}

/*
 * Return the integer ID for a given mnemonic. Return 0 if not found.
 */
int getOpc(const string &s, map<string, int> *m) {
    auto it = m->find(s);
    if (it != m->end()) {
        return it->second;
    }
    return 0;
}

/*
 * Determine if 'i' is in the jump opcode set 'jumpset'.
 */
bool isjump(int i, set<int> *jumpset) {
    return (jumpset->find(i) != jumpset->end());
}

/*
 * Count the number of indirect jumps (where the first operand is not IMM).
 */
void countindjumps(list<Inst> *L) {
    int indjumpnum = 0;
    for (auto &in : *L) {
        if (isjump(in.opc, jmpset) && in.oprd[0]->ty != Operand::IMM) {
            ++indjumpnum;
            cout << in.addr << "\t" << in.opcstr << " " << in.oprs[0] << endl;
        }
    }
    cout << "number of indirect jumps: " << indjumpnum << endl;
}

/*
 * Simple peephole optimization that removes pairs like push/pop or inc/dec
 * on the same register.
 */
void peephole(list<Inst> *L) {
    for (auto it = L->begin(); it != L->end();) {
        if (it == L->end() || next(it) == L->end()) {
            break; 
        }
        auto itNext = next(it);
        bool removePair = false;

        if ((it->opcstr == "pushad" && itNext->opcstr == "popad") ||
            (it->opcstr == "popad"  && itNext->opcstr == "pushad") ||
            (it->opcstr == "push"   && itNext->opcstr == "pop"   && it->oprs[0] == itNext->oprs[0]) ||
            (it->opcstr == "pop"    && itNext->opcstr == "push"  && it->oprs[0] == itNext->oprs[0]) ||
            (it->opcstr == "add"    && itNext->opcstr == "sub"   && it->oprs[0] == itNext->oprs[0] && it->oprs[1] == itNext->oprs[1]) ||
            (it->opcstr == "sub"    && itNext->opcstr == "add"   && it->oprs[0] == itNext->oprs[0] && it->oprs[1] == itNext->oprs[1]) ||
            (it->opcstr == "inc"    && itNext->opcstr == "dec"   && it->oprs[0] == itNext->oprs[0]) ||
            (it->opcstr == "dec"    && itNext->opcstr == "inc"   && it->oprs[0] == itNext->oprs[0])) 
        {
            removePair = true;
        }

        if (removePair) {
            it = L->erase(it);
            it = L->erase(it);
        } else {
            ++it;
        }
    }
}

/*
 * A struct holding info about a "context switch"-like code sequence.
 */
struct ctxswitch {
    list<Inst>::iterator begin;
    list<Inst>::iterator end;
    uint64_t sd;  // e.g. stack depth or some register context
};

/*
 * Global lists to keep track of these push/pop-based sequences.
 */
list<ctxswitch> ctxsave;
list<ctxswitch> ctxrestore;
list<pair<ctxswitch, ctxswitch>> ctxswh;

/*
 * Check if a string is one of the known 32-bit registers (extend as needed).
 * If you want to support r8-r15, add them here.
 */
bool isreg(const string &s) {
    static const set<string> regs = {
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"
        // For 64-bit: "rax", "rbx", "rcx", "rdx", etc.
    };
    return (regs.find(s) != regs.end());
}

/*
 * Check if all instructions from [i1..i2) are "push reg".
 */
bool chkpush(list<Inst>::iterator i1, list<Inst>::iterator i2) {
    int opcpush = getOpc("push", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpush || !isreg(it->oprs[0])) {
            return false;
        }
    }
    set<string> used;
    for (auto it = i1; it != i2; ++it) {
        if (!used.insert(it->oprs[0]).second) {
            return false; // repeated reg
        }
    }
    return true;
}

/*
 * Check if all instructions from [i1..i2) are "pop reg".
 */
bool chkpop(list<Inst>::iterator i1, list<Inst>::iterator i2) {
    int opcpop = getOpc("pop", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpop || !isreg(it->oprs[0])) {
            return false;
        }
    }
    set<string> used;
    for (auto it = i1; it != i2; ++it) {
        if (!used.insert(it->oprs[0]).second) {
            return false; // repeated reg
        }
    }
    return true;
}

/*
 * Find "VM-like" push/pop sequences and store them in global lists.
 */
void vmextract(list<Inst> *L) {
    for (auto it = L->begin(); it != L->end(); ++it) {
        // We'll check for a block of 7 push or 7 pop instructions in a row,
        // just as an example from your original code.
        // Adjust '7' to match your needs.
        if (distance(it, L->end()) >= 7 && chkpush(it, next(it, 7))) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = next(it, 7);
            cs.sd    = next(it, 7)->ctxreg[6];
            ctxsave.push_back(cs);

            cout << "push found" << endl;
            cout << it->id << " " << it->addr << " " << it->assembly << endl;
        }
        else if (distance(it, L->end()) >= 7 && chkpop(it, next(it, 7))) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = next(it, 7);
            cs.sd    = it->ctxreg[6];
            ctxrestore.push_back(cs);

            cout << it->id << " " << it->addr << " " << it->assembly << endl;
        }
    }

    // Pair up saves and restores using the same sd field.
    for (auto &save : ctxsave) {
        for (auto &restore : ctxrestore) {
            if (save.sd == restore.sd) {
                ctxswh.push_back({save, restore});
            }
        }
    }
}

/*
 * Write out matched "VM" sequences to separate vmN.txt files.
 */
void outputvm(list<pair<ctxswitch, ctxswitch>> *ctxswh) {
    int n = 1;
    for (auto &sw : *ctxswh) {
        auto i1 = sw.first.begin;
        auto i2 = sw.second.end;

        string vmfile = "vm" + to_string(n++) + ".txt";
        FILE *fp = fopen(vmfile.c_str(), "w");
        if (!fp) {
            cerr << "Failed to open " << vmfile << endl;
            continue;
        }
        for (auto it = i1; it != i2; ++it) {
            fprintf(fp, "%s;%s;", it->addr.c_str(), it->assembly.c_str());
            for (int j = 0; j < 8; ++j) {
                fprintf(fp, "%llx,", (unsigned long long)it->ctxreg[j]);
            }
            fprintf(fp, "%llx,%llx\n",
                    (unsigned long long)it->raddr,
                    (unsigned long long)it->waddr);
        }
        fclose(fp);
    }
}

/*
 * Check if a string looks like a hex address (starts with "0x").
 */
bool ishex(const string &s) {
    return (s.compare(0, 2, "0x") == 0);
}

/*
 * Basic Block and Edge structures for building a CFG. 
 */
struct Edge;

struct BB {
    vector<Inst> instvec;
    uint64_t beginaddr;
    uint64_t endaddr;
    vector<int> out;
    int ty;

    BB() : beginaddr(0), endaddr(0), ty(0) {}
    BB(uint64_t begin, uint64_t end) : beginaddr(begin), endaddr(end), ty(0) {}
    BB(uint64_t begin, uint64_t end, int type) : beginaddr(begin), endaddr(end), ty(type) {}
};

struct Edge {
    int from;
    int to;
    bool jumped;
    int ty;
    int count;
    uint64_t fromaddr;
    uint64_t toaddr;

    Edge() : from(0), to(0), jumped(false), ty(0), count(0), fromaddr(0), toaddr(0) {}
    Edge(uint64_t addr1, uint64_t addr2, int type, int num) {
        fromaddr = addr1;
        toaddr   = addr2;
        ty       = type;
        count    = num;
    }
};

/*
 * A CFG class that builds a control-flow graph from a list of Inst.
 */
class CFG {
    vector<BB> bbs;
    vector<Edge> edges;
    map<uint64_t, int> bbmap;

public:
    CFG() {}
    CFG(list<Inst> *L);
    void checkConsist();
    void showCFG();
    void outputDot();
    void outputSimpleDot();
    void showTrace(list<Inst> *L);
    void compressCFG();
};

/*
 * Constructor builds the CFG based on the instruction trace.
 */
CFG::CFG(list<Inst> *L) {
    if (L->empty()) return;
    auto it = L->begin();
    uint64_t addr1 = it->addrn;

    while (it != L->end()) {
        if (isjump(it->opc, jmpset) || it->opcstr == "ret" || it->opcstr == "call") {
            uint64_t addr2 = it->addrn;

            int curbb = 0;
            int maxbb = (int)bbs.size();
            for (; curbb < maxbb; ++curbb) {
                if (bbs[curbb].endaddr == addr2) {
                    break;
                }
            }
            int res = 0;
            if (curbb != maxbb) {
                if (bbs[curbb].beginaddr == addr1) {
                    res = 1;
                } else if (bbs[curbb].beginaddr < addr1) {
                    res = 2;
                } else {
                    res = 3;
                }
            } else {
                res = 4;
            }

            BB *tempBB;
            Edge *tempEdge;
            switch (res) {
            case 1:
                break;
            case 2:
                bbs[curbb].endaddr = addr1 - 1;
                tempBB = new BB(addr1, addr2, 2);
                bbs.push_back(*tempBB);
                tempEdge = new Edge(bbs[curbb].endaddr, addr1, 2, 1);
                edges.push_back(*tempEdge);
                break;
            case 3: {
                int i, maxi;
                for (i = 0, maxi = (int)bbs.size(); i < maxi; ++i) {
                    if (bbs[i].beginaddr == addr1) {
                        break;
                    }
                }
                if (i == maxi) {
                    tempBB = new BB(addr1, bbs[curbb].beginaddr - 1, 2);
                    bbs.push_back(*tempBB);
                }
                bool foundEdge = false;
                for (auto &e : edges) {
                    if (e.fromaddr == bbs[curbb].beginaddr - 1 &&
                        e.toaddr   == bbs[curbb].beginaddr) {
                        e.count++;
                        foundEdge = true;
                        break;
                    }
                }
                if (!foundEdge) {
                    tempEdge = new Edge(bbs[curbb].beginaddr - 1, bbs[curbb].beginaddr, 2, 1);
                    edges.push_back(*tempEdge);
                }
                break;
            }
            case 4:
                tempBB = new BB(addr1, addr2);
                bbs.push_back(*tempBB);
                break;
            }

            auto itNext = next(it);
            if (itNext != L->end()) {
                addr1 = itNext->addrn;
            }
        }
        ++it;
    }

    if (!L->empty()) {
        auto last = prev(L->end());
        if (!(isjump(last->opc, jmpset) || last->opcstr == "ret" || last->opcstr == "call")) {
            uint64_t lastaddr = last->addrn;
            BB *lastBB = new BB(addr1, lastaddr);
            bbs.push_back(*lastBB);
        }
    }

    auto nit = next(L->begin());
    for (auto it2 = L->begin(); nit != L->end(); ++it2, nit = next(it2)) {
        uint64_t curaddr = it2->addrn;
        uint64_t targetaddr = nit->addrn;
        int jumpty = 0;

        if (isjump(it2->opc, jmpset)) {
            string target = it2->oprs[0];
            jumpty = ishex(target) ? 2 : 1;
        }
        else if (it2->opcstr == "ret") {
            jumpty = 3;
        }
        else if (it2->opcstr == "call") {
            string target = it2->oprs[0];
            jumpty = ishex(target) ? 4 : 5;
        }
        else {
            continue;
        }

        bool edgeFound = false;
        for (auto &e : edges) {
            if (e.fromaddr == curaddr && e.toaddr == targetaddr) {
                e.count++;
                edgeFound = true;
                break;
            }
        }
        if (!edgeFound) {
            Edge newedge(curaddr, targetaddr, jumpty, 1);
            edges.push_back(newedge);
        }
    }

    for (auto &e : edges) {
        uint64_t addr2 = e.fromaddr;
        uint64_t addr3 = e.toaddr;
        int frombb = -1, tobb = -1;

        for (int j = 0; j < (int)bbs.size(); ++j) {
            if (bbs[j].endaddr == addr2) {
                frombb = j;
                break;
            }
        }
        if (frombb < 0) {
            printf("error: no bbs with endaddr == %llx\n", (unsigned long long)addr2);
        }
        for (int j = 0; j < (int)bbs.size(); ++j) {
            if (bbs[j].beginaddr == addr3) {
                tobb = j;
                break;
            }
        }
        if (tobb < 0) {
            printf("error: no bbs with beginaddr == %llx\n", (unsigned long long)addr3);
        }
        e.from = frombb;
        e.to   = tobb;
        if (frombb >= 0 && tobb >= 0) {
            auto &outs = bbs[frombb].out;
            if (find(outs.begin(), outs.end(), tobb) == outs.end()) {
                outs.push_back(tobb);
            }
        }
    }
}

/*
 * Print summary of basic blocks and edges to cfginfo.txt
 */
void CFG::showCFG() {
    FILE *fp = fopen("cfginfo.txt", "w");
    if (!fp) {
        cerr << "Failed to open cfginfo.txt" << endl;
        return;
    }
    int bbnum = (int)bbs.size();
    fprintf(fp, "Total BB number: %d\n", bbnum);

    int edgenum = (int)edges.size();
    fprintf(fp, "Total edge number: %d\n", edgenum);

    fprintf(fp, "BBs:\n");
    for (int i = 0; i < bbnum; i++) {
        int outnum = (int)bbs[i].out.size();
        fprintf(fp, "BB%d: %llx, %llx, %d\n",
                i,
                (unsigned long long)bbs[i].beginaddr,
                (unsigned long long)bbs[i].endaddr,
                outnum);
    }
    fprintf(fp, "\nEdges:\n");
    for (int i = 0; i < edgenum; i++) {
        fprintf(fp, "Edge%d: %d -> %d, %d, %llx, %llx, %d\n",
                i,
                edges[i].from,
                edges[i].to,
                edges[i].count,
                (unsigned long long)edges[i].fromaddr,
                (unsigned long long)edges[i].toaddr,
                edges[i].ty);
    }
    fclose(fp);
}

/*
 * Print the CFG in GraphViz .dot format, with labeled edges.
 */
void CFG::outputDot() {
    FILE *fp = fopen("cfg.dot", "w");
    if (!fp) {
        cerr << "Failed to open cfg.dot" << endl;
        return;
    }
    fprintf(fp, "digraph G {\n");
    for (auto &e : edges) {
        string lab;
        switch (e.ty) {
        case 1: lab = "i";  break;
        case 2: lab = "d";  break;
        case 3: lab = "r";  break;
        case 4: lab = "dc"; break;
        case 5: lab = "ic"; break;
        default:
            printf("unknown edge label: %d\n", e.ty);
            break;
        }
        fprintf(fp, "BB%d -> BB%d [label=\"%d,%s\"];\n",
                e.from, e.to, e.count, lab.c_str());
    }
    fprintf(fp, "}\n");
    fclose(fp);
}

/*
 * Print the CFG in GraphViz .dot format, but without edge labels.
 */
void CFG::outputSimpleDot() {
    FILE *fp = fopen("simplecfg.dot", "w");
    if (!fp) {
        cerr << "Failed to open simplecfg.dot" << endl;
        return;
    }
    fprintf(fp, "digraph G {\n");
    for (int i = 0; i < (int)bbs.size(); i++) {
        fprintf(fp, "BB%d [shape=record,label=\"\"];\n", i);
    }
    for (auto &e : edges) {
        fprintf(fp, "BB%d -> BB%d;\n", e.from, e.to);
    }
    fprintf(fp, "}\n");
    fclose(fp);
}

/*
 * Combine basic blocks that have exactly one predecessor and one successor.
 */
void CFG::compressCFG() {
    struct BBinfo {
        int id;
        int nfrom;
        int nto;
        BBinfo(int a, int b, int c) : id(a), nfrom(b), nto(c) {}
    };

    list<BBinfo> bblist;
    for (int i = 0; i < (int)bbs.size(); i++) {
        bblist.push_back(BBinfo(i, 0, 0));
    }
    for (auto &e : edges) {
        for (auto &bi : bblist) {
            if (bi.id == e.from) {
                bi.nto++;
            } else if (bi.id == e.to) {
                bi.nfrom++;
            }
        }
    }
    for (int i = 0; i < (int)edges.size(); i++) {
        auto &curEdge = edges[i];
        list<BBinfo>::iterator ifrom, ito;
        for (auto it = bblist.begin(); it != bblist.end(); ++it) {
            if (it->id == curEdge.from) {
                ifrom = it;
            }
            if (it->id == curEdge.to) {
                ito = it;
            }
        }
        int bb1 = ifrom->id;
        int bb2 = ito->id;
        if (ifrom->nto == 1 && ito->nfrom == 1) {
            edges.erase(edges.begin() + i);
            for (auto &e2 : edges) {
                if (e2.from == bb2) {
                    e2.from = bb1;
                }
            }
            ifrom->nto = ito->nto;
            bblist.erase(ito);
            i = -1;  // restart
        }
    }

    FILE *fp = fopen("compcfg.dot", "w");
    if (!fp) {
        cerr << "Failed to open compcfg.dot" << endl;
        return;
    }
    fprintf(fp, "digraph G {\n");
    for (auto &e : edges) {
        string lab;
        switch (e.ty) {
        case 1: lab = "i";  break;
        case 2: lab = "d";  break;
        case 3: lab = "r";  break;
        case 4: lab = "dc"; break;
        case 5: lab = "ic"; break;
        default:
            printf("unknown edge label: %d\n", e.ty);
            break;
        }
        fprintf(fp, "BB%d -> BB%d [label=\"%d,%s\"];\n",
                e.from, e.to, e.count, lab.c_str());
    }
    fprintf(fp, "}\n");
    fclose(fp);
}

/*
 * Check for overlapping or reversed basic blocks.
 */
void CFG::checkConsist() {
    int bbnum = (int)bbs.size();
    printf("Total BB number: %d\n", bbnum);

    int edgenum = (int)edges.size();
    printf("Total edge number: %d\n", edgenum);

    for (int i = 0; i < bbnum; i++) {
        uint64_t addr1 = bbs[i].beginaddr;
        uint64_t addr2 = bbs[i].endaddr;
        if (addr1 > addr2) {
            printf("bad bbs: 1, BB%d\n", i);
        }
        for (int j = i + 1; j < bbnum; j++) {
            uint64_t addr3 = bbs[j].beginaddr;
            uint64_t addr4 = bbs[j].endaddr;
            if ((addr3 <= addr4) && (addr2 < addr3 || addr4 < addr1)) {
                // good
            } else {
                printf("bad bbs: 2, BB%d and BB%d\n", i, j);
            }
        }
    }
}

/*
 * Show a trace of which BBs are visited, in order.
 */
void CFG::showTrace(list<Inst> *L) {
    FILE *fp = fopen("traceinfo.txt", "w");
    if (!fp) {
        cerr << "Failed to open traceinfo.txt" << endl;
        return;
    }
    for (auto &in : *L) {
        uint64_t addr = in.addrn;
        for (int i = 0; i < (int)bbs.size(); i++) {
            if (bbs[i].beginaddr == addr) {
                fprintf(fp, "%d -> ", i);
                break;
            }
        }
    }
    fprintf(fp, "end\n");
    fclose(fp);
}

/*
 * Build the opcode map, mark jump instructions, etc.
 */
void preprocess(list<Inst> *L) {
    instenum = buildOpcodeMap(L);

    for (auto &in : *L) {
        in.opc = getOpc(in.opcstr, instenum);
    }
    jmpset = new set<int>;
    for (auto &s : jmpInstrName) {
        int n = getOpc(s, instenum);
        if (n != 0) {
            jmpset->insert(n);
        }
    }
}

/*
 * MAIN function.
 * Usage: vmextract <tracefile>
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <tracefile>\n", argv[0]);
        return 1;
    }

    ifstream infile(argv[1]);
    if (!infile.is_open()) {
        fprintf(stderr, "Open file error!\n");
        return 1;
    }

    parseTrace(&infile, &instlist);
    infile.close();

    preprocess(&instlist);

    peephole(&instlist);

    vmextract(&instlist);
    outputvm(&ctxswh);

    // Optionally build a CFG or do other analyses:
    // CFG cfg(&instlist);
    // cfg.checkConsist();
    // cfg.showCFG();
    // cfg.outputDot();

    return 0;
}
