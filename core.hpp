// core.hpp
#ifndef CORE_HPP
#define CORE_HPP

#include <cstdint>
#include <string>
#include <utility>   // for std::pair
#include <map>
#include <vector>
#include <algorithm> // for std::transform

using std::string;
using std::pair;
using std::map;
using std::vector;

// 64-bit address type
typedef uint64_t ADDR64;

// Address range (start, end)
typedef pair<ADDR64, ADDR64> AddrRange;

// Enum of registers supporting various sizes and types
enum Register {
    // 64-bit General Purpose Registers
    RAX, RBX, RCX, RDX,
    RSI, RDI, RSP, RBP,
    R8, R9, R10, R11,
    R12, R13, R14, R15,

    // 32-bit General Purpose Registers
    EAX, EBX, ECX, EDX,
    ESI, EDI, ESP, EBP,
    R8D, R9D, R10D, R11D,
    R12D, R13D, R14D, R15D,

    // 16-bit General Purpose Registers
    AX, BX, CX, DX,
    SI, DI, BP, SP,
    R8W, R9W, R10W, R11W,
    R12W, R13W, R14W, R15W,

    // 8-bit General Purpose Registers
    AL, BL, CL, DL,
    AH, BH, CH, DH,
    SPL, BPL, SIL, DIL,
    R8B, R9B, R10B, R11B,
    R12B, R13B, R14B, R15B,

    // FPU Registers
    ST0, ST1, ST2, ST3,
    ST4, ST5,

    // Segment Registers
    CS, DS, ES, FS, GS, SS,

    // Vector Registers
    XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
    XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
    YMM0, YMM1, YMM2, YMM3, YMM4, YMM5, YMM6, YMM7,
    YMM8, YMM9, YMM10, YMM11, YMM12, YMM13, YMM14, YMM15,
    ZMM0, ZMM1, ZMM2, ZMM3, ZMM4, ZMM5, ZMM6, ZMM7,
    ZMM8, ZMM9, ZMM10, ZMM11, ZMM12, ZMM13, ZMM14, ZMM15,

    UNK                         // Unknown register
};

// Utility function to convert an enum Register to its string name
inline string reg2string(Register reg) {
    switch (reg) {
    case RAX: return "rax";
    case RBX: return "rbx";
    case RCX: return "rcx";
    case RDX: return "rdx";
    case RSI: return "rsi";
    case RDI: return "rdi";
    case RSP: return "rsp";
    case RBP: return "rbp";
    case R8:  return "r8";
    case R9:  return "r9";
    case R10: return "r10";
    case R11: return "r11";
    case R12: return "r12";
    case R13: return "r13";
    case R14: return "r14";
    case R15: return "r15";

    case EAX: return "eax";
    case EBX: return "ebx";
    case ECX: return "ecx";
    case EDX: return "edx";
    case ESI: return "esi";
    case EDI: return "edi";
    case ESP: return "esp";
    case EBP: return "ebp";
    case R8D: return "r8d";
    case R9D: return "r9d";
    case R10D: return "r10d";
    case R11D: return "r11d";
    case R12D: return "r12d";
    case R13D: return "r13d";
    case R14D: return "r14d";
    case R15D: return "r15d";

    case AX: return "ax";
    case BX: return "bx";
    case CX: return "cx";
    case DX: return "dx";
    case SI: return "si";
    case DI: return "di";
    case BP: return "bp";
    case SP: return "sp";
    case R8W: return "r8w";
    case R9W: return "r9w";
    case R10W: return "r10w";
    case R11W: return "r11w";
    case R12W: return "r12w";
    case R13W: return "r13w";
    case R14W: return "r14w";
    case R15W: return "r15w";

    case AL: return "al";
    case BL: return "bl";
    case CL: return "cl";
    case DL: return "dl";
    case AH: return "ah";
    case BH: return "bh";
    case CH: return "ch";
    case DH: return "dh";
    case SPL: return "spl";
    case BPL: return "bpl";
    case SIL: return "sil";
    case DIL: return "dil";
    case R8B: return "r8b";
    case R9B: return "r9b";
    case R10B: return "r10b";
    case R11B: return "r11b";
    case R12B: return "r12b";
    case R13B: return "r13b";
    case R14B: return "r14b";
    case R15B: return "r15b";

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

    case XMM0:  return "xmm0";
    case XMM1:  return "xmm1";
    case XMM2:  return "xmm2";
    case XMM3:  return "xmm3";
    case XMM4:  return "xmm4";
    case XMM5:  return "xmm5";
    case XMM6:  return "xmm6";
    case XMM7:  return "xmm7";
    case XMM8:  return "xmm8";
    case XMM9:  return "xmm9";
    case XMM10: return "xmm10";
    case XMM11: return "xmm11";
    case XMM12: return "xmm12";
    case XMM13: return "xmm13";
    case XMM14: return "xmm14";
    case XMM15: return "xmm15";

    case YMM0:  return "ymm0";
    case YMM1:  return "ymm1";
    case YMM2:  return "ymm2";
    case YMM3:  return "ymm3";
    case YMM4:  return "ymm4";
    case YMM5:  return "ymm5";
    case YMM6:  return "ymm6";
    case YMM7:  return "ymm7";
    case YMM8:  return "ymm8";
    case YMM9:  return "ymm9";
    case YMM10: return "ymm10";
    case YMM11: return "ymm11";
    case YMM12: return "ymm12";
    case YMM13: return "ymm13";
    case YMM14: return "ymm14";
    case YMM15: return "ymm15";

    case ZMM0:  return "zmm0";
    case ZMM1:  return "zmm1";
    case ZMM2:  return "zmm2";
    case ZMM3:  return "zmm3";
    case ZMM4:  return "zmm4";
    case ZMM5:  return "zmm5";
    case ZMM6:  return "zmm6";
    case ZMM7:  return "zmm7";
    case ZMM8:  return "zmm8";
    case ZMM9:  return "zmm9";
    case ZMM10: return "zmm10";
    case ZMM11: return "zmm11";
    case ZMM12: return "zmm12";
    case ZMM13: return "zmm13";
    case ZMM14: return "zmm14";
    case ZMM15: return "zmm15";

    case UNK: return "unk";
    default:  return "unknown";
    }
}

