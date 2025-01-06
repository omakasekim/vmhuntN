#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <map>
#include <stack>
#include <vector>
#include <set>
#include <cstdint>   // for uint64_t
#include <algorithm> // for std::next, etc.

using namespace std;

#include "core.hpp"
#include "parser.hpp"

/*
 * Global list of instructions from the trace.
 */
list<Inst> instlist;

/*
 * Data structures for function identification.
 */
struct FuncBody {
    int start;
    int end;
    int length;           // end - start
    uint64_t startAddr;   // changed to 64-bit
    uint64_t endAddr;     // changed to 64-bit
    int loopn;
};

struct Func {
    uint64_t callAddr;    // changed to 64-bit
    list<FuncBody*> body;
};

/*
 * Jump instruction mnemonics.
 */
string jmpInstrName[33] = {
    "jo","jno","js","jns","je","jz","jne","jnz","jb","jnae","jc","jnb","jae",
    "jnc","jbe","jna","ja","jnbe","jl","jnge","jge","jnl","jle","jng","jg",
    "jnle","jp","jpe","jnp","jpo","jcxz","jecxz","jmp"
};

/*
 * Sets and maps for instruction processing.
 */
set<int> *jmpset;              // jump opcodes
map<string, int> *instenum;    // mnemonic -> opcode ID

/*
 * Return the string mnemonic for a given numeric opcode.
 */
string getOpcName(int opc, map<string, int> *m)
{
    for (auto it = m->begin(); it != m->end(); ++it) {
        if (it->second == opc) {
            return it->first;
        }
    }
    return "unknown";
}

/*
 * Print the instructions in a debug-friendly way.
 */
void printInstlist(list<Inst> *L, map<string, int> *m)
{
    for (auto it = L->begin(); it != L->end(); ++it) {
        cout << it->id << ' '
             << hex << it->addrn << ' '
             << it->addr << ' '
             << it->opcstr << ' '
             << getOpcName(it->opc, m) << ' '
             << dec << it->oprnum << endl;
        for (auto ii = it->oprs.begin(); ii != it->oprs.end(); ++ii) {
            cout << *ii << endl;
        }
    }
}

/*
 * Build a map of (function address -> list of FuncBody*).
 * This is a placeholder for a function-identification mechanism.
 */
map<uint64_t, list<FuncBody *> *> *buildFuncList(list<Inst> *L)
{
    auto *funcmap = new map<uint64_t, list<FuncBody *> *>;
    stack<list<Inst>::iterator> stk;

    for (auto it = L->begin(); it != L->end(); ++it) {
        if (it->opcstr == "call") {
            stk.push(it);
            auto pos = funcmap->find(it->addrn);
            if (pos == funcmap->end()) {
                // parse call operand (hex) as 64-bit
                uint64_t calladdr = stoull(it->oprs[0], nullptr, 16);
                funcmap->insert({calladdr, nullptr});
            }
        } else if (it->opcstr == "ret") {
            if (!stk.empty()) {
                stk.pop();
            }
        }
    }
    return funcmap;
}

/*
 * Print the keys in the function map.
 */
void printFuncmap(map<uint64_t, list<FuncBody *> *> *funcmap)
{
    for (auto it = funcmap->begin(); it != funcmap->end(); ++it) {
        cout << hex << it->first << endl;
    }
}

/*
 * Build a map from instruction mnemonic to a unique integer ID.
 */
map<string, int> *buildOpcodeMap(list<Inst> *L)
{
    auto *instenum = new map<string, int>;
    for (auto it = L->begin(); it != L->end(); ++it) {
        if (instenum->find(it->opcstr) == instenum->end()) {
            (*instenum)[it->opcstr] = instenum->size() + 1;
        }
    }
    return instenum;
}

/*
 * Lookup the numeric opcode of a mnemonic. Return 0 if not found.
 */
int getOpc(string s, map<string, int> *m)
{
    auto it = m->find(s);
    if (it != m->end()) {
        return it->second;
    } else {
        return 0;
    }
}

