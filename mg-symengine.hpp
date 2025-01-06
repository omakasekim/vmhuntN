#include <map>
#include <list>
#include <vector>
#include <string>

#include "core.hpp"

using namespace std;
// Example: If you previously had a typedef for ADDR32, replace it with ADDR64
typedef std::uint64_t ADDR64;

// If you had an AddrRange struct for 32-bit, update it similarly to hold 64-bit addresses.
// You can keep the same name "AddrRange" or rename it "AddrRange64" as you prefer.
// For example:

struct Operation;
struct Value;

// Example placeholder for Inst and Operand (not shown in your snippet).

class SEEngine {
private:
    // Update register names to 64-bit
    map<string, Value*> ctx;

    // The instruction pointer range (iterators into a list of Inst)
    list<Inst>::iterator start;
    list<Inst>::iterator end;
    list<Inst>::iterator ip;

    // Memory model: map from 64-bit address ranges to symbolic Values
    map<AddrRange, Value*> mem;

    // Inputs from memory
    map<Value*, AddrRange> meminput;

    // Inputs from registers
    map<Value*, string> reginput;

    // Helper functions
    bool memfind(AddrRange ar) {
        map<AddrRange, Value*>::iterator ii = mem.find(ar);
        return (ii != mem.end());
    }

    // Overload for raw addresses
    bool memfind(ADDR64 b, ADDR64 e) {
        AddrRange ar(b, e);
        map<AddrRange, Value*>::iterator ii = mem.find(ar);
        return (ii != mem.end());
    }

    bool isnew(AddrRange ar);
    bool issubset(AddrRange ar, AddrRange *superset);
    bool issuperset(AddrRange ar, AddrRange *subset);

    Value* readReg(string &s);
    void writeReg(string &s, Value *v);

    // Updated to 64-bit addresses
    Value* readMem(ADDR64 addr, int nbyte);
    void writeMem(ADDR64 addr, int nbyte, Value *v);

    // Example commented-out function if you need it in 64-bit:
    // void readornew(int64_t addr, int nbyte, Value *&v);

    // Return the concrete (numeric) value of a 64-bit register, if known
    ADDR64 getRegConVal(string reg);

    // Evaluate an addressing mode to a 64-bit address
    ADDR64 calcAddr(Operand *opr);

    void printformula(Value* v);

public:
    // Update initialization list to reflect 64-bit registers
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

    // If you still want an init(...) that takes 8 values (for rax, rbx, etc.)
    void init(Value *v1, Value *v2, Value *v3, Value *v4,
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

        start = it1;
        end = it2;
        ip = start;
    }

    // Overloaded init if you don’t need specific reg values
    void init(list<Inst>::iterator it1,
              list<Inst>::iterator it2)
    {
        start = it1;
        end = it2;
        ip = start;
    }

    // Make all registers symbolic
    void initAllRegSymol(list<Inst>::iterator it1,
                         list<Inst>::iterator it2)
    {
        start = it1;
        end = it2;
        ip = start;

        // If you want to set them all to some symbolic value
        // for (auto &r : ctx) {
        //     r.second = makeSymbolicValue(...);
        // }
    }

    int symexec();

    // Now returning a 64-bit address and taking a map of Value* to 64-bit addresses
    ADDR64 conexec(Value *f, map<Value*, ADDR64> *input);

    void outputFormula(string reg);
    void dumpreg(string reg);
    void printAllRegFormulas();
    void printAllMemFormulas();
    void printInputSymbols(string output);

    // Accessor for a particular register’s Value
    Value *getValue(string s) { return ctx[s]; }

    // Possibly gather all final symbolic outputs
    vector<Value*> getAllOutput();

    void showMemInput();

    // Print memory formula for address range [addr1, addr2)
    void printMemFormula(ADDR64 addr1, ADDR64 addr2);
};

// External helper functions updated for 64-bit
void outputCVCFormula(Value *f);

// Now these take 64-bit maps
void outputChkEqCVC(Value *f1, Value *f2, map<int,int> *m);

void outputBitCVC(Value *f1, Value *f2,
                  vector<Value*> *inv1, vector<Value*> *inv2,
                  list<FullMap> *result);

// Build an input map from vector of Values to vector of 64-bit addresses
map<Value*, ADDR64> buildinmap(vector<Value*> *vv, vector<ADDR64> *input);

// Gather all input Values from a symbolic expression
vector<Value*> getInputVector(Value *f);

// Possibly keep the same
string getValueName(Value *v);