// Utility function to convert a register string to its enum value
inline Register reg2enum(const string &regStr) {
    string reg = regStr;
    // Convert to lowercase for case-insensitive comparison
    std::transform(reg.begin(), reg.end(), reg.begin(), ::tolower);

    if (reg == "rax") return Register::RAX;
    if (reg == "rbx") return Register::RBX;
    if (reg == "rcx") return Register::RCX;
    if (reg == "rdx") return Register::RDX;
    if (reg == "rsi") return Register::RSI;
    if (reg == "rdi") return Register::RDI;
    if (reg == "rsp") return Register::RSP;
    if (reg == "rbp") return Register::RBP;
    if (reg == "r8")  return Register::R8;
    if (reg == "r9")  return Register::R9;
    if (reg == "r10") return Register::R10;
    if (reg == "r11") return Register::R11;
    if (reg == "r12") return Register::R12;
    if (reg == "r13") return Register::R13;
    if (reg == "r14") return Register::R14;
    if (reg == "r15") return Register::R15;

    if (reg == "eax") return Register::EAX;
    if (reg == "ebx") return Register::EBX;
    if (reg == "ecx") return Register::ECX;
    if (reg == "edx") return Register::EDX;
    if (reg == "esi") return Register::ESI;
    if (reg == "edi") return Register::EDI;
    if (reg == "esp") return Register::ESP;
    if (reg == "ebp") return Register::EBP;
    if (reg == "r8d")  return Register::R8D;
    if (reg == "r9d")  return Register::R9D;
    if (reg == "r10d") return Register::R10D;
    if (reg == "r11d") return Register::R11D;
    if (reg == "r12d") return Register::R12D;
    if (reg == "r13d") return Register::R13D;
    if (reg == "r14d") return Register::R14D;
    if (reg == "r15d") return Register::R15D;

    if (reg == "ax") return Register::AX;
    if (reg == "bx") return Register::BX;
    if (reg == "cx") return Register::CX;
    if (reg == "dx") return Register::DX;
    if (reg == "si") return Register::SI;
    if (reg == "di") return Register::DI;
    if (reg == "bp") return Register::BP;
    if (reg == "sp") return Register::SP;
    if (reg == "r8w")  return Register::R8W;
    if (reg == "r9w")  return Register::R9W;
    if (reg == "r10w") return Register::R10W;
    if (reg == "r11w") return Register::R11W;
    if (reg == "r12w") return Register::R12W;
    if (reg == "r13w") return Register::R13W;
    if (reg == "r14w") return Register::R14W;
    if (reg == "r15w") return Register::R15W;

    if (reg == "al") return Register::AL;
    if (reg == "bl") return Register::BL;
    if (reg == "cl") return Register::CL;
    if (reg == "dl") return Register::DL;
    if (reg == "ah") return Register::AH;
    if (reg == "bh") return Register::BH;
    if (reg == "ch") return Register::CH;
    if (reg == "dh") return Register::DH;
    if (reg == "spl") return Register::SPL;
    if (reg == "bpl") return Register::BPL;
    if (reg == "sil") return Register::SIL;
    if (reg == "dil") return Register::DIL;
    if (reg == "r8b")  return Register::R8B;
    if (reg == "r9b")  return Register::R9B;
    if (reg == "r10b") return Register::R10B;
    if (reg == "r11b") return Register::R11B;
    if (reg == "r12b") return Register::R12B;
    if (reg == "r13b") return Register::R13B;
    if (reg == "r14b") return Register::R14B;
    if (reg == "r15b") return Register::R15B;

    if (reg == "st0") return Register::ST0;
    if (reg == "st1") return Register::ST1;
    if (reg == "st2") return Register::ST2;
    if (reg == "st3") return Register::ST3;
    if (reg == "st4") return Register::ST4;
    if (reg == "st5") return Register::ST5;

    if (reg == "cs")  return Register::CS;
    if (reg == "ds")  return Register::DS;
    if (reg == "es")  return Register::ES;
    if (reg == "fs")  return Register::FS;
    if (reg == "gs")  return Register::GS;
    if (reg == "ss")  return Register::SS;

    // Vector Registers
    if (reg.substr(0, 3) == "xmm") {
        int num = std::stoi(reg.substr(3));
        if (num >= 0 && num <= 15) return static_cast<Register>(Register::XMM0 + num);
    }
    if (reg.substr(0, 3) == "ymm") {
        int num = std::stoi(reg.substr(3));
        if (num >= 0 && num <= 15) return static_cast<Register>(Register::YMM0 + num);
    }
    if (reg.substr(0, 3) == "zmm") {
        int num = std::stoi(reg.substr(3));
        if (num >= 0 && num <= 15) return static_cast<Register>(Register::ZMM0 + num);
    }

    return Register::UNK;
}

