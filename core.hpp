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

// An operand as parsed from assembly
struct Operand {
    enum Type { IMM, REG, MEM };
    Type ty;             // Immediate, register, or memory
    int tag;             // Extra tag (for addressing modes)
    int bit;             // Operand size in bits (e.g., 8, 16, 32, 64)
    bool issegaddr;      // True if it uses a segment register (e.g. fs:[...])
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
string reg2string(Register reg) {
    switch (reg) {
    case RAX: return "rax";
    case RBX: return "rbx";
    case RCX: return "rcx";
    case RDX: return "rdx";
    case RSI: return "rsi";
    case RDI: return "rdi";
    case RSP: return "rsp";
    case RBP: return "rbp";

    case EAX: return "eax";
    case EBX: return "ebx";
    case ECX: return "ecx";
    case EDX: return "edx";
    case ESI: return "esi";
    case EDI: return "edi";
    case ESP: return "esp";
    case EBP: return "ebp";

    case ST0: return "st0";
    case ST1: return "st1";
    case ST2: return "st2";
    case ST3: return "st3";
    case ST4: return "st4";
    case ST5: return "st5";

    case CS:  return "cs";
    case DS:  return "ds";
    case ES:  return "es";
    case FS:  return "fs";
    case GS:  return "gs";
    case SS:  return "ss";

    case UNK: return "unk";
    default:  return "unknown";
    }
}

#endif // CORE_HPP
