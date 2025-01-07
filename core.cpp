#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

#include "core.hpp"

typedef uint64_t ADDR64;          // Update address size to 64-bit
typedef pair<ADDR64, ADDR64> AddrRange;

// Implementation of Parameter operators
bool Parameter::operator==(const Parameter& other) {
    if (ty == other.ty) {
        switch (ty) {
        case IMM:
            return idx == other.idx;
        case REG:
            return reg == other.reg && idx == other.idx;
        case MEM:
            return idx == other.idx;
        default:
            return false;
        }
    }
    return false;
}

bool Parameter::operator<(const Parameter& other) const {
    if (ty < other.ty)
        return true;
    else if (ty > other.ty)
        return false;
    else {
        switch (ty) {
        case IMM:
            return idx < other.idx;
        case REG:
            return (reg < other.reg) || (reg == other.reg && idx < other.idx);
        case MEM:
            return idx < other.idx;
        default:
            return true;
        }
    }
}

bool Parameter::isIMM() {
    return ty == IMM;
}

// Helper: convert Register enum to string
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
    default:  return "unknown";
    }
}

// Show a Parameter in console
void Parameter::show() const {
    if (ty == IMM) {
        // Print as 64-bit hex
        printf("(IMM 0x%llx) ", (unsigned long long)idx);
    } 
    else if (ty == REG) {
        cout << "(REG " << reg2string(reg) << ") ";
    } 
    else if (ty == MEM) {
        // Print as 64-bit hex
        printf("(MEM 0x%llx) ", (unsigned long long)idx);
    } 
    else {
        cout << "Parameter show() error: unknown src type." << endl;
    }
}

// Helper functions to identify register widths
bool isReg64(string reg) {
    static const set<string> regs64 = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    return regs64.find(reg) != regs64.end();
}

bool isReg32(string reg) {
    return reg == "eax" || reg == "ebx" || reg == "ecx" || reg == "edx" ||
           reg == "esi" || reg == "edi" || reg == "esp" || reg == "ebp";
}

bool isReg16(string reg) {
    return reg == "ax" || reg == "bx" || reg == "cx" || reg == "dx" ||
           reg == "si" || reg == "di" || reg == "bp";
}

bool isReg8(string reg) {
    return reg == "al" || reg == "ah" || reg == "bl" || reg == "bh" ||
           reg == "cl" || reg == "ch" || reg == "dl" || reg == "dh";
}

// Return Register enum + fill 'idx' with sub-parts (for partial regs)
Register getRegParameter(string regname, vector<int>& idx) {
    // Remove R8-R15 handling
    // if (regname.substr(0,2) == "r8") return R8;
    // if (regname.substr(0,2) == "r9") return R9;
    // if (regname.substr(0,3) == "r10") return R10;
    // if (regname.substr(0,3) == "r11") return R11;
    // if (regname.substr(0,3) == "r12") return R12;
    // if (regname.substr(0,3) == "r13") return R13;
    // if (regname.substr(0,3) == "r14") return R14;
    // if (regname.substr(0,3) == "r15") return R15;
    
    if (isReg64(regname)) {
        idx.push_back(0);
        idx.push_back(1);
        idx.push_back(2);
        idx.push_back(3);
        if (regname == "rax") return RAX;
        if (regname == "rbx") return RBX;
        if (regname == "rcx") return RCX;
        if (regname == "rdx") return RDX;
        if (regname == "rsi") return RSI;
        if (regname == "rdi") return RDI;
        if (regname == "rsp") return RSP;
        if (regname == "rbp") return RBP;
        cout << "Unknown 64-bit reg: " << regname << endl;
        return UNK;
    } 
    else if (isReg32(regname)) {
        idx.push_back(0);
        idx.push_back(1);
        idx.push_back(2);
        idx.push_back(3);
        if (regname == "eax") return EAX;
        if (regname == "ebx") return EBX;
        if (regname == "ecx") return ECX;
        if (regname == "edx") return EDX;
        if (regname == "esi") return ESI;
        if (regname == "edi") return EDI;
        if (regname == "esp") return ESP;
        if (regname == "ebp") return EBP;
        cout << "Unknown 32-bit reg: " << regname << endl;
        return UNK;
    } 
    else if (isReg16(regname)) {
        idx.push_back(0);
        idx.push_back(1);
        if (regname == "ax") return EAX;
        if (regname == "bx") return EBX;
        if (regname == "cx") return ECX;
        if (regname == "dx") return EDX;
        if (regname == "si") return ESI;
        if (regname == "di") return EDI;
        if (regname == "bp") return EBP;
        cout << "Unknown 16-bit reg: " << regname << endl;
        return UNK;
    } 
    else if (isReg8(regname)) {
        idx.push_back(0);
        if (regname == "al") return EAX;
        if (regname == "bl") return EBX;
        if (regname == "cl") return ECX;
        if (regname == "dl") return EDX;
        cout << "Unknown 8-bit reg: " << regname << endl;
        return UNK;
    } 
    else {
        cout << "Unknown reg: " << regname << endl;
        return UNK;
    }
}

// Implementation of Inst's helper methods
void Inst::addsrc(Parameter::Type t, string s) {
    if (t == Parameter::IMM) {
        Parameter p;
        p.ty = t;
        p.idx = stoull(s, 0, 16);  // Parse as 64-bit
        src.push_back(p);
    } 
    else if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        for (int i = 0; i < (int)v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            src.push_back(p);
        }
    } 
    else {
        cout << "addsrc error!" << endl;
    }
}

void Inst::addsrc(Parameter::Type t, AddrRange a) {
    for (ADDR64 i = a.first; i <= a.second; ++i) {
        Parameter p;
        p.ty = t;
        p.idx = i;
        src.push_back(p);
    }
}

void Inst::adddst(Parameter::Type t, string s) {
    if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        for (int i = 0; i < (int)v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            dst.push_back(p);
        }
    } 
    else {
        cout << "adddst error!" << endl;
    }
}

void Inst::adddst(Parameter::Type t, AddrRange a) {
    for (ADDR64 i = a.first; i <= a.second; ++i) {
        Parameter p;
        p.ty = t;
        p.idx = i;
        dst.push_back(p);
    }
}

void Inst::addsrc2(Parameter::Type t, string s) {
    if (t == Parameter::IMM) {
        Parameter p;
        p.ty = t;
        p.idx = stoull(s, 0, 16);
        src2.push_back(p);
    } 
    else if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        for (int i = 0; i < (int)v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            src2.push_back(p);
        }
    } 
    else {
        cout << "addsrc2 error!" << endl;
    }
}

void Inst::addsrc2(Parameter::Type t, AddrRange a) {
    for (ADDR64 i = a.first; i <= a.second; ++i) {
        Parameter p;
        p.ty = t;
        p.idx = i;
        src2.push_back(p);
    }
}

void Inst::adddst2(Parameter::Type t, string s) {
    if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        for (int i = 0; i < (int)v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            dst2.push_back(p);
        }
    } 
    else {
        cout << "adddst2 error!" << endl;
    }
}

void Inst::adddst2(Parameter::Type t, AddrRange a) {
    for (ADDR64 i = a.first; i <= a.second; ++i) {
        Parameter p;
        p.ty = t;
        p.idx = i;
        dst2.push_back(p);
    }
}
