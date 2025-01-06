#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <bitset>
#include <sstream>
#include <cstdint>      // for uint64_t
#include <cstdio>       // for printf, FILE, etc.
#include <cstdlib>      // for size_t, stoull

using namespace std;

// Example: define 64-bit address type
typedef uint64_t ADDR64;

// For printing addresses in hex with printf, you might do:
// #include <inttypes.h>
// printf("Address: 0x%" PRIx64 "\n", (uint64_t)addr);

// Forward declarations as before
struct Operation;
struct Value;

// Enums remain the same
enum ValueTy {SYMBOL, CONCRETE, HYBRID, UNKNOWN};
enum OperTy {ADD, MOV, SHL, XOR, SHR};
typedef pair<int,int> BitRange;

// A symbolic or concrete value in a formula
struct Value {
    int id;                       // a unique id for each value
    ValueTy valty;
    Operation *opr;
    string conval;                // concrete value (string form)
    bitset<64> bsconval;          // 64-bit concrete value stored as bitset
    BitRange brange;              // range of bits in the 64-bit
    map<BitRange, Value*> childs; // child values in a hybrid value
    int len;                      // length of the value in bits (up to 64)

    static int idseed;

    Value(ValueTy vty);
    Value(ValueTy vty, int l);
    Value(ValueTy vty, string con); // constructor for concrete value
    Value(ValueTy vty, string con, int l);
    Value(ValueTy vty, bitset<64> bs);
    Value(ValueTy vty, Operation *oper);
    Value(ValueTy vty, Operation *oper, int l);

    bool isSymbol();
    bool isConcrete();
    bool isHybrid();
};

int Value::idseed = 0;

Value::Value(ValueTy vty) : opr(NULL)
{
    id = ++idseed;
    valty = vty;
    len = 64;
}

Value::Value(ValueTy vty, int l) : opr(NULL)
{
    id = ++idseed;
    valty = vty;
    len = l;
}

Value::Value(ValueTy vty, string con) : opr(NULL)
{
    id = ++idseed;
    valty = vty;
    // Convert the string hex to a 64-bit integer, store in bsconval
    uint64_t tmp = 0;
    if (!con.empty()) {
        // handle possible “0x” prefix
        tmp = stoull(con, 0, 16);
    }
    bsconval = bitset<64>(tmp);

    conval = con;
    brange.first = 0;
    brange.second = 63;
    len = 64;
}

Value::Value(ValueTy vty, string con, int l) : opr(NULL)
{
    id = ++idseed;
    valty = vty;
    conval = con;
    len = l;
}

Value::Value(ValueTy vty, bitset<64> bs) : opr(NULL)
{
    id = ++idseed;
    valty = vty;
    bsconval = bs;
    len = 64;
}

Value::Value(ValueTy vty, Operation *oper)
{
    id = ++idseed;
    valty = vty;
    opr = oper;
    len = 64;
}

Value::Value(ValueTy vty, Operation *oper, int l)
{
    id = ++idseed;
    valty = vty;
    opr = oper;
    len = l;
}

bool Value::isSymbol()
{
    return (this->valty == SYMBOL);
}

bool Value::isConcrete()
{
    return (this->valty == CONCRETE);
}

bool Value::isHybrid()
{
    return (this->valty == HYBRID);
}

string getValueName(Value *v)
{
    if (v->valty == SYMBOL)
        return "sym" + to_string(v->id);
    else
        return v->conval;
}

// An operation taking several values to calculate a result value
struct Operation {
    string opty;
    Value *val[3];

    Operation(string opt, Value *v1);
    Operation(string opt, Value *v1, Value *v2);
    Operation(string opt, Value *v1, Value *v2, Value *v3);
};

Operation::Operation(string opt, Value *v1)
{
    opty = opt;
    val[0] = v1;
    val[1] = NULL;
    val[2] = NULL;
}

Operation::Operation(string opt, Value *v1, Value *v2)
{
    opty = opt;
    val[0] = v1;
    val[1] = v2;
    val[2] = NULL;
}

Operation::Operation(string opt, Value *v1, Value *v2, Value *v3)
{
    opty = opt;
    val[0] = v1;
    val[1] = v2;
    val[2] = v3;
}

Value *buildop1(string opty, Value *v1)
{
    Operation *oper = new Operation(opty, v1);
    Value *result;

    if (v1->isSymbol())
        result = new Value(SYMBOL, oper);
    else
        result = new Value(CONCRETE, oper);

    return result;
}

Value *buildop2(string opty, Value *v1, Value *v2)
{
    Operation *oper = new Operation(opty, v1, v2);
    Value *result;

    if (v1->isSymbol() || v2->isSymbol())
        result = new Value(SYMBOL, oper);
    else
        result = new Value(CONCRETE, oper);

    return result;
}

// Currently there is no 3-operand operation in this code,
// it is reserved for future expansion.
Value *buildop3(string opty, Value *v1, Value *v2, Value *v3)
{
    Operation *oper = new Operation(opty, v1, v2, v3);
    Value *result;

    if (v1->isSymbol() || v2->isSymbol() || v3->isSymbol())
        result = new Value(SYMBOL, oper);
    else
        result = new Value(CONCRETE, oper);

    return result;
}

//**********************************************************
//   Symbolic Execution Engine (64-bit version)
//**********************************************************

// Example 64-bit address range
struct AddrRange {
    ADDR64 first;
    ADDR64 second;

    AddrRange() : first(0), second(0) {}
    AddrRange(ADDR64 f, ADDR64 s) : first(f), second(s) {}

    // comparison for map key
    bool operator<(const AddrRange &o) const {
        if (first < o.first) return true;
        if (first == o.first) return (second < o.second);
        return false;
    }
};

struct Operand {
    // This is just a placeholder; the real definition depends on your code
    // Keep or adjust fields as needed
    int tag;
    int ty;
    int bit;           // how many bits in the operand
    vector<string> field;
    // ...
};

struct Inst {
    // Another placeholder
    int id;
    string opcstr;
    int oprnum;
    Operand* oprd[3];
    ADDR64 addrn;
    // For 64-bit registers, we might store them in ctxreg[0..7] for rax..rbp
    uint64_t ctxreg[8];

    // read addresses, write addresses
    ADDR64 raddr;
    ADDR64 waddr;
    // ...
};

// We also have a FullMap type used in outputBitCVC
typedef map<int,int> FullMapPair;
typedef pair<FullMapPair,FullMapPair> FullMap;

