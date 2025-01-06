#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <map>
#include <stack>
#include <vector>
#include <set>
#include <cstdint>     // for uint64_t
#include <algorithm>   // for std::next, std::distance, etc.

using namespace std;

#include "core.hpp"    // Ensure Parameter::idx and Inst::raddr/waddr are uint64_t
#include "parser.hpp"  // parseTrace(...), parseOperand(...)

// Global instruction list
list<Inst> instlist;

/*
 * A set of instructions that do NOT affect data dependencies,
 * so we skip them when building parameters.
 */
const set<string> skipinst = {
    "test","jmp","jz","jbe","jo","jno","js","jns","je","jz","jne","jnz","jb",
    "jnae","jc","jnb","jae","jnc","jbe","jna","ja","jnbe","jl","jnge","jge",
    "jnl","jle","jng","jg","jnle","jp","jpe","jnp","jpo","jcxz","jecxz","ret",
    "cmp","call"
};

/*
 * Build fine-grained parameters (src/dst) for each instruction in L.
 * 
 * This uses operand info (op0.ty, op0.field[0], etc.) plus
 * read/write addresses (raddr/waddr) to figure out what's being read/written.
 */
int buildParameter(list<Inst> &L)
{
    for (auto &ins : L) {
        // If the opcode is in skipinst, do nothing for it
        if (skipinst.find(ins.opcstr) != skipinst.end()) {
            continue;
        }

        switch (ins.oprnum) {
        case 0:
            // No operands => nothing to do
            break;

        case 1:
        {
            Operand &op0 = ins.oprd[0];
            int nbyte = 0;

            if (ins.opcstr == "push") {
                // On a 64-bit system, pushing is typically 8 bytes
                nbyte = 8;

                if (op0.ty == Operand::IMM) {
                    // Immediate value is pushed onto the stack
                    Parameter src_param;
                    src_param.ty = Parameter::IMM;
                    try {
                        src_param.idx = stoull(op0.field[0], nullptr, 16);
                    }
                    catch (const exception& e) {
                        cerr << "[push error] Invalid immediate value: " << op0.field[0] << endl;
                        return 1;
                    }
                    ins.addsrc(src_param);

                    // The stack is being written to (memory)
                    AddrRange ar(ins.waddr, ins.waddr + nbyte - 1);
                    Parameter dst_param;
                    dst_param.ty = Parameter::MEM;
                    dst_param.idx = ar.first; // Starting address
                    ins.adddst(dst_param);
                }
                else if (op0.ty == Operand::REG) {
                    // Register is pushed onto the stack
                    Parameter src_param;
                    src_param.ty = Parameter::REG;
                    src_param.reg = reg2enum(op0.field[0]);
                    ins.addsrc(src_param);

                    // The stack is being written to (memory)
                    AddrRange ar(ins.waddr, ins.waddr + nbyte - 1);
                    Parameter dst_param;
                    dst_param.ty = Parameter::MEM;
                    dst_param.idx = ar.first;
                    ins.adddst(dst_param);
                }
                else if (op0.ty == Operand::MEM) {
                    // Pushing a memory location onto the stack
                    nbyte = op0.bit / 8;
                    if (nbyte == 0) nbyte = 8;  // Fallback
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src_param;
                    src_param.ty = Parameter::MEM;
                    src_param.idx = rar.first;
                    ins.addsrc(src_param);

                    // The stack is being written to (memory)
                    AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                    Parameter dst_param;
                    dst_param.ty = Parameter::MEM;
                    dst_param.idx = war.first;
                    ins.adddst(dst_param);
                }
                else {
                    cerr << "[push error] Unknown operand type for op0!\n";
                    return 1;
                }
            }
            else if (ins.opcstr == "pop") {
                // On 64-bit, pop also fetches 8 bytes
                nbyte = 8;

                if (op0.ty == Operand::REG) {
                    // Pop into a register: read from stack (memory), write to register
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src_param;
                    src_param.ty = Parameter::MEM;
                    src_param.idx = rar.first;
                    ins.addsrc(src_param);

                    Parameter dst_param;
                    dst_param.ty = Parameter::REG;
                    dst_param.reg = reg2enum(op0.field[0]);
                    ins.adddst(dst_param);
                }
                else if (op0.ty == Operand::MEM) {
                    // Pop into memory: read from stack (memory), write to memory (memory)
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src_param;
                    src_param.ty = Parameter::MEM;
                    src_param.idx = rar.first;
                    ins.addsrc(src_param);

                    AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                    Parameter dst_param;
                    dst_param.ty = Parameter::MEM;
                    dst_param.idx = war.first;
                    ins.adddst(dst_param);
                }
                else {
                    cerr << "[pop error] op0 is not REG or MEM!\n";
                    return 1;
                }
            }
            else {
                // Single-operand instructions: inc [mem], dec reg, neg reg, etc.
                if (op0.ty == Operand::REG) {
                    // Register is both source and destination
                    Parameter src_param;
                    src_param.ty = Parameter::REG;
                    src_param.reg = reg2enum(op0.field[0]);
                    ins.addsrc(src_param);
                    
                    Parameter dst_param;
                    dst_param.ty = Parameter::REG;
                    dst_param.reg = reg2enum(op0.field[0]);
                    ins.adddst(dst_param);
                }
                else if (op0.ty == Operand::MEM) {
                    // Memory operand: read and write to memory
                    nbyte = op0.bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src_param;
                    src_param.ty = Parameter::MEM;
                    src_param.idx = rar.first;
                    ins.addsrc(src_param);
                    
                    AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                    Parameter dst_param;
                    dst_param.ty = Parameter::MEM;
                    dst_param.idx = war.first;
                    ins.adddst(dst_param);
                }
                else {
                    cerr << "[Error] Instruction " << ins.id
                         << ": Unknown 1-op form for " << ins.opcstr << endl;
                    return 1;
                }
            }
            break;
        }

        case 2:
        {
            Operand &op0 = ins.oprd[0];
            Operand &op1 = ins.oprd[1];
            int nbyte = 0;

            // Common instructions: mov, movzx, etc.
            if (ins.opcstr == "mov" || ins.opcstr == "movzx") {
                if (op0.ty == Operand::REG) {
                    if (op1.ty == Operand::IMM) {
                        // mov reg, imm
                        Parameter src_param;
                        src_param.ty = Parameter::IMM;
                        try {
                            src_param.idx = stoull(op1.field[0], nullptr, 16);
                        }
                        catch (const exception& e) {
                            cerr << "[mov error] Invalid immediate value: " << op1.field[0] << endl;
                            return 1;
                        }
                        ins.addsrc(src_param);
                        
                        Parameter dst_param;
                        dst_param.ty = Parameter::REG;
                        dst_param.reg = reg2enum(op0.field[0]);
                        ins.adddst(dst_param);
                    }
                    else if (op1.ty == Operand::REG) {
                        // mov reg, reg
                        Parameter src_param;
                        src_param.ty = Parameter::REG;
                        src_param.reg = reg2enum(op1.field[0]);
                        ins.addsrc(src_param);
                        
                        Parameter dst_param;
                        dst_param.ty = Parameter::REG;
                        dst_param.reg = reg2enum(op0.field[0]);
                        ins.adddst(dst_param);
                    }
                    else if (op1.ty == Operand::MEM) {
                        // mov reg, [mem]
                        nbyte = op1.bit / 8;
                        if (nbyte == 0) nbyte = 8;
                        AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                        
                        Parameter src_param;
                        src_param.ty = Parameter::MEM;
                        src_param.idx = rar.first;
                        ins.addsrc(src_param);
                        
                        Parameter dst_param;
                        dst_param.ty = Parameter::REG;
                        dst_param.reg = reg2enum(op0.field[0]);
                        ins.adddst(dst_param);
                    }
                    else {
                        cerr << "[mov error] op0=REG, op1 not IMM/REG/MEM\n";
                        return 1;
                    }
                }
                else if (op0.ty == Operand::MEM) {
                    // mov [mem], imm/reg
                    nbyte = op0.bit / 8;
                    if (nbyte == 0) nbyte = 8;

                    if (op1.ty == Operand::IMM) {
                        // mov [mem], imm
                        Parameter src_param;
                        src_param.ty = Parameter::IMM;
                        try {
                            src_param.idx = stoull(op1.field[0], nullptr, 16);
                        }
                        catch (const exception& e) {
                            cerr << "[mov error] Invalid immediate value: " << op1.field[0] << endl;
                            return 1;
                        }
                        ins.addsrc(src_param);
                        
                        AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                        Parameter dst_param;
                        dst_param.ty = Parameter::MEM;
                        dst_param.idx = war.first;
                        ins.adddst(dst_param);
                    }
                    else if (op1.ty == Operand::REG) {
                        // mov [mem], reg
                        Parameter src_param;
                        src_param.ty = Parameter::REG;
                        src_param.reg = reg2enum(op1.field[0]);
                        ins.addsrc(src_param);
                        
                        AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                        Parameter dst_param;
                        dst_param.ty = Parameter::MEM;
                        dst_param.idx = war.first;
                        ins.adddst(dst_param);
                    }
                    else {
                        cerr << "[mov error] op0=MEM, op1 not IMM/REG\n";
                        return 1;
                    }
                }
                else {
                    cerr << "[mov error] op0 is not MEM or REG\n";
                    return 1;
                }
            }
            else if (ins.opcstr == "lea") {
                // e.g. lea reg, [mem]
                if (op0.ty != Operand::REG || op1.ty != Operand::MEM) {
                    cerr << "[lea error] op0 must be REG, op1 must be MEM\n";
                    break;
                }
                // Handle different address tagging modes if necessary
                switch (op1.tag) {
                case 5: // e.g., rax+rbx*2
                    {
                        // Parameter::REG for base
                        Parameter src_param1;
                        src_param1.ty = Parameter::REG;
                        src_param1.reg = reg2enum(op1.field[0]);
                        ins.addsrc(src_param1);
                        
                        // Parameter::REG for index
                        Parameter src_param2;
                        src_param2.ty = Parameter::REG;
                        src_param2.reg = reg2enum(op1.field[1]);
                        ins.addsrc(src_param2);
                        
                        // The result goes into op0->REG as well.
                        Parameter dst_param;
                        dst_param.ty = Parameter::REG;
                        dst_param.reg = reg2enum(op0.field[0]);
                        ins.adddst(dst_param);
                    }
                    break;
                // Add other cases (tag 3,4,6,7) if needed
                default:
                    cerr << "[lea error] unhandled address tag: " << op1.tag << endl;
                    break;
                }
            }
            else if (ins.opcstr == "xchg") {
                // xchg => each operand is both src and dst
                // We'll store them as separate sets: main (src/dst) vs. second (src2/dst2)
                if (op1.ty == Operand::REG) {
                    // Operand1 is a register
                    Parameter src_param;
                    src_param.ty = Parameter::REG;
                    src_param.reg = reg2enum(op1.field[0]);
                    ins.addsrc(src_param);
                    
                    Parameter dst2_param;
                    dst2_param.ty = Parameter::REG;
                    dst2_param.reg = reg2enum(op1.field[0]);
                    ins.adddst2(dst2_param);
                }
                else if (op1.ty == Operand::MEM) {
                    // Operand1 is memory
                    nbyte = op1.bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange ar(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src_param;
                    src_param.ty = Parameter::MEM;
                    src_param.idx = ar.first;
                    ins.addsrc(src_param);
                    
                    Parameter dst2_param;
                    dst2_param.ty = Parameter::MEM;
                    dst2_param.idx = ar.first;
                    ins.adddst2(dst2_param);
                }
                else {
                    cerr << "[xchg error] op1 is not REG or MEM\n";
                    return 1;
                }

                if (op0.ty == Operand::REG) {
                    // Operand0 is a register
                    Parameter src2_param;
                    src2_param.ty = Parameter::REG;
                    src2_param.reg = reg2enum(op0.field[0]);
                    ins.addsrc2(src2_param);
                    
                    Parameter dst_param;
                    dst_param.ty = Parameter::REG;
                    dst_param.reg = reg2enum(op0.field[0]);
                    ins.adddst(dst_param);
                }
                else if (op0.ty == Operand::MEM) {
                    // Operand0 is memory
                    nbyte = op0.bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange ar(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src2_param;
                    src2_param.ty = Parameter::MEM;
                    src2_param.idx = ar.first;
                    ins.addsrc2(src2_param);
                    
                    Parameter dst_param;
                    dst_param.ty = Parameter::MEM;
                    dst_param.idx = ar.first;
                    ins.adddst(dst_param);
                }
                else {
                    cerr << "[xchg error] op0 is not REG or MEM\n";
                    return 1;
                }
            }
            else {
                // Generic 2-operand instruction (like add, sub, and, or, etc.)
                // 1) handle second operand as source
                if (op1.ty == Operand::IMM) {
                    Parameter src_param;
                    src_param.ty = Parameter::IMM;
                    try {
                        src_param.idx = stoull(op1.field[0], nullptr, 16);
                    }
                    catch (const exception& e) {
                        cerr << "[2-op error] Invalid immediate value: " << op1.field[0] << endl;
                        return 1;
                    }
                    ins.addsrc(src_param);
                }
                else if (op1.ty == Operand::REG) {
                    Parameter src_param;
                    src_param.ty = Parameter::REG;
                    src_param.reg = reg2enum(op1.field[0]);
                    ins.addsrc(src_param);
                }
                else if (op1.ty == Operand::MEM) {
                    nbyte = op1.bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange rar1(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src_param;
                    src_param.ty = Parameter::MEM;
                    src_param.idx = rar1.first;
                    ins.addsrc(src_param);
                }
                else {
                    cerr << "[2-op error] op1 not IMM/REG/MEM\n";
                    return 1;
                }

                // 2) handle first operand as source+dest
                if (op0.ty == Operand::REG) {
                    Parameter src_param;
                    src_param.ty = Parameter::REG;
                    src_param.reg = reg2enum(op0.field[0]);
                    ins.addsrc(src_param);
                    
                    Parameter dst_param;
                    dst_param.ty = Parameter::REG;
                    dst_param.reg = reg2enum(op0.field[0]);
                    ins.adddst(dst_param);
                }
                else if (op0.ty == Operand::MEM) {
                    nbyte = op0.bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange rar2(ins.raddr, ins.raddr + nbyte - 1);
                    
                    Parameter src_param;
                    src_param.ty = Parameter::MEM;
                    src_param.idx = rar2.first;
                    ins.addsrc(src_param);
                    
                    Parameter dst_param;
                    dst_param.ty = Parameter::MEM;
                    dst_param.idx = rar2.first;
                    ins.adddst(dst_param);
                }
                else {
                    cerr << "[2-op error] op0 not REG or MEM\n";
                    return 1;
                }
            }
            break;
        }

        case 3:
        {
            // Example: imul reg, reg, imm
            Operand &op0 = ins.oprd[0];
            Operand &op1 = ins.oprd[1];
            Operand &op2 = ins.oprd[2];

            if (ins.opcstr == "imul" &&
                op0.ty == Operand::REG &&
                op1.ty == Operand::REG &&
                op2.ty == Operand::IMM)
            {
                // imul reg, reg, imm
                Parameter src_param1;
                src_param1.ty = Parameter::IMM;
                try {
                    src_param1.idx = stoull(op2.field[0], nullptr, 16);
                }
                catch (const exception& e) {
                    cerr << "[imul error] Invalid immediate value: " << op2.field[0] << endl;
                    return 1;
                }
                ins.addsrc(src_param1);

                Parameter src_param2;
                src_param2.ty = Parameter::REG;
                src_param2.reg = reg2enum(op1.field[0]);
                ins.addsrc(src_param2);

                Parameter src_param3;
                src_param3.ty = Parameter::REG;
                src_param3.reg = reg2enum(op0.field[0]);
                ins.addsrc(src_param3);

                Parameter dst_param;
                dst_param.ty = Parameter::REG;
                dst_param.reg = reg2enum(op0.field[0]);
                ins.adddst(dst_param);
            }
            else {
                cerr << "[3-op error] Unrecognized pattern, e.g., 'imul reg, reg, imm'\n";
                return 1;
            }
            break;
        }

        default:
            cerr << "[error] Instruction has " << ins.oprnum 
                 << " operands (more than 3?) or unknown form\n";
            return 1;
        }
    }

    return 0;
}

/*
 * Print the instructions in L along with their src/dst parameters.
 */
void printInstParameter(const list<Inst> &L)
{
    for (const auto &ins : L) {
        cout << ins.id << " " << ins.addr << " " << ins.assembly << "\t";
        cout << "src: ";

        // Print src parameters
        for (const auto &p : ins.src) {
            if (p.ty == Parameter::IMM) {
                cout << "(IMM 0x" << std::hex << p.idx << std::dec << ") ";
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg) << ") ";
            }
            else if (p.ty == Parameter::MEM) {
                cout << "(MEM 0x" << std::hex << p.idx << std::dec << ") ";
            }
            else {
                cout << "[Error] Unknown src Parameter type! ";
            }
        }

        cout << ", dst: ";
        for (const auto &p : ins.dst) {
            if (p.ty == Parameter::IMM) {
                cout << "(IMM 0x" << std::hex << p.idx << std::dec << ") ";
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg) << ") ";
            }
            else if (p.ty == Parameter::MEM) {
                cout << "(MEM 0x" << std::hex << p.idx << std::dec << ") ";
            }
            else {
                cout << "[Error] Unknown dst Parameter type! ";
            }
        }
        cout << endl;
    }
}

/*
 * Perform a backward slice on the instruction list L,
 * starting from the last instruction's src parameters.
 */
int backslice(list<Inst> &L)
{
    // 'wl' is our working set of Parameters to track backward
    set<Parameter> wl;
    // 'sl' is the final sliced list of instructions
    list<Inst> sl;

    // Start from the last instruction
    if (L.empty()) {
        cout << "[backslice] No instructions in list!\n";
        return 0;
    }
    auto rit = L.rbegin();
    // Add all its src parameters to the worklist
    for (const auto &param : rit->src) {
        wl.insert(param);
    }
    // Also consider xchg's src2 if relevant
    for (const auto &param : rit->src2) {
        wl.insert(param);
    }

    // Put that last instruction in the sliced list
    sl.push_front(*rit);
    ++rit;

    // Walk instructions in reverse
    while (rit != L.rend()) {
        bool isdepMain = false;
        bool isdepXchgSecond = false; // for xchg’s second set of dst2

        if (rit->dst.empty() && rit->dst2.empty()) {
            // No destinations => not data dependent
        }
        else if (rit->opcstr == "xchg") {
            // Check main dst
            for (const auto &dstParam : rit->dst) {
                auto found = wl.find(dstParam);
                if (found != wl.end()) {
                    isdepMain = true;
                    wl.erase(found);
                }
            }
            // Check secondary dst2
            for (const auto &dstParam2 : rit->dst2) {
                auto found2 = wl.find(dstParam2);
                if (found2 != wl.end()) {
                    isdepXchgSecond = true;
                    wl.erase(found2);
                }
            }
            // If we depend on the main dst
            if (isdepMain) {
                // Push src2 into worklist
                for (const auto &src2Param : rit->src2) {
                    wl.insert(src2Param);
                }
                sl.push_front(*rit);
            }
            // If we depend on the second dst
            if (isdepXchgSecond) {
                // Push main src into worklist
                for (const auto &srcParam : rit->src) {
                    wl.insert(srcParam);
                }
                sl.push_front(*rit);
            }
        }
        else {
            // Normal single-dst or multi-dst instructions
            bool dependent = false;
            for (const auto &dstParam : rit->dst) {
                auto found = wl.find(dstParam);
                if (found != wl.end()) {
                    dependent = true;
                    wl.erase(found);
                }
            }
            if (dependent) {
                // Insert non-IMM sources into the worklist
                for (const auto &srcParam : rit->src) {
                    if (srcParam.ty != Parameter::IMM) {
                        wl.insert(srcParam);
                    }
                }
                // Possibly also handle src2 if relevant:
                for (const auto &src2Param : rit->src2) {
                    if (src2Param.ty != Parameter::IMM) {
                        wl.insert(src2Param);
                    }
                }
                sl.push_front(*rit);
            }
        }
        ++rit;
    }

    // Print any leftover parameters in the working list
    if (!wl.empty()) {
        cout << "\n[backslice] Leftover parameters in WL:\n";
        for (const auto &p : wl) {
            p.show();
        }
        cout << endl;
    }

    // Print the sliced instructions
    cout << "\n[backslice] Final Sliced Instructions:\n";
    printInstParameter(sl);

    // Write the slices out to separate files in your custom format
    printTraceHuman(sl, "slice.human.trace");
    printTraceLLSE(sl, "slice.llse.trace");

    return 0;
}

/*
 * Check if a string is hex ("0x...").
 */
bool ishex(const string &s)
{
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return true;
    }
    return false;
}

