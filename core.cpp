#include <iostream>
#include <string>
#include <vector>
#include <map>

using namespace std;

#include "new_core.hpp"

typedef uint64_t ADDR64; // Update address size to 64-bit
typedef pair<ADDR64, ADDR64> AddrRange;

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
    default: return "unknown";
    }
}

void Parameter::show() const {
    if (ty == IMM) {
        printf("(IMM 0x%llx) ", idx); // Use %llx for 64-bit
    } else if (ty == REG) {
        cout << "(REG " << reg2string(reg) << ") ";
    } else if (ty == MEM) {
        printf("(MEM 0x%llx) ", idx); // Use %llx for 64-bit
    } else {
        cout << "Parameter show() error: unknown src type." << endl;
    }
}

bool isReg64(string reg) {
    return reg == "rax" || reg == "rbx" || reg == "rcx" || reg == "rdx" ||
           reg == "rsi" || reg == "rdi" || reg == "rsp" || reg == "rbp";
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

Register getRegParameter(string regname, vector<int>& idx) {
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
    } else if (isReg32(regname)) {
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
    } else if (isReg16(regname)) {
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
    } else if (isReg8(regname)) {
        idx.push_back(0);

        if (regname == "al") return EAX;
        if (regname == "bl") return EBX;
        if (regname == "cl") return ECX;
        if (regname == "dl") return EDX;
        cout << "Unknown 8-bit reg: " << regname << endl;
        return UNK;
    } else {
        cout << "Unknown reg: " << regname << endl;
        return UNK;
    }
}

void Inst::addsrc(Parameter::Type t, string s) {
    if (t == Parameter::IMM) {
        Parameter p;
        p.ty = t;
        p.idx = stoull(s, 0, 16); // Use stoull for 64-bit values
        src.push_back(p);
    } else if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        for (int i = 0; i < v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            src.push_back(p);
        }
    } else {
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
        for (int i = 0; i < v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            dst.push_back(p);
        }
    } else {
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
        p.idx = stoull(s, 0, 16); // Use stoull for 64-bit values
        src2.push_back(p);
    } else if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        for (int i = 0; i < v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            src2.push_back(p);
        }
    } else {
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
        for (int i = 0; i < v.size(); ++i) {
            Parameter p;
            p.ty = t;
            p.reg = r;
            p.idx = v[i];
            dst2.push_back(p);
        }
    } else {
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