class SEEngine {
private:
    map<string, Value*> ctx;      // 64-bit register context
    list<Inst>::iterator start;
    list<Inst>::iterator end;
    list<Inst>::iterator ip;

    map<AddrRange, Value*> mem;   // memory model
    map<Value*, AddrRange> meminput;   // inputs from memory
    map<Value*, string> reginput; // inputs from registers

    bool memfind(AddrRange ar) {
        auto ii = mem.find(ar);
        return (ii != mem.end());
    }

    bool memfind(ADDR64 b, ADDR64 e) {
        AddrRange ar(b, e);
        auto ii = mem.find(ar);
        return (ii != mem.end());
    }

    bool isnew(AddrRange ar);
    bool issubset(AddrRange ar, AddrRange *superset);
    bool issuperset(AddrRange ar, AddrRange *subset);

    Value* readReg(string &s);
    void writeReg(string &s, Value *v);

    Value* readMem(ADDR64 addr, int nbyte);
    void writeMem(ADDR64 addr, int nbyte, Value *v);

    ADDR64 getRegConVal(string reg);
    ADDR64 calcAddr(Operand *opr);
    void printformula(Value* v);

public:
    // Initialize register context to null
    SEEngine() {
        ctx = {
            {"rax", nullptr},
            {"rbx", nullptr},
            {"rcx", nullptr},
            {"rdx", nullptr},
            {"rsi", nullptr},
            {"rdi", nullptr},
            {"rsp", nullptr},
            {"rbp", nullptr}
        };
    }

    void init(Value *v1, Value *v2, Value *v3, Value *v4,
              Value *v5, Value *v6, Value *v7, Value *v8,
              list<Inst>::iterator it1,
              list<Inst>::iterator it2);

    void init(list<Inst>::iterator it1,
              list<Inst>::iterator it2);

    void initAllRegSymol(list<Inst>::iterator it1,
                         list<Inst>::iterator it2);

    int symexec();
    ADDR64 conexec(Value *f, map<Value*, ADDR64> *input);
    void outputFormula(string reg);
    void dumpreg(string reg);
    void printAllRegFormulas();
    void printAllMemFormulas();
    void printInputSymbols(string output);
    Value *getValue(string s) { return ctx[s]; }
    vector<Value*> getAllOutput();
    void showMemInput();
    void printMemFormula(ADDR64 addr1, ADDR64 addr2);
};

//----------------------------------------------
//  Implementation details of SEEngine (64-bit)
//----------------------------------------------
bool hasVal(Value *v, int start, int end)
{
    BitRange br(start,end);
    auto i = v->childs.find(br);
    return (i != v->childs.end());
}

Value *readVal(Value *v, int start, int end)
{
    BitRange br(start,end);
    auto i = v->childs.find(br);
    if (i == v->childs.end())
        return NULL;
    else
        return i->second;
}

// Convert bits [start..end] in bs to a hex string
string bs2str(bitset<64> bs, BitRange br){
    int st = br.first, ed = br.second;
    uint64_t ui = 0;
    uint64_t step = 1;
    for (int i = st; i <= ed; ++i, step <<= 1) {
        if (bs[i]) {
            ui += step;
        }
    }
    stringstream strs;
    strs << "0x" << hex << ui;
    return strs.str();
}

Value *writeVal(Value *from, Value *to, int start, int end)
{
    Value *res = new Value(HYBRID);
    BitRange brfrom(start, end);
    if (to->isHybrid()) {
        auto i = to->childs.find(brfrom);
        if (i != to->childs.end()) {
            i->second = from;
            return to;
        } else {
            cerr << "writeVal: no child in 'to' matches the from-range!" << endl;
            return NULL;
        }
    } else if (from->isSymbol() && to->isConcrete()) {
        int s1 = to->brange.first;
        int e1 = to->brange.second;
        Value *v1 = new Value(CONCRETE, to->bsconval);
        Value *v2 = new Value(CONCRETE, to->bsconval);

        v1->brange.first = s1;
        v1->brange.second = start-1;
        v1->conval = bs2str(v1->bsconval, v1->brange);

        v2->brange.first = end+1;
        v2->brange.second = e1;
        v2->conval = bs2str(v2->bsconval, v2->brange);

        res->childs.insert(pair<BitRange, Value*>(v1->brange, v1));
        res->childs.insert(pair<BitRange, Value*>(brfrom, from));
        res->childs.insert(pair<BitRange, Value*>(v2->brange, v2));

        return res;
    } else {
        cerr << "writeVal: unhandled case!" << endl;
        return NULL;
    }
}

//-------------------------------------------------
//    64-bit version: Get the concrete value
//-------------------------------------------------
ADDR64 SEEngine::getRegConVal(string reg)
{
    // For demonstration, we assume ip->ctxreg[0..7] are rax..rbp
    if (reg == "rax")
        return ip->ctxreg[0];
    else if (reg == "rbx")
        return ip->ctxreg[1];
    else if (reg == "rcx")
        return ip->ctxreg[2];
    else if (reg == "rdx")
        return ip->ctxreg[3];
    else if (reg == "rsi")
        return ip->ctxreg[4];
    else if (reg == "rdi")
        return ip->ctxreg[5];
    else if (reg == "rsp")
        return ip->ctxreg[6];
    else if (reg == "rbp")
        return ip->ctxreg[7];
    else {
        cerr << "Now only get 64-bit register's concrete value for [rax..rbp]." << endl;
        return 0;
    }
}