// An operand as parsed from assembly
struct Operand {
    enum Type { IMM, REG, MEM };
    Type ty;             // Immediate, register, or memory
    int tag;             // Extra tag (for addressing modes)
    int bit;             // Operand size in bits (e.g., 8, 16, 32, 64)
    bool issegaddr;      // True if it uses a segment register (e.g., fs:[...])
    string segreg;       // Segment register name if issegaddr == true
    string field[5];     // Fields for operand decoding (e.g., reg name, displacement, etc.)

    // Constructors
    Operand() : ty(IMM), tag(0), bit(0), issegaddr(false), segreg(""), field{"", "", "", "", ""} {}
    Operand(Type type, int bitSize, const string& fld0)
        : ty(type), tag(0), bit(bitSize), issegaddr(false), segreg(""), field{fld0, "", "", "", ""} {}
};

// A parameter struct for finer-grained operand info
struct Parameter {
    enum Type { IMM, REG, MEM };
    Type ty;
    Register reg;   // Which register if type == REG
    ADDR64 idx;     // If MEM, this could be the address. If IMM, the immediate value

    // Operators
    bool operator==(const Parameter& other) const;
    bool operator<(const Parameter& other) const;
    bool isIMM() const;
    void show() const;
};

// A single instruction in the trace
struct Inst {
    int id;                // Unique instruction ID
    string addr;           // Instruction address (string form)
    uint64_t addrn;        // Instruction address as numeric form
    string assembly;       // Full assembly text
    int opc;               // Opcode (numeric)
    string opcstr;         // Opcode string
    vector<string> oprs;   // Raw operands (string)
    int oprnum;            // Number of operands
    Operand oprd[3];       // Parsed operand structures (by value)
    ADDR64 ctxreg[8] = {0};      // Context registers (64-bit)
    ADDR64 raddr = 0;          // Memory read address
    ADDR64 waddr = 0;          // Memory write address

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

#endif // CORE_HPP