/*
 * Check if 'i' is in the jump opcode set.
 */
bool isjump(int i, set<int> *jumpset)
{
    auto it = jumpset->find(i);
    if (it == jumpset->end()) {
        return false;
    } else {
        return true;
    }
}

/*
 * Count indirect jumps by checking if operand[0] is not IMM.
 */
void countindjumps(list<Inst> *L)
{
    int indjumpnum = 0;
    for (auto it = L->begin(); it != L->end(); ++it) {
        if (isjump(it->opc, jmpset) && it->oprd[0]->ty != Operand::IMM) {
            ++indjumpnum;
            cout << it->addr << "\t" << it->opcstr << " " << it->oprs[0] << endl;
        }
    }
    cout << "number of indirect jumps: " << indjumpnum << endl;
}

/*
 * Simple peephole optimizations that remove canceling pairs (push/pop, add/sub, etc.)
 */
void peephole(list<Inst> *L)
{
    for (auto it = L->begin(); it != L->end(); ++it) {
        // ensure there's a next instruction
        if (std::next(it) == L->end()) {
            break;
        }
        // check pairs
        if ((it->opcstr == "pushad" && std::next(it)->opcstr == "popad") ||
            (it->opcstr == "popad" && std::next(it)->opcstr == "pushad") ||
            (it->opcstr == "push"  && std::next(it)->opcstr == "pop"  && it->oprs[0] == std::next(it)->oprs[0]) ||
            (it->opcstr == "pop"   && std::next(it)->opcstr == "push" && it->oprs[0] == std::next(it)->oprs[0]) ||
            (it->opcstr == "add"   && std::next(it)->opcstr == "sub"  && it->oprs[0] == std::next(it)->oprs[0] && it->oprs[1] == std::next(it)->oprs[1]) ||
            (it->opcstr == "sub"   && std::next(it)->opcstr == "add"  && it->oprs[0] == std::next(it)->oprs[0] && it->oprs[1] == std::next(it)->oprs[1]) ||
            (it->opcstr == "inc"   && std::next(it)->opcstr == "dec"  && it->oprs[0] == std::next(it)->oprs[0]) ||
            (it->opcstr == "dec"   && std::next(it)->opcstr == "inc"  && it->oprs[0] == std::next(it)->oprs[0]))
        {
            it = L->erase(it);
            it = L->erase(it);
            if (it == L->begin()) {
                // if we erased from the start, break out or reset
                it = L->begin();
            } else {
                // step back one to recheck
                --it;
            }
        }
    }
}

/*
 * Struct capturing a range of instructions that save or restore context.
 */
struct ctxswitch {
    list<Inst>::iterator begin;
    list<Inst>::iterator end;
    uint64_t sd; // changed to 64-bit (stack depth or similar)
};

/*
 * Global lists storing context saves/restores and their pairs.
 */
list<ctxswitch> ctxsave;
list<ctxswitch> ctxrestore;
list<pair<ctxswitch, ctxswitch>> ctxswh;

/*
 * Check if a string is one of our recognized registers.
 * Extend for 64-bit if desired (e.g., rax, rbx, etc.).
 */
bool isreg(string s)
{
    if (s == "eax" || s == "ebx" || s == "ecx" || s == "edx" ||
        s == "esi" || s == "edi" || s == "ebp") {
        return true;
    } else {
        return false;
    }
}

/*
 * Check if all instructions in [i1..i2) are push <reg>.
 */
bool chkpush(list<Inst>::iterator i1, list<Inst>::iterator i2)
{
    int opcpush = getOpc("push", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpush || !isreg(it->oprs[0])) {
            return false;
        }
    }
    set<string> opcs;
    for (auto it = i1; it != i2; ++it) {
        if (!opcs.insert(it->oprs[0]).second) {
            return false; // repeated register
        }
    }
    return true;
}

/*
 * Check if all instructions in [i1..i2) are pop <reg>.
 */