//-------------------------------------------------
//  64-bit version: Calculate address
//-------------------------------------------------
ADDR64 SEEngine::calcAddr(Operand *opr)
{
    ADDR64 r1, r2, c;
    int64_t n;
    switch (opr->tag)
    {
    case 7: // addr7 = r1 + r2*n + c
        r1 = getRegConVal(opr->field[0]);
        r2 = getRegConVal(opr->field[1]);
        n  = stoll(opr->field[2]);
        c  = stoull(opr->field[4], 0, 16);
        if (opr->field[3] == "+")
            return r1 + (r2 * n) + c;
        else if (opr->field[3] == "-")
            return r1 + (r2 * n) - c;
        else {
            cerr << "unrecognized addr: tag 7" << endl;
            return 0;
        }
    case 4: // addr4 = r1 + c
        r1 = getRegConVal(opr->field[0]);
        c = stoull(opr->field[2], 0, 16);
        if (opr->field[1] == "+")
            return r1 + c;
        else if (opr->field[1] == "-")
            return r1 - c;
        else {
            cerr << "unrecognized addr: tag 4" << endl;
            return 0;
        }
    case 5: // addr5 = r1 + r2*n
        r1 = getRegConVal(opr->field[0]);
        r2 = getRegConVal(opr->field[1]);
        n  = stoll(opr->field[2]);
        return r1 + r2*n;
    case 6: // addr6 = r2*n + c
        r2 = getRegConVal(opr->field[0]);
        n = stoll(opr->field[1]);
        c = stoull(opr->field[3], 0, 16);
        if (opr->field[2] == "+")
            return r2*n + c;
        else if (opr->field[2] == "-")
            return r2*n - c;
        else {
            cerr << "unrecognized addr: tag 6" << endl;
            return 0;
        }
    case 3: // addr3 = r2*n
        r2 = getRegConVal(opr->field[0]);
        n = stoll(opr->field[1]);
        return r2 * n;
    case 1: // addr1 = c
        c = stoull(opr->field[0], 0, 16);
        return c;
    case 2: // addr2 = r1
        r1 = getRegConVal(opr->field[0]);
        return r1;
    default:
        cerr << "unrecognized addr tag" << endl;
        return 0;
    }
}

//-----------------------------------------------------------------------
//  64-bit readReg: read a symbolic/concrete Value from register context
//-----------------------------------------------------------------------
Value* SEEngine::readReg(string &s)
{
    Value *res;

    // 64-bit full register
    if (s == "rax" || s == "rbx" || s == "rcx" || s == "rdx" ||
        s == "rsi" || s == "rdi" || s == "rsp" || s == "rbp") {
        return ctx[s];
    }
    // 32-bit sub-register, e.g. "eax" => we take the lower 32 bits of "rax"
    else if (s == "eax" || s == "ebx" || s == "ecx" || s == "edx") {
        string rname = "r" + s.substr(1); // e.g. "eax" -> "rax"
        if (hasVal(ctx[rname], 0, 31)) {
            res = readVal(ctx[rname], 0, 31);
            return res;
        } else {
            // build a mask of 0x00000000ffffffff
            Value *v0 = ctx[rname];
            Value *v1 = new Value(CONCRETE, "0x00000000ffffffff");
            res = buildop2("and", v0, v1);
            return res;
        }
    }
    // 16-bit sub-registers: "ax", "bx", "cx", "dx", "si", "di", "bp", "sp"
    else if (s == "ax" || s == "bx" || s == "cx" || s == "dx" ||
             s == "si" || s == "di" || s == "bp" || s == "sp")
    {
        // For example, "ax" => "rax", then bits [0..15]
        // If s=="ax", then rname="rax". If s=="si", then rname="rsi"
        string rname = "r" + s;
        if (hasVal(ctx[rname], 0, 15)) {
            return readVal(ctx[rname], 0, 15);
        } else {
            Value *v0 = ctx[rname];
            Value *v1 = new Value(CONCRETE, "0x000000000000ffff");
            res = buildop2("and", v0, v1);
            return res;
        }
    }
    // 8-bit low registers: "al", "bl", "cl", "dl", "sil", "dil", "bpl", "spl"
    else if (s == "al" || s == "bl" || s == "cl" || s == "dl" ||
             s == "sil" || s == "dil" || s == "bpl" || s == "spl")
    {
        // e.g. "al" => "rax", bits [0..7]
        // We'll just map "al" => "rax"; "bl" => "rbx", etc.
        // The first char is r, e.g. 'a' from "al"
        // But we might have "sil" => 's', so let's do a small manual approach:
        // For brevity, we do "r" + s.substr(0,1) + "x" for "al","bl","cl","dl" only
        // If you want to handle "sil","dil","bpl","spl", do so similarly.
        string base = s.substr(0, 1);  // 'a','b','c','d'...
        string rname;
        if (s == "al") rname = "rax";
        else if (s == "bl") rname = "rbx";
        else if (s == "cl") rname = "rcx";
        else if (s == "dl") rname = "rdx";
        else if (s == "sil") rname = "rsi";
        else if (s == "dil") rname = "rdi";
        else if (s == "bpl") rname = "rbp";
        else if (s == "spl") rname = "rsp";
        if (hasVal(ctx[rname], 0, 7)) {
            res = readVal(ctx[rname], 0, 7);
            return res;
        } else {
            Value *v0 = ctx[rname];
            Value *v1 = new Value(CONCRETE, "0x00000000000000ff");
            res = buildop2("and", v0, v1);
            return res;
        }
    }
    // 8-bit high registers: "ah", "bh", "ch", "dh"
    else if (s == "ah" || s == "bh" || s == "ch" || s == "dh") {
        // e.g. "ah" => bits [8..15] of "rax"
        string rname;
        if (s == "ah") rname = "rax";
        else if (s == "bh") rname = "rbx";
        else if (s == "ch") rname = "rcx";
        else if (s == "dh") rname = "rdx";

        if (hasVal(ctx[rname], 8, 15)) {
            res = readVal(ctx[rname], 8, 15);
            return res;
        } else {
            Value *v0 = ctx[rname];
            Value *v1 = new Value(CONCRETE, "0x000000000000ff00");
            Value *v2 = buildop2("and", v0, v1);
            // shift right by 8 bits
            Value *v3 = new Value(CONCRETE, "0x8");
            res = buildop2("shr", v2, v3);
            return res;
        }
    } else {
        cerr << "unknown reg name: " << s << endl;
        return NULL;
    }
}

