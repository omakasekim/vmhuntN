#ifndef MG_SYMENGINE_HPP
#define MG_SYMENGINE_HPP

#include <map>
#include <list>
#include <vector>
#include <string>
#include <cstdint>
#include <set>
#include <algorithm>

// Avoid using 'using namespace std;' in headers to prevent namespace pollution

#include "core.hpp"    // Ensure Parameter::idx and Inst::raddr/waddr are uint64_t
#include "parser.hpp"  // parseTrace(...), parseOperand(...)

/*
 * Type Definitions
 */

// Define ADDR64 as an alias for uint64_t for clarity
typedef uint64_t ADDR64;

/*
 * AddrRange Structure
 * Represents a range of addresses from 'start' to 'end'.
 * Implements the '<' operator to allow usage as a key in std::map.
 */
struct AddrRange {
    ADDR64 start;
    ADDR64 end;

    // Default constructor
    AddrRange() : start(0), end(0) {}

    // Parameterized constructor
    AddrRange(ADDR64 s, ADDR64 e) : start(s), end(e) {}

    // Comparison operator for std::map
    bool operator<(const AddrRange &other) const {
        if (start < other.start) return true;
        if (start == other.start) return end < other.end;
        return false;
    }
};

/*
 * Forward Declarations
 * Assuming Operand, Inst, Value, Operation are defined in core.hpp or parser.hpp
 */
struct Operation;
struct Value;
struct Operand; // Defined in core.hpp or parser.hpp
struct Inst;    // Defined in core.hpp or parser.hpp

/*
 * SEEngine Class
 * Symbolic Execution Engine tailored for 64-bit architectures.
 */
class SEEngine {
private:
    // Context: Mapping from register names to their symbolic Values
    std::map<std::string, Value*> ctx;

    // Instruction Pointer Range: Iterators into a list of Inst
    std::list<Inst>::iterator start;
    std::list<Inst>::iterator end;
    std::list<Inst>::iterator ip;

    // Memory Model: Mapping from AddrRange to symbolic Values
    std::map<AddrRange, Value*> mem;

    // Inputs from Memory and Registers
    std::map<Value*, AddrRange> meminput;
    std::map<Value*, std::string> reginput;

    /*
     * Helper Functions
     */

    // Check if a memory range exists in the memory model
    bool memfind(const AddrRange& ar);

    // Overloaded memfind for raw addresses
    bool memfind(ADDR64 b, ADDR64 e);

    // Check if a memory range is new
    bool isnew(const AddrRange& ar);

    // Check if 'ar' is a subset of 'superset'
    bool issubset(const AddrRange& ar, AddrRange *superset);

    // Check if 'ar' is a superset of 'subset'
    bool issuperset(const AddrRange& ar, AddrRange *subset);

    // Read a register's symbolic Value
    Value* readReg(const std::string &s);

    // Write a symbolic Value to a register
    void writeReg(const std::string &s, Value *v);

    // Read from memory
    Value* readMem(ADDR64 addr, int nbyte);

    // Write to memory
    void writeMem(ADDR64 addr, int nbyte, Value *v);

    // Get the concrete (numeric) value of a 64-bit register, if known
    ADDR64 getRegConVal(const std::string &reg) const;

    // Evaluate an addressing mode to a 64-bit address
    ADDR64 calcAddr(const Operand &opr) const;

    // Print the symbolic formula of a Value
    void printformula(Value* v) const;

public:
    /*
     * Constructors
     */

    // Default constructor
    SEEngine();

    /*
     * Initialization Functions
     */

    // Initialize registers with specific symbolic Values and set instruction pointers
    void init(Value *v1, Value *v2, Value *v3, Value *v4,
              Value *v5, Value *v6, Value *v7, Value *v8,
              std::list<Inst>::iterator it1,
              std::list<Inst>::iterator it2);

    // Initialize instruction pointers without specific register Values
    void init(std::list<Inst>::iterator it1,
              std::list<Inst>::iterator it2);

    // Initialize all registers as symbolic
    void initAllRegSymbol(std::list<Inst>::iterator it1,
                          std::list<Inst>::iterator it2);

    /*
     * Execution Functions
     */

    // Perform symbolic execution
    int symexec();

    // Perform concrete execution, taking a symbolic Value and input map
    ADDR64 conexec(Value *f, std::map<Value*, ADDR64> *input);

    /*
     * Output Functions
     */

    // Output the symbolic formula of a specific register
    void outputFormula(const std::string &reg) const;

    // Dump the symbolic formula of a specific register
    void dumpreg(const std::string &reg) const;

    // Print all register symbolic formulas
    void printAllRegFormulas() const;

    // Print all memory symbolic formulas
    void printAllMemFormulas() const;

    // Print input symbols to an output file
    void printInputSymbols(const std::string &output) const;

    /*
     * Accessor Functions
     */

    // Get the symbolic Value of a specific register
    Value *getValue(const std::string &s) const;

    // Gather all final symbolic outputs
    std::vector<Value*> getAllOutput() const;

    // Show memory inputs
    void showMemInput() const;

    // Print memory formula for a specific address range [addr1, addr2)
    void printMemFormula(ADDR64 addr1, ADDR64 addr2) const;
};

/*
 * External Helper Functions
 */

// Output the symbolic formula of a Value in CVC format
void outputCVCFormula(Value *f);

// Output equality check between two symbolic Values in CVC format
void outputChkEqCVC(Value *f1, Value *f2, std::map<int,int> *m);

// Output bit-level comparisons between two symbolic Values in CVC format
void outputBitCVC(Value *f1, Value *f2,
                 std::vector<Value*> *inv1, std::vector<Value*> *inv2,
                 std::list<FullMap> *result);

// Build an input map from a vector of Values to a vector of 64-bit addresses
std::map<Value*, ADDR64> buildinmap(std::vector<Value*> *vv, std::vector<ADDR64> *input);

// Gather all input Values from a symbolic expression
std::vector<Value*> getInputVector(Value *f);

// Get the name of a symbolic Value
std::string getValueName(Value *v);

#endif // MG_SYMENGINE_HPP