/*
 * Check if a string is one of our recognized 64-bit regs (rax, rbx, etc.).
 * Includes vector registers and 'rip'.
 */
bool isreg(const string &s)
{
    static const set<string> gprs64 = {
        "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
        "r8","r9","r10","r11","r12","r13","r14","r15",
        "rip",
        "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
        "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15",
        "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7",
        "ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15",
        "zmm0","zmm1","zmm2","zmm3","zmm4","zmm5","zmm6","zmm7",
        "zmm8","zmm9","zmm10","zmm11","zmm12","zmm13","zmm14","zmm15"
    };
    return (gprs64.find(s) != gprs64.end());
}

/*
 * Check if [i1..i2) are all "push <reg>", and no repeated regs.
 */
bool chkpush(list<Inst>::iterator i1, list<Inst>::iterator i2)
{
    int opcpush = getOpc("push", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpush) return false;
        if (it->oprs.empty())   return false;
        if (!isreg(it->oprs[0])) {
            return false;
        }
    }
    // Ensure no repeated registers
    set<string> used;
    for (auto it = i1; it != i2; ++it) {
        const string &rname = it->oprs[0];
        if (!used.insert(rname).second) {
            return false;
        }
    }
    return true;
}

/*
 * Check if [i1..i2) are all "pop <reg>", and no repeated regs.
 */