//-------------------------------------------------------
// 64-bit writeReg: write a symbolic/concrete Value
//-------------------------------------------------------
void SEEngine::writeReg(string &s, Value *v)
{
    Value *res;
    // Full 64-bit
    if (s == "rax" || s == "rbx" || s == "rcx" || s == "rdx" ||
        s == "rsi" || s == "rdi" || s == "rsp" || s == "rbp") {
        ctx[s] = v;
    }
    // 32-bit sub-register
    else if (s == "eax" || s == "ebx" || s == "ecx" || s == "edx") {
        // e.g. "eax" => combine the old "rax" top bits with new lower 32 bits
        string rname = "r" + s.substr(1); // "eax" => "rax"
        // mask out the low 32 bits
        Value *v0 = new Value(CONCRETE, "0xffffffff00000000");
        Value *v1 = ctx[rname];
        Value *v2 = buildop2("and", v1, v0);
        // or in the new value
        res = buildop2("or", v2, v);
        ctx[rname] = res;
    }
    // 16-bit sub-register
    else if (s == "ax" || s == "bx" || s == "cx" || s == "dx" ||
             s == "si" || s == "di" || s == "bp" || s == "sp")
    {
        // e.g. "ax" => "rax"
        string rname = "r" + s;
        // mask out the low 16 bits
        Value *v0 = new Value(CONCRETE, "0xffffffffffff0000");
        Value *v1 = ctx[rname];
        Value *v2 = buildop2("and", v1, v0);
        res = buildop2("or", v2, v);
        ctx[rname] = res;
    }
    // 8-bit low: "al", "bl", etc.
    else if (s == "al" || s == "bl" || s == "cl" || s == "dl" ||
             s == "sil" || s == "dil" || s == "bpl" || s == "spl")
    {
        string rname;
        if (s == "al")  rname = "rax";
        else if (s == "bl") rname = "rbx";
        else if (s == "cl") rname = "rcx";
        else if (s == "dl") rname = "rdx";
        else if (s == "sil") rname = "rsi";
        else if (s == "dil") rname = "rdi";
        else if (s == "bpl") rname = "rbp";
        else if (s == "spl") rname = "rsp";

        // mask out low 8 bits
        Value *v0 = new Value(CONCRETE, "0xffffffffffffff00");
        Value *v1 = ctx[rname];
        Value *v2 = buildop2("and", v1, v0);
        res = buildop2("or", v2, v);
        ctx[rname] = res;
    }
    // 8-bit high: "ah", "bh", "ch", "dh"
    else if (s == "ah" || s == "bh" || s == "ch" || s == "dh") {
        string rname;
        if (s == "ah") rname = "rax";
        else if (s == "bh") rname = "rbx";
        else if (s == "ch") rname = "rcx";
        else if (s == "dh") rname = "rdx";

        // If the old is concrete but the new is symbolic in bits [8..15], we can use writeVal
        if (ctx[rname]->isConcrete() && v->isSymbol()) {
            ctx[rname] = writeVal(v, ctx[rname], 8, 15);
        } else {
            // shift the new value left by 8
            Value *v0 = new Value(CONCRETE, "0x8");
            Value *v1 = buildop2("shl", v, v0);
            // mask out bits [8..15]
            Value *v2 = new Value(CONCRETE, "0xffffffffffff00ff");
            Value *v3 = ctx[rname];
            Value *v4 = buildop2("and", v3, v2);
            res = buildop2("or", v4, v1);
            ctx[rname] = res;
        }
    } else {
        cerr << "unknown reg name to write: " << s << endl;
    }
}

//----------------------------------------------------------------
// 64-bit isnew, issubset, issuperset
//----------------------------------------------------------------
bool SEEngine::issubset(AddrRange ar, AddrRange *superset)
{
    for (auto it = mem.begin(); it != mem.end(); ++it) {
        AddrRange curar = it->first;
        if (curar.first <= ar.first && curar.second >= ar.second) {
            superset->first = curar.first;
            superset->second = curar.second;
            return true;
        }
    }
    return false;
}

bool SEEngine::issuperset(AddrRange ar, AddrRange *subset)
{
    for (auto it = mem.begin(); it != mem.end(); ++it) {
        AddrRange curar = it->first;
        if (curar.first >= ar.first && curar.second <= ar.second) {
            subset->first = curar.first;
            subset->second = curar.second;
            return true;
        }
    }
    return false;
}

bool SEEngine::isnew(AddrRange ar)
{
    for (auto it = mem.begin(); it != mem.end(); ++it) {
        AddrRange curar = it->first;
        // Overlap check
        if ((curar.first <= ar.first && curar.second >= ar.first) ||
            (curar.first <= ar.second && curar.second >= ar.second)) {
            return false;
        }
    }
    return true;
}

//-------------------------------------------------------
//    64-bit readMem
//-------------------------------------------------------
Value* SEEngine::readMem(ADDR64 addr, int nbyte)
{
    ADDR64 end = addr + nbyte - 1;
    AddrRange ar(addr, end), res;

    // If exact range is found in mem, return it
    if (memfind(ar)) return mem[ar];

    // If range is brand new, create a new symbolic Value
    if (isnew(ar)) {
        Value *v = new Value(SYMBOL, nbyte * 8); // nbyte*8 bits
        mem[ar] = v;
        meminput[v] = ar;
        return v;
    }
    // If it is a subset of an existing range
    else if (issubset(ar, &res)) {
        ADDR64 b1 = ar.first, e1 = ar.second;
        ADDR64 b2 = res.first, e2 = res.second;

        // Construct a bit-mask for the subset
        // For little-endian, we read from the “top” bits
        // but let's keep it simple:
        // # of bytes = e2-b2+1
        // We'll build a hex string with (e2-b2+1)*2 hex digits,
        // where bytes in [b1..e1] => "ff", else "00".
        // Then shift right as needed.
        ostringstream mask;
        mask << "0x";
        for (ADDR64 i = e2; i >= b2; --i) {
            if (i >= b1 && i <= e1)
                mask << "ff";
            else
                mask << "00";
        }
        // how many bits to shift right?
        uint64_t shift_bits = (b1 - b2) * 8;
        ostringstream strs;
        strs << "0x" << hex << shift_bits;

        Value *v0 = mem[res];
        Value *v1 = new Value(CONCRETE, mask.str());
        Value *v2 = buildop2("and", v0, v1);
        Value *v3 = new Value(CONCRETE, strs.str());
        Value *v4 = buildop2("shr", v2, v3); // shift right
        return v4;
    }
    else {
        cerr << "readMem: Partial overlapping symbolic memory access not implemented!\n";
        return NULL;
    }
}