bool chkpop(list<Inst>::iterator i1, list<Inst>::iterator i2)
{
    int opcpop = getOpc("pop", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpop || !isreg(it->oprs[0])) {
            return false;
        }
    }
    set<string> opcs;
    for (auto it = i1; it != i2; ++it) {
        if (!opcs.insert(it->oprs[0]).second) {
            return false; // repeated register
        }
    }
    return true;
}

/*
 * Search the instruction list and extract sequences that look like VM contexts.
 */
void vmextract(list<Inst> *L)
{
    for (auto it = L->begin(); it != L->end(); ++it) {
        // check for 7 pushes in a row
        if (distance(it, L->end()) >= 7 && chkpush(it, std::next(it, 7))) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = std::next(it, 7);
            // context register #6 might be "esp" or "rsp" in your data
            cs.sd    = std::next(it, 7)->ctxreg[6];
            ctxsave.push_back(cs);
            cout << "push found" << endl;
            cout << it->id << " " << it->addr << " " << it->assembly << endl;
        }
        // check for 7 pops in a row
        else if (distance(it, L->end()) >= 7 && chkpop(it, std::next(it, 7))) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = std::next(it, 7);
            cs.sd    = it->ctxreg[6];
            ctxrestore.push_back(cs);
            cout << it->id << " " << it->addr << " " << it->assembly << endl;
        }
    }

    // pair up the saves and restores by matching cs.sd
    for (auto i = ctxsave.begin(); i != ctxsave.end(); ++i) {
        for (auto ii = ctxrestore.begin(); ii != ctxrestore.end(); ++ii) {
            if (i->sd == ii->sd) {
                ctxswh.push_back({*i, *ii});
            }
        }
    }
}

/*
 * Output the extracted "VM" snippets to separate files.
 */
void outputvm(list<pair<ctxswitch, ctxswitch>> *ctxswh)
{
    int n = 1;
    for (auto i = ctxswh->begin(); i != ctxswh->end(); ++i) {
        auto i1 = i->first.begin;
        auto i2 = i->second.end;
        string vmfile = "vm" + to_string(n++) + ".txt";
        FILE *fp = fopen(vmfile.c_str(), "w");
        if (!fp) {
            cerr << "Failed to open " << vmfile << endl;
            continue;
        }
        for (auto ii = i1; ii != i2; ++ii) {
            fprintf(fp, "%s;%s;", ii->addr.c_str(), ii->assembly.c_str());
            for (int j = 0; j < 8; ++j) {
                fprintf(fp, "%llx,", (unsigned long long)ii->ctxreg[j]);
            }
            fprintf(fp, "%llx,%llx\n",
                    (unsigned long long)ii->raddr,
                    (unsigned long long)ii->waddr);
        }
        fclose(fp);
    }
}

/*
 * Check if a string looks like a hex number ("0x...").
 */
bool ishex(string &s)
{
    if (s.compare(0, 2, "0x") == 0) {
        return true;
    } else {
        return false;
    }
}

/*
 * Structures and code for building a basic-block CFG.
 */
struct Edge;

struct BB {
    vector<Inst> instvec;
    uint64_t beginaddr;  // replaced ADDR32
    uint64_t endaddr;    // replaced ADDR32
    vector<int> out;
    int ty; // 1: end with jump; 2: no jump

    BB() : beginaddr(0), endaddr(0), ty(0) {}
    BB(uint64_t begin, uint64_t end) : beginaddr(begin), endaddr(end), ty(0) {}
    BB(uint64_t begin, uint64_t end, int type) : beginaddr(begin), endaddr(end), ty(type) {}
};

struct Edge {
    int from;
    int to;
    bool jumped;
    int ty;        // 1: indirect jump, 2: direct jump, 3: return, etc.
    int count;
    uint64_t fromaddr; // replaced ADDR32
    uint64_t toaddr;   // replaced ADDR32