bool chkpop(list<Inst>::iterator i1, list<Inst>::iterator i2)
{
    int opcpop = getOpc("pop", instenum);
    for (auto it = i1; it != i2; ++it) {
        if (it->opc != opcpop) return false;
        if (it->oprs.empty())  return false;
        if (!isreg(it->oprs[0])) {
            return false;
        }
    }
    // Ensure no repeated registers
    set<string> used;
    for (auto it = i1; it != i2; ++it) {
        const string &rname = it->oprs[0];
        if (!used.insert(rname).second) {
            return false;
        }
    }
    return true;
}

/*
 * Search the instruction list L and extract "VM" snippets:
 * sequences of 7 pushes or 7 pops in a row.
 */
void vmextract(list<Inst> &L)
{
    // We'll look for exactly 7 consecutive pushes or pops.
    // If you want a different count, change "7" to something else.
    const int VM_COUNT = 7;

    for (auto it = L.begin(); it != L.end(); ++it) {
        // Ensure we have at least 7 instructions from [it..end).
        if (distance(it, L.end()) < VM_COUNT) {
            break; // Not enough instructions
        }
        auto it7 = it;
        advance(it7, VM_COUNT);

        // Check 7 pushes
        if (chkpush(it, it7)) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = it7;
            // Assume ctxreg[6] is 'rsp' if your trace sets that
            cs.sd    = it7->ctxreg[6];  // 64-bit stack pointer
            ctxsave.push_back(cs);
            cout << "[vmextract] Push sequence found starting at instruction ID " 
                 << it->id << " (" << it->addr << ")\n";
            // Skip the next 6 instructions as they are part of this VM snippet
            it = it7;
            continue;
        }
        // Check 7 pops
        else if (chkpop(it, it7)) {
            ctxswitch cs;
            cs.begin = it;
            cs.end   = it7;
            cs.sd    = it->ctxreg[6];
            ctxrestore.push_back(cs);
            cout << "[vmextract] Pop sequence found starting at instruction ID " 
                 << it->id << " (" << it->addr << ")\n";
            // Skip the next 6 instructions as they are part of this VM snippet
            it = it7;
            continue;
        }
    }

    // Pair up saves and restores by matching sd
    for (const auto &sv : ctxsave) {
        for (const auto &rs : ctxrestore) {
            if (sv.sd == rs.sd) {
                ctxswh.emplace_back(make_pair(sv, rs));
                // If you only want to pair each save with its first matching restore, uncomment the next line
                // break;
            }
        }
    }

    // Optionally, clear ctxsave and ctxrestore if they are no longer needed
    // ctxsave.clear();
    // ctxrestore.clear();
}