//-------------------------------------------------------
//    64-bit writeMem
//-------------------------------------------------------
void SEEngine::writeMem(ADDR64 addr, int nbyte, Value *v)
{
    ADDR64 end = addr + nbyte - 1;
    AddrRange ar(addr, end), res;

    // If exact range or new
    if (memfind(ar) || isnew(ar)) {
        mem[ar] = v;
        return;
    }
    // If new range is a superset of an existing range
    else if (issuperset(ar, &res)) {
        mem.erase(res);
        mem[ar] = v;
        return;
    }
    // If new range is subset of an existing range
    else if (issubset(ar, &res)) {
        ADDR64 b1 = ar.first, e1 = ar.second;
        ADDR64 b2 = res.first, e2 = res.second;

        // Build a mask to zero out [b1..e1] in the old value
        ostringstream mask;
        mask << "0x";
        for (ADDR64 i = e2; i >= b2; --i) {
            if (i >= b1 && i <= e1)
                mask << "00";
            else
                mask << "ff";
        }
        // how many bits to shift left?
        uint64_t shift_bits = (b1 - b2) * 8;
        ostringstream strs;
        strs << "0x" << hex << shift_bits;

        Value *v0 = mem[res];
        Value *v1 = new Value(CONCRETE, mask.str());
        Value *v2 = buildop2("and", v0, v1);
        Value *v3 = new Value(CONCRETE, strs.str());
        Value *v4 = buildop2("shl", v, v3);
        Value *v5 = buildop2("or", v2, v4);
        mem[res] = v5;
    }
    else {
        cerr << "writeMem: Partial overlapping symbolic memory access not implemented!\n";
    }
}

//-------------------------------------------------------
//    64-bit init(...) variants
//-------------------------------------------------------
void SEEngine::init(Value *v1, Value *v2, Value *v3, Value *v4,
                    Value *v5, Value *v6, Value *v7, Value *v8,
                    list<Inst>::iterator it1,
                    list<Inst>::iterator it2)
{
    ctx["rax"] = v1;
    ctx["rbx"] = v2;
    ctx["rcx"] = v3;
    ctx["rdx"] = v4;
    ctx["rsi"] = v5;
    ctx["rdi"] = v6;
    ctx["rsp"] = v7;
    ctx["rbp"] = v8;

    reginput[v1] = "rax";
    reginput[v2] = "rbx";
    reginput[v3] = "rcx";
    reginput[v4] = "rdx";
    reginput[v5] = "rsi";
    reginput[v6] = "rdi";
    reginput[v7] = "rsp";
    reginput[v8] = "rbp";

    this->start = it1;
    this->end = it2;
}

void SEEngine::init(list<Inst>::iterator it1,
                    list<Inst>::iterator it2)
{
    this->start = it1;
    this->end = it2;
}

void SEEngine::initAllRegSymol(list<Inst>::iterator it1,
                               list<Inst>::iterator it2)
{
    Value *v1 = new Value(SYMBOL, 64);
    Value *v2 = new Value(SYMBOL, 64);
    Value *v3 = new Value(SYMBOL, 64);
    Value *v4 = new Value(SYMBOL, 64);
    Value *v5 = new Value(SYMBOL, 64);
    Value *v6 = new Value(SYMBOL, 64);
    Value *v7 = new Value(SYMBOL, 64);
    Value *v8 = new Value(SYMBOL, 64);

    ctx["rax"] = v1;
    ctx["rbx"] = v2;
    ctx["rcx"] = v3;
    ctx["rdx"] = v4;
    ctx["rsi"] = v5;
    ctx["rdi"] = v6;
    ctx["rsp"] = v7;
    ctx["rbp"] = v8;

    reginput[v1] = "rax";
    reginput[v2] = "rbx";
    reginput[v3] = "rcx";
    reginput[v4] = "rdx";
    reginput[v5] = "rsi";
    reginput[v6] = "rdi";
    reginput[v7] = "rsp";
    reginput[v8] = "rbp";

    start = it1;
    end = it2;
}

// no-effect instructions
set<string> noeffectinst = {
    "test","jmp","jz","jbe","jo","jno","js","jns","je","jne",
    "jnz","jb","jnae","jc","jnb","jae","jnc","jna","ja","jnbe",
    "jl","jnge","jge","jnl","jle","jng","jg","jnle","jp","jpe",
    "jnp","jpo","jcxz","jecxz","ret","cmp","call"
};

