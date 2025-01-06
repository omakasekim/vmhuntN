#ifndef CORE_HPP
#define CORE_HPP
#include <cstdint>
#include <string>
#include <utility>   // for std::pair
#include <map>
#include <vector>

using std::string;
using std::pair;
using std::map;
using std::vector;

// 64-bit address type
typedef uint64_t ADDR64;

// Address range (start, end)
typedef pair<ADDR64, ADDR64> AddrRange;

// Enum of registers supporting both 64-bit and 32-bit registers, FPU, and segments
enum Register {
    RAX, RBX, RCX, RDX,         // 64-bit common registers
    RSI, RDI, RSP, RBP,
    EAX, EBX, ECX, EDX,         // 32-bit registers
    ESI, EDI, ESP, EBP,
    ST0, ST1, ST2, ST3,         // FPU registers
    ST4, ST5,
    CS, DS, ES, FS, GS,         // Segment registers
    SS,
    UNK,                        // Unknown register
};
enum class OperandType {IMM, REG, MEM, UNK} ;
// An operand as parsed from assembly
struct Operand {
    //enum OperandType { IMM, REG, MEM,UNK };
    OperandType ty;             // Immediate, register, or memory
    int tag=0;             // Extra tag (for addressing modes)
    int bit=0;             // Operand size in bits (e.g., 8, 16, 32, 64)
    bool issegaddr=false;      // True if it uses a segment register (e.g. fs:[...])
    string segreg;       // Segment register name if issegaddr == true
    string field[5];     // Fields for operand decoding (e.g., reg name, displacement, etc.)

    Operand() : bit(0), issegaddr(false) {}
};

// A parameter struct for finer-grained operand info
struct Parameter {
    enum Type { IMM, REG, MEM };
    Type ty;
    Register reg;   // Which register if type == REG
    ADDR64 idx;     // If MEM, this could be the address. If IMM, the immediate value

    bool operator==(const Parameter &other);
    bool operator<(const Parameter &other) const;
    bool isIMM();
    void show() const;
};

// A single instruction in the trace
struct Inst {
    int id;                // Unique instruction ID
    string addr;           // Instruction address (string form)
    uint64_t addrn;        // Instruction address (numeric form)
    string assembly;       // Full assembly text
    int opc;               // Opcode (numeric)
    string opcstr;         // Opcode (string)
    vector<string> oprs;   // Raw operands (string)
    int oprnum;            // Number of operands
    Operand *oprd[3];      // Parsed operand structures
    ADDR64 ctxreg[8];      // Context registers (64-bit)
    ADDR64 raddr;          // Memory read address
    ADDR64 waddr;          // Memory write address

    // Parameter-based representation of source/dest
    vector<Parameter> src;    // Primary sources
    vector<Parameter> dst;    // Primary destinations
    vector<Parameter> src2;   // Additional sources (e.g., for xchg)
    vector<Parameter> dst2;   // Additional destinations

    // Helper methods to add parameters
    void addsrc(Parameter::Type t, string s);
    void addsrc(Parameter::Type t, AddrRange a);
    void adddst(Parameter::Type t, string s);
    void adddst(Parameter::Type t, AddrRange a);
    void addsrc2(Parameter::Type t, string s);
    void addsrc2(Parameter::Type t, AddrRange a);
    void adddst2(Parameter::Type t, string s);
    void adddst2(Parameter::Type t, AddrRange a);
};

// A type alias for some mapping structure used in your project
typedef pair<map<int, int>, map<int, int>> FullMap;

// Utility function to convert an enum Register to its string name
string reg2string(Register reg);
#endif // CORE_HPP