/*
 * Write the extracted VM snippets to separate files (vm1.txt, vm2.txt, etc.).
 */
void outputvm(const list<pair<ctxswitch, ctxswitch>> &ctxswh)
{
    int n = 1;
    for (const auto &pairCS : ctxswh) {
        auto i1 = pairCS.first.begin;
        auto i2 = pairCS.second.end;
        string vmfile = "vm" + to_string(n++) + ".txt";
        ofstream fp(vmfile);
        if (!fp.is_open()) {
            cerr << "[outputvm] Failed to open " << vmfile << endl;
            continue;
        }
        // Dump instructions from [i1..i2)
        for (auto it = i1; it != i2; ++it) {
            fp << it->addr << ";" << it->assembly << ";";
            // Print context registers
            for (int j = 0; j < 8; ++j) {
                fp << "0x" << hex << it->ctxreg[j] << ",";
            }
            // Print read/write addresses
            fp << "0x" << hex << it->raddr << ",0x" << hex << it->waddr << "\n";
        }
        fp.close();
        cout << "[outputvm] Written VM snippet to " << vmfile << endl;
    }
}

/*
 * Print the instructions in L along with their src/dst parameters.
 */
void printInstParameter(const list<Inst> &L)
{
    for (const auto &ins : L) {
        cout << ins.id << " " << ins.addr << " " << ins.assembly << "\t";
        cout << "src: ";

        // Print src parameters
        for (const auto &p : ins.src) {
            if (p.ty == Parameter::IMM) {
                cout << "(IMM 0x" << hex << p.idx << dec << ") ";
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg) << ") ";
            }
            else if (p.ty == Parameter::MEM) {
                cout << "(MEM 0x" << hex << p.idx << dec << ") ";
            }
            else {
                cout << "[Error] Unknown src Parameter type! ";
            }
        }

        cout << ", dst: ";
        for (const auto &p : ins.dst) {
            if (p.ty == Parameter::IMM) {
                cout << "(IMM 0x" << hex << p.idx << dec << ") ";
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg) << ") ";
            }
            else if (p.ty == Parameter::MEM) {
                cout << "(MEM 0x" << hex << p.idx << dec << ") ";
            }
            else {
                cout << "[Error] Unknown dst Parameter type! ";
            }
        }
        cout << endl;
    }
}

/*
 * MAIN function for the slicer.
 */
int main(int argc, char **argv)
{
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <tracefile>\n";
        return 1;
    }

    // Open and parse the trace
    ifstream infile(argv[1]);
    if (!infile.is_open()) {
        cerr << "[Error] Cannot open file: " << argv[1] << endl;
        return 1;
    }
    parseTrace(infile, instlist); // Pass by reference
    infile.close();

    // Convert string operands into structured 'Operand'
    parseOperand(instlist.begin(), instlist.end());

    // Build the fine-grained parameter sets (src/dst)
    if (buildParameter(instlist) != 0) {
        cerr << "[Error] buildParameter failed.\n";
        return 1;
    }

    // Optionally, print parameters for debugging
    // printInstParameter(instlist);

    // Perform a backward slice from the last instruction
    if (backslice(instlist) != 0) {
        cerr << "[Error] backslice encountered an issue.\n";
        return 1;
    }

    return 0;
}