// The main symbolic execution loop
int SEEngine::symexec()
{
    for (list<Inst>::iterator it = start; it != end; ++it) {
        ip = it;

        // skip no-effect instructions
        if (noeffectinst.find(it->opcstr) != noeffectinst.end())
            continue;

        switch (it->oprnum) {
        case 0:
            // no operands
            break;
        case 1:
        {
            Operand *op0 = it->oprd[0];
            Value *v0, *res, *temp;
            int nbyte;
            if (it->opcstr == "push") {
                if (op0->ty == /*IMM*/ 1) {
                    v0 = new Value(CONCRETE, op0->field[0]);
                    writeMem(it->waddr, 8, v0); // 64-bit push => 8 bytes
                } else if (op0->ty == /*REG*/ 2) {
                    nbyte = op0->bit / 8;
                    temp = readReg(op0->field[0]);
                    writeMem(it->waddr, nbyte, temp);
                } else if (op0->ty == /*MEM*/ 3) {
                    nbyte = op0->bit / 8;
                    v0 = readMem(it->raddr, nbyte);
                    writeMem(it->waddr, nbyte, v0);
                } else {
                    cout << "push error: operand not Imm/Reg/Mem!" << endl;
                    return 1;
                }
            } else if (it->opcstr == "pop") {
                if (op0->ty == /*REG*/ 2) {
                    nbyte = op0->bit / 8;
                    temp = readMem(it->raddr, nbyte);
                    writeReg(op0->field[0], temp);
                } else if (op0->ty == /*MEM*/ 3) {
                    nbyte = op0->bit / 8;
                    temp = readMem(it->raddr, nbyte);
                    writeMem(it->waddr, nbyte, temp);
                } else {
                    cout << "pop error: operand not Reg/Mem!" << endl;
                    return 1;
                }
            } else {
                // 1-operand instructions, e.g. "neg", "inc", etc.
                if (op0->ty == /*REG*/ 2) {
                    v0 = readReg(op0->field[0]);
                    res = buildop1(it->opcstr, v0);
                    writeReg(op0->field[0], res);
                } else if (op0->ty == /*MEM*/ 3) {
                    nbyte = op0->bit / 8;
                    v0 = readMem(it->raddr, nbyte);
                    res = buildop1(it->opcstr, v0);
                    writeMem(it->waddr, nbyte, res);
                } else {
                    cout << "[Error] " << it->id << ": Unknown 1-op instruction!\n";
                    return 1;
                }
            }
            break;
        }
        case 2:
        {
            Operand *op0 = it->oprd[0];
            Operand *op1 = it->oprd[1];
            Value *v0, *v1, *res, *temp;
            int nbyte;

            if (it->opcstr == "mov") {
                // mov
                if (op0->ty == /*REG*/ 2) {
                    if (op1->ty == /*IMM*/ 1) {
                        v1 = new Value(CONCRETE, op1->field[0]);
                        writeReg(op0->field[0], v1);
                    } else if (op1->ty == /*REG*/ 2) {
                        temp = readReg(op1->field[0]);
                        writeReg(op0->field[0], temp);
                    } else if (op1->ty == /*MEM*/ 3) {
                        nbyte = op1->bit / 8;
                        v1 = readMem(it->raddr, nbyte);
                        writeReg(op0->field[0], v1);
                    } else {
                        cerr << "op1 not Imm/Reg/Mem\n";
                        return 1;
                    }
                } else if (op0->ty == /*MEM*/ 3) {
                    if (op1->ty == /*IMM*/ 1) {
                        temp = new Value(CONCRETE, op1->field[0]);
                        nbyte = op0->bit / 8;
                        writeMem(it->waddr, nbyte, temp);
                    } else if (op1->ty == /*REG*/ 2) {
                        temp = readReg(op1->field[0]);
                        nbyte = op0->bit / 8;
                        writeMem(it->waddr, nbyte, temp);
                    } else {
                        cerr << "Error: The first operand in MOV is Mem, second not Imm/Reg?\n";
                    }
                } else {
                    cerr << "Error: The first operand in MOV is not Reg or Mem!\n";
                }
            }
            else if (it->opcstr == "lea") {
                // lea
                if (op0->ty != /*REG*/ 2 || op1->ty != /*MEM*/ 3) {
                    cerr << "lea format error!\n";
                }
                switch (op1->tag) {
                case 5:
                {
                    // e.g. lea reg, [r1 + r2*n]
                    Value *f0, *f1, *f2;
                    f0 = readReg(op1->field[0]);
                    f1 = readReg(op1->field[1]);
                    f2 = new Value(CONCRETE, op1->field[2]);
                    res = buildop2("imul", f1, f2);
                    res = buildop2("add", f0, res);
                    writeReg(op0->field[0], res);
                    break;
                }
                default:
                    cerr << "Other tags in addr not ready for lea!\n";
                    break;
                }
            }
            else if (it->opcstr == "xchg") {
                // xchg
                if (op1->ty == /*REG*/ 2) {
                    v1 = readReg(op1->field[0]);
                    if (op0->ty == /*REG*/ 2) {
                        v0 = readReg(op0->field[0]);
                        writeReg(op1->field[0], v0);
                        writeReg(op0->field[0], v1);
                    } else if (op0->ty == /*MEM*/ 3) {
                        nbyte = op0->bit / 8;
                        v0 = readMem(it->raddr, nbyte);
                        writeReg(op1->field[0], v0);
                        writeMem(it->waddr, nbyte, v1);
                    } else {
                        cerr << "xchg error: 1\n";
                    }
                } else if (op1->ty == /*MEM*/ 3) {
                    nbyte = op1->bit / 8;
                    v1 = readMem(it->raddr, nbyte);
                    if (op0->ty == /*REG*/ 2) {
                        v0 = readReg(op0->field[0]);
                        writeReg(op0->field[0], v1);
                        writeMem(it->waddr, nbyte, v0);
                    } else {
                        cerr << "xchg error: 3\n";
                    }
                } else {
                    cerr << "xchg error: 2\n";
                }
            }
            else {
                // handle 2-operand instructions: shl, shr, add, sub, xor, and, or, etc.
                // read operand 1
                if (op1->ty == /*IMM*/ 1) {
                    v1 = new Value(CONCRETE, op1->field[0]);
                } else if (op1->ty == /*REG*/ 2) {
                    v1 = readReg(op1->field[0]);
                } else if (op1->ty == /*MEM*/ 3) {
                    nbyte = op1->bit / 8;
                    v1 = readMem(it->raddr, nbyte);
                } else {
                    cerr << "other instructions: op1 not Imm/Reg/Mem!\n";
                    return 1;
                }

                // read operand 0
                if (op0->ty == /*REG*/ 2) {
                    v0 = readReg(op0->field[0]);
                    res = buildop2(it->opcstr, v0, v1);
                    writeReg(op0->field[0], res);
                } else if (op0->ty == /*MEM*/ 3) {
                    nbyte = op0->bit / 8;
                    v0 = readMem(it->raddr, nbyte);
                    res = buildop2(it->opcstr, v0, v1);
                    writeMem(it->waddr, nbyte, res);
                } else {
                    cerr << "other instructions: op0 not Reg/Mem!\n";
                    return 1;
                }
            }
            break;
        }
        case 3:
        {
            // three-operands instructions: e.g. "imul reg, reg, imm"
            Operand *op0 = it->oprd[0];
            Operand *op1 = it->oprd[1];
            Operand *op2 = it->oprd[2];
            Value *v1, *v2, *res;

            if (it->opcstr == "imul" && op0->ty == /*REG*/ 2 &&
                op1->ty == /*REG*/ 2 && op2->ty == /*IMM*/ 1)
            {
                v1 = readReg(op1->field[0]);
                v2 = new Value(CONCRETE, op2->field[0]);
                res = buildop2("imul", v1, v2);
                writeReg(op0->field[0], res);
            } else {
                cerr << "3-operands instructions other than imul not handled!\n";
            }
            break;
        }
        default:
            cerr << "all instructions: operand count > 3 not handled!\n";
            break;
        }
    }
    return 0;
}

//-----------------------------------------------------------------------
// Recursively traverse a Value to print it as a parenthesized formula
//-----------------------------------------------------------------------
void traverse(Value *v)
{
    if (!v) return;
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE)
            cout << v->conval;
        else
            cout << "sym" << v->id;
    } else {
        cout << "(" << op->opty << " ";
        traverse(op->val[0]);
        cout << " ";
        traverse(op->val[1]);
        cout << ")";
    }
}