    Edge() : from(0), to(0), jumped(false), ty(0), count(0), fromaddr(0), toaddr(0) {}
    Edge(uint64_t addr1, uint64_t addr2, int type, int num)
    {
        fromaddr = addr1;
        toaddr   = addr2;
        ty       = type;
        count    = num;
    }
};

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
 * Build a CFG from the instruction trace.
 * Identify basic blocks by detecting jumps, calls, or rets.
 */
CFG::CFG(list<Inst> *L)
{
    if (L->empty()) {
        return;
    }
    auto it = L->begin();
    uint64_t addr1 = it->addrn;

    while (it != L->end()) {
        if (isjump(it->opc, jmpset) || it->opcstr == "ret" || it->opcstr == "call") {
            uint64_t addr2 = it->addrn;

            int curbb = 0;
            int maxbb = (int)bbs.size();
            int res   = 0;

            for (; curbb < maxbb; ++curbb) {
                if (bbs[curbb].endaddr == addr2) {
                    break;
                }
            }

            if (curbb != maxbb) {
                if (bbs[curbb].beginaddr == addr1) {
                    res = 1; // exact match
                } else if (bbs[curbb].beginaddr < addr1) {
                    res = 2; // subset
                } else {
                    res = 3;
                }
            } else {
                res = 4; // new BB
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
                    if (e.fromaddr == (bbs[curbb].beginaddr - 1) && e.toaddr == bbs[curbb].beginaddr) {
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

            auto itNext = std::next(it);
            if (itNext != L->end()) {
                addr1 = itNext->addrn;
            }
        }
        ++it;
    }

    // handle last BB if the last instruction is not jump/ret/call
    if (!L->empty()) {
        auto last = std::prev(L->end());
        if (!(isjump(last->opc, jmpset) || last->opcstr == "ret" || last->opcstr == "call")) {
            uint64_t lastaddr = last->addrn;
            BB *lastBB = new BB(addr1, lastaddr);
            bbs.push_back(*lastBB);
        }
    }

    // Add edges, ignoring the final instruction
    auto nit = std::next(L->begin());
    for (auto it2 = L->begin(); nit != L->end(); ++it2, nit = std::next(it2)) {
        uint64_t curaddr    = it2->addrn;
        uint64_t targetaddr = nit->addrn;
        int jumpty = 0;

        if (isjump(it2->opc, jmpset)) {
            string target = it2->oprs[0];
            jumpty = ishex(target) ? 2 : 1;
        } else if (it2->opcstr == "ret") {
            jumpty = 3;
        } else if (it2->opcstr == "call") {
            string target = it2->oprs[0];
            jumpty = ishex(target) ? 4 : 5;
        } else {
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

    // fill in 'from' / 'to' fields in edges
    for (int i = 0; i < (int)edges.size(); i++) {
        uint64_t a1 = edges[i].fromaddr;
        uint64_t a2 = edges[i].toaddr;
        int frombb = -1;
        int tobb   = -1;

        for (int j = 0; j < (int)bbs.size(); j++) {
            if (bbs[j].endaddr == a1) {
                frombb = j;
                break;
            }
        }
        if (frombb < 0) {
            printf("error: no endaddr == %llx\n", (unsigned long long)a1);
        }
        for (int j = 0; j < (int)bbs.size(); j++) {
            if (bbs[j].beginaddr == a2) {
                tobb = j;
                break;
            }
        }
        if (tobb < 0) {
            printf("error: no beginaddr == %llx\n", (unsigned long long)a2);
        }

        edges[i].from = frombb;
        edges[i].to   = tobb;

        if (frombb >= 0 && tobb >= 0) {
            auto &outs = bbs[frombb].out;
            if (std::find(outs.begin(), outs.end(), tobb) == outs.end()) {
                outs.push_back(tobb);
            }
        }
    }
}

/*
 * Print a summary of the CFG to cfginfo.txt
 */
void CFG::showCFG()
{
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
 * Print the CFG in GraphViz DOT format (with labels).
 */
void CFG::outputDot()
{
    FILE *fp = fopen("cfg.dot", "w");
    if (!fp) {
        cerr << "Failed to open cfg.dot" << endl;
        return;
    }
    fprintf(fp, "digraph G {\n");
    for (int i = 0; i < (int)edges.size(); i++) {
        string lab;
        switch (edges[i].ty) {
        case 1: lab = "i";  break;
        case 2: lab = "d";  break;
        case 3: lab = "r";  break;
        case 4: lab = "dc"; break;
        case 5: lab = "ic"; break;
        default:
            printf("unknown edge label: %d\n", edges[i].ty);
            break;
        }
        fprintf(fp, "BB%d -> BB%d [label=\"%d,%s\"];\n",
                edges[i].from, edges[i].to, edges[i].count, lab.c_str());
    }
    fprintf(fp, "}\n");
    fclose(fp);
}

/*
 * Print a simpler DOT (no labels).
 */
void CFG::outputSimpleDot()
{
    FILE *fp = fopen("simplecfg.dot", "w");
    if (!fp) {
        cerr << "Failed to open simplecfg.dot" << endl;
        return;
    }
    fprintf(fp, "digraph G {\n");
    for (int i = 0; i < (int)bbs.size(); i++) {
        fprintf(fp, "BB%d [shape=record,label=\"\"];\n", i);
    }
    for (int i = 0; i < (int)edges.size(); i++) {
        fprintf(fp, "BB%d -> BB%d;\n", edges[i].from, edges[i].to);
    }
    fprintf(fp, "}\n");
    fclose(fp);
}

/*
 * Combine basic blocks that have exactly one predecessor and one successor.
 */
void CFG::compressCFG()
{
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
            if (it->id == curEdge.from) ifrom = it;
            if (it->id == curEdge.to)   ito   = it;
        }
        int bb1 = ifrom->id;
        int bb2 = ito->id;
        if (ifrom->nto == 1 && ito->nfrom == 1) {
            edges.erase(edges.begin() + i);
            for (int j = 0; j < (int)edges.size(); j++) {
                if (edges[j].from == bb2) {
                    edges[j].from = bb1;
                }
            }
            ifrom->nto = ito->nto;
            bblist.erase(ito);
            i = -1; // reset
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
 * Verify that all basic blocks are well-defined (no overlaps, etc.).
 */
void CFG::checkConsist()
{
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
            if (addr3 <= addr4 && (addr2 < addr3 || addr4 < addr1)) {
                // good
            } else {
                printf("bad bbs: 2, BB%d and BB%d\n", i, j);
            }
        }
    }
}

/*
 * Show a trace of which basic blocks are visited in the order of instructions.
 */
void CFG::showTrace(list<Inst> *L)
{
    FILE *fp = fopen("traceinfo.txt", "w");
    if (!fp) {
        cerr << "Failed to open traceinfo.txt" << endl;
        return;
    }
    for (auto it = L->begin(); it != L->end(); ++it) {
        uint64_t addr = it->addrn;
        int i, max;
        for (i = 0, max = (int)bbs.size(); i < max; ++i) {
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
 * Build opcode map, fill in numeric opcodes in Inst, and populate jump set.
 */
void preprocess(list<Inst> *L)
{
    instenum = buildOpcodeMap(L);
    for (auto it = L->begin(); it != L->end(); ++it) {
        it->opc = getOpc(it->opcstr, instenum);
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
 * MAIN function for vmextract, 64-bit version.
 */
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <tracefile>\n", argv[0]);
        return 1;
    }

    ifstream infile(argv[1]);
    if (!infile.is_open()) {
        fprintf(stderr, "Open file error!\n");
        return 1;
    }

    // parseTrace should store 64-bit addresses in Inst::addrn, etc.
    parseTrace(&infile, &instlist);
    infile.close();

    preprocess(&instlist);

    peephole(&instlist);

    vmextract(&instlist);
    outputvm(&ctxswh);

    // Optionally build a CFG, etc.
    // CFG cfg(&instlist);
    // cfg.checkConsist();
    // cfg.showCFG();
    // cfg.outputDot();

    return 0;
}
