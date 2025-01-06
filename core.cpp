// core.cpp
#include "core.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm> // for std::transform
#include <cstdio>    // for printf

using namespace std;

// Implementation of Parameter operators
bool Parameter::operator==(const Parameter& other) const {
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

bool Parameter::isIMM() const {
    return ty == IMM;
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
bool isReg64(const string& reg) {
    return reg == "rax" || reg == "rbx" || reg == "rcx" || reg == "rdx" ||
           reg == "rsi" || reg == "rdi" || reg == "rsp" || reg == "rbp" ||
           reg == "r8" || reg == "r9" || reg == "r10" || reg == "r11" ||
           reg == "r12" || reg == "r13" || reg == "r14" || reg == "r15";
}

bool isReg32(const string& reg) {
    return reg == "eax" || reg == "ebx" || reg == "ecx" || reg == "edx" ||
           reg == "esi" || reg == "edi" || reg == "esp" || reg == "ebp" ||
           reg == "r8d" || reg == "r9d" || reg == "r10d" || reg == "r11d" ||
           reg == "r12d" || reg == "r13d" || reg == "r14d" || reg == "r15d";
}

bool isReg16(const string& reg) {
    return reg == "ax" || reg == "bx" || reg == "cx" || reg == "dx" ||
           reg == "si" || reg == "di" || reg == "bp" || reg == "sp" ||
           reg == "r8w" || reg == "r9w" || reg == "r10w" || reg == "r11w" ||
           reg == "r12w" || reg == "r13w" || reg == "r14w" || reg == "r15w";
}

bool isReg8(const string& reg) {
    return reg == "al" || reg == "ah" || reg == "bl" || reg == "bh" ||
           reg == "cl" || reg == "ch" || reg == "dl" || reg == "dh" ||
           reg == "spl" || reg == "bpl" || reg == "sil" || reg == "dil" ||
           reg == "r8b" || reg == "r9b" || reg == "r10b" || reg == "r11b" ||
           reg == "r12b" || reg == "r13b" || reg == "r14b" || reg == "r15b";
}

// Return Register enum + fill 'idx' with sub-parts (for partial regs)
Register getRegParameter(const string& regname, vector<int>& idx) {
    if (isReg64(regname)) {
        // For 64-bit registers, idx can represent byte positions or other attributes if needed
        idx.push_back(0);
        idx.push_back(1);
        idx.push_back(2);
        idx.push_back(3);
        if (regname == "rax") return Register::RAX;
        if (regname == "rbx") return Register::RBX;
        if (regname == "rcx") return Register::RCX;
        if (regname == "rdx") return Register::RDX;
        if (regname == "rsi") return Register::RSI;
        if (regname == "rdi") return Register::RDI;
        if (regname == "rsp") return Register::RSP;
        if (regname == "rbp") return Register::RBP;
        if (regname == "r8")  return Register::R8;
        if (regname == "r9")  return Register::R9;
        if (regname == "r10") return Register::R10;
        if (regname == "r11") return Register::R11;
        if (regname == "r12") return Register::R12;
        if (regname == "r13") return Register::R13;
        if (regname == "r14") return Register::R14;
        if (regname == "r15") return Register::R15;
        cout << "Unknown 64-bit reg: " << regname << endl;
        return Register::UNK;
    } 
    else if (isReg32(regname)) {
        idx.push_back(0);
        idx.push_back(1);
        idx.push_back(2);
        idx.push_back(3);
        if (regname == "eax") return Register::EAX;
        if (regname == "ebx") return Register::EBX;
        if (regname == "ecx") return Register::ECX;
        if (regname == "edx") return Register::EDX;
        if (regname == "esi") return Register::ESI;
        if (regname == "edi") return Register::EDI;
        if (regname == "esp") return Register::ESP;
        if (regname == "ebp") return Register::EBP;
        if (regname == "r8d")  return Register::R8D;
        if (regname == "r9d")  return Register::R9D;
        if (regname == "r10d") return Register::R10D;
        if (regname == "r11d") return Register::R11D;
        if (regname == "r12d") return Register::R12D;
        if (regname == "r13d") return Register::R13D;
        if (regname == "r14d") return Register::R14D;
        if (regname == "r15d") return Register::R15D;
        cout << "Unknown 32-bit reg: " << regname << endl;
        return Register::UNK;
    } 
    else if (isReg16(regname)) {
        idx.push_back(0);
        idx.push_back(1);
        if (regname == "ax") return Register::AX;
        if (regname == "bx") return Register::BX;
        if (regname == "cx") return Register::CX;
        if (regname == "dx") return Register::DX;
        if (regname == "si") return Register::SI;
        if (regname == "di") return Register::DI;
        if (regname == "bp") return Register::BP;
        if (regname == "sp") return Register::SP;
        if (regname == "r8w")  return Register::R8W;
        if (regname == "r9w")  return Register::R9W;
        if (regname == "r10w") return Register::R10W;
        if (regname == "r11w") return Register::R11W;
        if (regname == "r12w") return Register::R12W;
        if (regname == "r13w") return Register::R13W;
        if (regname == "r14w") return Register::R14W;
        if (regname == "r15w") return Register::R15W;
        cout << "Unknown 16-bit reg: " << regname << endl;
        return Register::UNK;
    } 
    else if (isReg8(regname)) {
        idx.push_back(0);
        if (regname == "al") return Register::AL;
        if (regname == "bl") return Register::BL;
        if (regname == "cl") return Register::CL;
        if (regname == "dl") return Register::DL;
        if (regname == "ah") return Register::AH;
        if (regname == "bh") return Register::BH;
        if (regname == "ch") return Register::CH;
        if (regname == "dh") return Register::DH;
        if (regname == "spl") return Register::SPL;
        if (regname == "bpl") return Register::BPL;
        if (regname == "sil") return Register::SIL;
        if (regname == "dil") return Register::DIL;
        if (regname == "r8b")  return Register::R8B;
        if (regname == "r9b")  return Register::R9B;
        if (regname == "r10b") return Register::R10B;
        if (regname == "r11b") return Register::R11B;
        if (regname == "r12b") return Register::R12B;
        if (regname == "r13b") return Register::R13B;
        if (regname == "r14b") return Register::R14B;
        if (regname == "r15b") return Register::R15B;
        cout << "Unknown 8-bit reg: " << regname << endl;
        return Register::UNK;
    } 
    else {
        cout << "Unknown reg: " << regname << endl;
        return Register::UNK;
    }
}

// Implementation of Inst's helper methods
void Inst::addsrc(Parameter::Type t, string s) {
    if (t == Parameter::IMM) {
        try {
            Parameter p;
            p.ty = t;
            p.idx = stoull(s, 0, 16);  // Parse as 64-bit
            src.push_back(p);
        } catch (const std::invalid_argument& e) {
            std::cerr << "[addsrc] Invalid IMM value: " << s << endl;
        } catch (const std::out_of_range& e) {
            std::cerr << "[addsrc] IMM value out of range: " << s << endl;
        }
    } 
    else if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        if (r != Register::UNK) {
            for (int i = 0; i < (int)v.size(); ++i) {
                Parameter p;
                p.ty = t;
                p.reg = r;
                p.idx = v[i];
                src.push_back(p);
            }
        } else {
            std::cerr << "[addsrc] Unknown REG operand: " << s << endl;
        }
    } 
    else {
        std::cerr << "addsrc error! Unknown type." << endl;
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
        if (r != Register::UNK) {
            for (int i = 0; i < (int)v.size(); ++i) {
                Parameter p;
                p.ty = t;
                p.reg = r;
                p.idx = v[i];
                dst.push_back(p);
            }
        } else {
            std::cerr << "[adddst] Unknown REG operand: " << s << endl;
        }
    } 
    else {
        std::cerr << "adddst error! Unsupported type." << endl;
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
        try {
            Parameter p;
            p.ty = t;
            p.idx = stoull(s, 0, 16);
            src2.push_back(p);
        } catch (const std::invalid_argument& e) {
            std::cerr << "[addsrc2] Invalid IMM value: " << s << endl;
        } catch (const std::out_of_range& e) {
            std::cerr << "[addsrc2] IMM value out of range: " << s << endl;
        }
    } 
    else if (t == Parameter::REG) {
        vector<int> v;
        Register r = getRegParameter(s, v);
        if (r != Register::UNK) {
            for (int i = 0; i < (int)v.size(); ++i) {
                Parameter p;
                p.ty = t;
                p.reg = r;
                p.idx = v[i];
                src2.push_back(p);
            }
        } else {
            std::cerr << "[addsrc2] Unknown REG operand: " << s << endl;
        }
    } 
    else {
        std::cerr << "addsrc2 error! Unknown type." << endl;
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
        if (r != Register::UNK) {
            for (int i = 0; i < (int)v.size(); ++i) {
                Parameter p;
                p.ty = t;
                p.reg = r;
                p.idx = v[i];
                dst2.push_back(p);
            }
        } else {
            std::cerr << "[adddst2] Unknown REG operand: " << s << endl;
        }
    } 
    else {
        std::cerr << "adddst2 error! Unsupported type." << endl;
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