// Another “pretty” traverse for debugging
void traverse2(Value *v)
{
    if (!v) return;
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE)
            cout << v->conval;
        else if (v->valty == SYMBOL)
            cout << "sym" << v->id << " ";
        else if (v->valty == HYBRID) {
            cout << "[hyb" << v->id << " ";
            for (auto &kv : v->childs) {
                cout << "[" << kv.first.first << "," << kv.first.second << "]:";
                traverse2(kv.second);
            }
            cout << "]";
        } else {
            cout << "unknown type\n";
            return;
        }
    } else {
        cout << "(" << op->opty << " ";
        traverse2(op->val[0]);
        cout << " ";
        traverse2(op->val[1]);
        cout << ")";
    }
}

// Output the formula for a given register
void SEEngine::outputFormula(string reg)
{
    Value *v = ctx[reg];
    if (!v) {
        cout << reg << " is null\n";
        return;
    }
    cout << "sym" << v->id << " =\n";
    traverse(v);
    cout << endl;
}

void SEEngine::dumpreg(string reg)
{
    Value *v = ctx[reg];
    if (!v) {
        cout << "reg " << reg << " is null\n";
        return;
    }
    cout << "reg " << reg << " =\n";
    traverse2(v);
    cout << endl;
}

// Return all Values from registers + memory that have a non-null Operation
vector<Value*> SEEngine::getAllOutput()
{
    vector<Value*> outputs;
    // Check registers
    {
        Value *v = ctx["rax"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rbx"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rcx"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rdx"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rsi"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rdi"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rsp"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rbp"];
        if (v && v->opr) outputs.push_back(v);
    }
    // Check memory
    for (auto const& x : mem) {
        Value *v = x.second;
        if (v && v->opr)
            outputs.push_back(v);
    }

    return outputs;
}

void SEEngine::printAllRegFormulas()
{
    // Example for a few registers:
    cout << "rax: ";
    outputFormula("rax");
    printInputSymbols("rax");
    cout << endl;

    cout << "rbx: ";
    outputFormula("rbx");
    printInputSymbols("rbx");
    cout << endl;

    cout << "rcx: ";
    outputFormula("rcx");
    printInputSymbols("rcx");
    cout << endl;

    cout << "rdx: ";
    outputFormula("rdx");
    printInputSymbols("rdx");
    cout << endl;

    cout << "rsi: ";
    outputFormula("rsi");
    printInputSymbols("rsi");
    cout << endl;

    cout << "rdi: ";
    outputFormula("rdi");
    printInputSymbols("rdi");
    cout << endl;
}

// Print memory formulas
void SEEngine::printAllMemFormulas()
{
    for (auto &x : mem) {
        AddrRange ar = x.first;
        Value *v = x.second;
        if (!v) continue;
        printf("[0x%llx,0x%llx]: ", (long long)ar.first, (long long)ar.second);
        cout << "sym" << v->id << "=\n";
        traverse(v);
        cout << endl << endl;
    }
}

// BFS to get all symbolic inputs for Value *output
set<Value*> *getInputs(Value *output)
{
    queue<Value*> que;
    que.push(output);

    set<Value*> *inputset = new set<Value*>;

    while (!que.empty()) {
        Value *v = que.front();
        que.pop();
        Operation *op = v->opr;
        if (!op) {
            if (v->valty == SYMBOL)
                inputset->insert(v);
        } else {
            for (int i = 0; i < 3; ++i) {
                if (op->val[i]) que.push(op->val[i]);
            }
        }
    }
    return inputset;
}

void SEEngine::printInputSymbols(string output)
{
    Value *v = ctx[output];
    if (!v) {
        cout << output << " is null\n";
        return;
    }
    set<Value*> *insyms = getInputs(v);
    cout << insyms->size() << " input symbols: ";
    for (auto &symval : *insyms) {
        cout << "sym" << symval->id << " ";
    }
    cout << endl;
}

// Print formula details: which memory/reg inputs feed into Value *v
void SEEngine::printformula(Value *v)
{
    set<Value*> *insyms = getInputs(v);

    cout << insyms->size() << " input symbols: " << endl;
    for (auto &symval : *insyms) {
        cout << "sym" << symval->id << ": ";
        auto it1 = meminput.find(symval);
        if (it1 != meminput.end()) {
            printf("[0x%llx, 0x%llx]\n",
                   (long long)it1->second.first,
                   (long long)it1->second.second);
        } else {
            auto it2 = reginput.find(symval);
            if (it2 != reginput.end()) {
                cout << it2->second << endl;
            }
        }
    }
    cout << endl;

    cout << "sym" << v->id << "=\n";
    traverse(v);
    cout << endl;
}

void SEEngine::printMemFormula(ADDR64 addr1, ADDR64 addr2)
{
    AddrRange ar(addr1, addr2);
    if (mem.find(ar) == mem.end()) {
        cout << "No memory formula for [0x" << hex << addr1
             << ", 0x" << addr2 << "]" << endl;
        return;
    }
    Value *v = mem[ar];
    printformula(v);
}

// Helper to recursively evaluate a Value
static ADDR64 eval(Value *v, map<Value*, ADDR64> *inmap)
{
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE) {
            return stoull(v->conval, 0, 16);
        } else {
            // symbolic input
            return (*inmap)[v];
        }
    } else {
        ADDR64 op0 = 0, op1 = 0;
        if (op->val[0]) op0 = eval(op->val[0], inmap);
        if (op->val[1]) op1 = eval(op->val[1], inmap);

        if (op->opty == "add") {
            return op0 + op1;
        } else if (op->opty == "sub") {
            return op0 - op1;
        } else if (op->opty == "imul") {
            return op0 * op1;
        } else if (op->opty == "xor") {
            return op0 ^ op1;
        } else if (op->opty == "and") {
            return op0 & op1;
        } else if (op->opty == "or") {
            return op0 | op1;
        } else if (op->opty == "shl") {
            return op0 << op1;
        } else if (op->opty == "shr") {
            return op0 >> op1;
        } else if (op->opty == "neg") {
            return (ADDR64)(-((int64_t)op0));
        } else if (op->opty == "inc") {
            return op0 + 1;
        } else {
            cout << "Instruction: " << op->opty << " is not interpreted!\n";
            return 1;
        }
    }
}

// Execute a formula v with a set of symbolic inputs inmap
ADDR64 SEEngine::conexec(Value *f, map<Value*, ADDR64> *inmap)
{
    set<Value*> *inputsym = getInputs(f);
    set<Value*> inmapkeys;
    for (auto &pair : *inmap) {
        inmapkeys.insert(pair.first);
    }
    // quick check (not robust) that all symbolic inputs are in inmap
    if (inmapkeys != *inputsym) {
        cout << "Some inputs don't have parameters!\n";
        return 1;
    }
    return eval(f, inmap);
}

