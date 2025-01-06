typedef uint64_t ADDR64;
typedef pair<ADDR64, ADDR64> AddrRange;

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
struct Operand {
    enum Type { IMM, REG, MEM };
    Type ty;
    int tag;
    int bit;                    // Operand size (e.g., 8, 16, 32, 64)
    bool issegaddr;
    string segreg;              // Segment register for memory access, e.g., fs:[0x1]
    string field[5];            // Fields for operand decoding

    Operand() : bit(0), issegaddr(false) {}
};

// Parameter for fine-grained operand definition
// Updated for 64-bit architecture
struct Parameter {
    enum Type { IMM, REG, MEM };
    Type ty;
    Register reg;               // Register name
    ADDR64 idx;                 // Index value for memory or immediate value

    bool operator==(const Parameter &other);
    bool operator<(const Parameter &other) const;
    bool isIMM();
    void show() const;
};

struct Inst {
    int id;                     // Unique instruction ID
    string addr;                // Instruction address (string)
    uint64_t addrn;             // Instruction address (numeric)
    string assembly;            // Assembly code, including opcode and operands
    int opc;                    // Opcode (numeric)
    string opcstr;              // Opcode (string)
    vector<string> oprs;        // Operands (string representation)
    int oprnum;                 // Number of operands
    Operand *oprd[3];           // Parsed operands
    ADDR64 ctxreg[8];           // Context registers (64-bit)
    ADDR64 raddr;               // Read memory address
    ADDR64 waddr;               // Write memory address

    vector<Parameter> src;      // Source parameters
    vector<Parameter> dst;      // Destination parameters
    vector<Parameter> src2;     // Source parameters for extra dependencies (e.g., `xchg`)
    vector<Parameter> dst2;     // Destination parameters for extra dependencies

    void addsrc(Parameter::Type t, string s);
    void addsrc(Parameter::Type t, AddrRange a);
    void adddst(Parameter::Type t, string s);
    void adddst(Parameter::Type t, AddrRange a);
    void addsrc2(Parameter::Type t, string s);
    void addsrc2(Parameter::Type t, AddrRange a);
    void adddst2(Parameter::Type t, string s);
    void adddst2(Parameter::Type t, AddrRange a);
};

typedef pair<map<int, int>, map<int, int>> FullMap;

string reg2string(Register reg);

// Implementation of reg2string for 64-bit support
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

    case CS: return "cs";
    case DS: return "ds";
    case ES: return "es";
    case FS: return "fs";
    case GS: return "gs";
    case SS: return "ss";

    case UNK: return "unk";
    default: return "unknown";
    }
}