// build a map from a vector of Values to a vector of 64-bit inputs
map<Value*, ADDR64> buildinmap(vector<Value*> *vv, vector<ADDR64> *input)
{
    map<Value*, ADDR64> inmap;
    if (vv->size() != input->size()) {
        cout << "Mismatch in # of input symbols!\n";
        return inmap;
    }
    for (size_t i = 0; i < vv->size(); i++) {
        inmap.insert({(*vv)[i], (*input)[i]});
    }
    return inmap;
}

// gather all input Values from a formula f
vector<Value*> getInputVector(Value *f)
{
    set<Value*> *inset = getInputs(f);
    vector<Value*> vv;
    for (auto &symval : *inset) {
        vv.push_back(symval);
    }
    return vv;
}

// a global postfix for naming
string sympostfix;

// output the calculation of v in a simplistic CVC-like style
static void outputCVC(Value *v, FILE *fp)
{
    if (!v) return;
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE) {
            uint64_t i = stoull(v->conval, 0, 16);
            fprintf(fp, "0hex%016llx", (long long)i);
        } else {
            fprintf(fp, "sym%d%s", v->id, sympostfix.c_str());
        }
    } else {
        if (op->opty == "add") {
            fprintf(fp, "BVPLUS(64, ");
            outputCVC(op->val[0], fp);
            fprintf(fp, ", ");
            outputCVC(op->val[1], fp);
            fprintf(fp, ")");
        } else if (op->opty == "sub") {
            fprintf(fp, "BVSUB(64, ");
            outputCVC(op->val[0], fp);
            fprintf(fp, ", ");
            outputCVC(op->val[1], fp);
            fprintf(fp, ")");
        } else if (op->opty == "imul") {
            fprintf(fp, "BVMULT(64, ");
            outputCVC(op->val[0], fp);
            fprintf(fp, ", ");
            outputCVC(op->val[1], fp);
            fprintf(fp, ")");
        } else if (op->opty == "xor") {
            fprintf(fp, "BVXOR(");
            outputCVC(op->val[0], fp);
            fprintf(fp, ", ");
            outputCVC(op->val[1], fp);
            fprintf(fp, ")");
        } else if (op->opty == "and") {
            outputCVC(op->val[0], fp);
            fprintf(fp, " & ");
            outputCVC(op->val[1], fp);
        } else if (op->opty == "or") {
            outputCVC(op->val[0], fp);
            fprintf(fp, " | ");
            outputCVC(op->val[1], fp);
        } else if (op->opty == "neg") {
            fprintf(fp, "~");
            outputCVC(op->val[0], fp);
        } else if (op->opty == "shl") {
            outputCVC(op->val[0], fp);
            fprintf(fp, " << ");
            outputCVC(op->val[1], fp);
        } else if (op->opty == "shr") {
            outputCVC(op->val[0], fp);
            fprintf(fp, " >> ");
            outputCVC(op->val[1], fp);
        } else {
            cout << "Instruction: " << op->opty << " not interpreted in CVC!\n";
        }
    }
}

// Output to file "formula.cvc"
void outputCVCFormula(Value *f)
{
    const char *cvcfile = "formula.cvc";
    FILE *fp = fopen(cvcfile, "w");
    if (!fp) {
        cerr << "Cannot open formula.cvc for writing!\n";
        return;
    }
    outputCVC(f, fp);
    fclose(fp);
}

// Check equivalence of f1, f2 with variable mapping
void outputChkEqCVC(Value *f1, Value *f2, map<int,int> *m)
{
    const char *cvcfile = "ChkEq.cvc";
    FILE *fp = fopen(cvcfile, "w");
    if (!fp) {
        cerr << "Cannot open ChkEq.cvc!\n";
        return;
    }

    for (auto &pr : *m) {
        fprintf(fp, "sym%da: BV(64);\n", pr.first);
        fprintf(fp, "sym%db: BV(64);\n", pr.second);
    }
    fprintf(fp, "\n");
    for (auto &pr : *m) {
        fprintf(fp, "ASSERT(sym%da = sym%db);\n", pr.first, pr.second);
    }

    fprintf(fp, "\nQUERY(\n");
    sympostfix = "a";
    outputCVC(f1, fp);
    fprintf(fp, "\n=\n");
    sympostfix = "b";
    outputCVC(f2, fp);
    fprintf(fp, ");\n");
    fprintf(fp, "COUNTEREXAMPLE;\n");
    fclose(fp);
}

// Output all bit formulas for variable mapping
void outputBitCVC(Value *f1, Value *f2,
                  vector<Value*> *inv1, vector<Value*> *inv2,
                  list<FullMap> *result)
{
    // This function was never fully elaborated in the original, so we do
    // a direct “64-bit style” update. Adjust as needed.
    int n = 1;
    for (auto &mpair : *result) {
        string cvcfile = "formula" + to_string(n++) + ".cvc";
        FILE *fp = fopen(cvcfile.c_str(), "w");
        if (!fp) {
            cerr << "Cannot open " << cvcfile << "!\n";
            continue;
        }

        map<int,int> *inmap = &(mpair.first);
        map<int,int> *outmap = &(mpair.second);

        // output 64-bit placeholders
        // if you want bit-level, you’d need 64 separate bits each
        for (int i = 0; i < 64*(int)inv1->size(); i++) {
            fprintf(fp, "bit%da: BV(1);\n", i);
        }
        for (int i = 0; i < 64*(int)inv2->size(); i++) {
            fprintf(fp, "bit%db: BV(1);\n", i);
        }

        // Input mapping
        for (auto &pr : *inmap) {
            fprintf(fp, "ASSERT(bit%da = bit%db);\n", pr.first, pr.second);
        }
        fprintf(fp, "\nQUERY(\n");

        // We’d proceed to build the 64-bit composition from bits
        // This is partially left as an exercise, since it’s quite large.

        fclose(fp);
    }
}

void SEEngine::showMemInput()
{
    cout << "Inputs in memory:\n";
    for (auto &mp : meminput) {
        Value *symv = mp.first;
        AddrRange rng = mp.second;
        printf("sym%d: [0x%llx, 0x%llx]\n", symv->id, (long long)rng.first, (long long)rng.second);
    }
    cout << endl;
}
