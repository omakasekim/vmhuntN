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

#include "core.hpp"    // Make sure Parameter::idx and Inst::raddr/waddr etc. are uint64_t
#include "parser.hpp"  // parseTrace(...), parseOperand(...)

// Global instruction list
list<Inst> instlist;

/*
 * A set of instructions that do NOT affect data dependencies,
 * so we skip them when building parameters.
 */
set<string> skipinst = {
    "test","jmp","jz","jbe","jo","jno","js","jns","je","jne",
    "jnz","jb","jnae","jc","jnb","jae","jnc","jna","ja","jnbe","jl",
    "jnge","jge","jnl","jle","jng","jg","jnle","jp","jpe","jnp","jpo",
    "jcxz","jecxz", "ret", "cmp", "call"
};

/*
 * Build fine-grained parameters (src/dst) for each instruction in L.
 */
int buildParameter(list<Inst> &L)
{
    for (auto it = L.begin(); it != L.end(); ++it) {
        // If the opcode is in skipinst, do nothing for it
        if (skipinst.find(it->opcstr) != skipinst.end()) {
            continue;
        }

        switch (it->oprnum) {
        case 0:
            // No operands, nothing to do
            break;

        case 1:
        {
            Operand *op0 = it->oprd[0];
            int nbyte = 0;

            if (it->opcstr == "push") {
                if (op0->ty == Operand::IMM) {
                    it->addsrc(Parameter::IMM, op0->field[0]);
                    // Example: push imm writes 4 bytes to the stack (for 32-bit push).
                    // For 64-bit, you might push 8 bytes. Adjust if needed.
                    AddrRange ar(it->waddr, it->waddr + 3);
                    it->adddst(Parameter::MEM, ar);
                }
                else if (op0->ty == Operand::REG) {
                    it->addsrc(Parameter::REG, op0->field[0]);
                    nbyte = op0->bit / 8;
                    AddrRange ar(it->waddr, it->waddr + nbyte - 1);
                    it->adddst(Parameter::MEM, ar);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    AddrRange rar(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc(Parameter::MEM, rar);
                    AddrRange war(it->waddr, it->waddr + nbyte - 1);
                    it->adddst(Parameter::MEM, war);
                }
                else {
                    cout << "push error: operand is not IMM, REG or MEM!" << endl;
                    return 1;
                }
            }
            else if (it->opcstr == "pop") {
                if (op0->ty == Operand::REG) {
                    nbyte = op0->bit / 8;
                    AddrRange rar(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc(Parameter::MEM, rar);
                    it->adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    AddrRange rar(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc(Parameter::MEM, rar);
                    AddrRange war(it->waddr, it->waddr + nbyte - 1);
                    it->adddst(Parameter::MEM, war);
                }
                else {
                    cout << "pop error: the operand is not REG or MEM!" << endl;
                    return 1;
                }
            }
            else {
                // e.g., inc [mem], dec reg, etc. (single-operand instructions)
                if (op0->ty == Operand::REG) {
                    it->addsrc(Parameter::REG, op0->field[0]);
                    it->adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    AddrRange rar(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc(Parameter::MEM, rar);
                    AddrRange war(it->waddr, it->waddr + nbyte - 1);
                    it->adddst(Parameter::MEM, war);
                }
                else {
                    cout << "[Error] Instruction " << it->id
                         << ": Unknown 1-op form!"  << endl;
                    return 1;
                }
            }
            break;
        }

        case 2:
        {
            Operand *op0 = it->oprd[0];
            Operand *op1 = it->oprd[1];
            int nbyte = 0;

            // e.g., mov, movzx, etc.
            if (it->opcstr == "mov" || it->opcstr == "movzx") {
                if (op0->ty == Operand::REG) {
                    if (op1->ty == Operand::IMM) {
                        it->addsrc(Parameter::IMM, op1->field[0]);
                        it->adddst(Parameter::REG, op0->field[0]);
                    }
                    else if (op1->ty == Operand::REG) {
                        it->addsrc(Parameter::REG, op1->field[0]);
                        it->adddst(Parameter::REG, op0->field[0]);
                    }
                    else if (op1->ty == Operand::MEM) {
                        nbyte = op1->bit / 8;
                        AddrRange rar(it->raddr, it->raddr + nbyte - 1);
                        it->addsrc(Parameter::MEM, rar);
                        it->adddst(Parameter::REG, op0->field[0]);
                    }
                    else {
                        cout << "mov error: op0=REG, but op1 is not IMM/REG/MEM" << endl;
                        return 1;
                    }
                }
                else if (op0->ty == Operand::MEM) {
                    if (op1->ty == Operand::IMM) {
                        it->addsrc(Parameter::IMM, op1->field[0]);
                        nbyte = op0->bit / 8;
                        AddrRange war(it->waddr, it->waddr + nbyte - 1);
                        it->adddst(Parameter::MEM, war);
                    }
                    else if (op1->ty == Operand::REG) {
                        it->addsrc(Parameter::REG, op1->field[0]);
                        nbyte = op0->bit / 8;
                        AddrRange war(it->waddr, it->waddr + nbyte - 1);
                        it->adddst(Parameter::MEM, war);
                    }
                    else {
                        cout << "mov error: op0=MEM, op1 not IMM/REG" << endl;
                        return 1;
                    }
                }
                else {
                    cout << "mov error: op0 is not MEM or REG." << endl;
                    return 1;
                }
            }
            else if (it->opcstr == "lea") {
                // e.g., lea reg, [reg + reg*scale + disp]
                if (op0->ty != Operand::REG || op1->ty != Operand::MEM) {
                    cout << "lea format error!" << endl;
                }
                switch (op1->tag) {
                case 5:
                    // Example: op1->field[0] = base reg, field[1] = index reg
                    it->addsrc(Parameter::REG, op1->field[0]);
                    it->addsrc(Parameter::REG, op1->field[1]);
                    it->adddst(Parameter::REG, op0->field[0]);
                    break;
                default:
                    cerr << "lea error: unhandled address pattern or tag." << endl;
                    break;
                }
            }
            else if (it->opcstr == "xchg") {
                // xchg can have two destinations (op0 and op1) and two sources
                if (op1->ty == Operand::REG) {
                    it->addsrc(Parameter::REG, op1->field[0]);
                    it->adddst2(Parameter::REG, op1->field[0]);
                }
                else if (op1->ty == Operand::MEM) {
                    nbyte = op1->bit / 8;
                    AddrRange ar(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc(Parameter::MEM, ar);
                    it->adddst2(Parameter::MEM, ar);
                }
                else {
                    cout << "xchg error: op1 is not REG or MEM." << endl;
                    return 1;
                }

                if (op0->ty == Operand::REG) {
                    it->addsrc2(Parameter::REG, op0->field[0]);
                    it->adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    AddrRange ar(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc2(Parameter::MEM, ar);
                    it->adddst(Parameter::MEM, ar);
                }
                else {
                    cout << "xchg error: op0 is not REG or MEM." << endl;
                    return 1;
                }
            }
            else {
                // "other" 2-op instructions, e.g. add reg, imm / add mem, reg, etc.
                // handle the second operand first (source)
                if (op1->ty == Operand::IMM) {
                    it->addsrc(Parameter::IMM, op1->field[0]);
                }
                else if (op1->ty == Operand::REG) {
                    it->addsrc(Parameter::REG, op1->field[0]);
                }
                else if (op1->ty == Operand::MEM) {
                    nbyte = op1->bit / 8;
                    AddrRange rar1(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc(Parameter::MEM, rar1);
                }
                else {
                    cout << "2-op error: op1 not IMM/REG/MEM." << endl;
                    return 1;
                }

                // handle the first operand (destination, also can be source)
                if (op0->ty == Operand::REG) {
                    it->addsrc(Parameter::REG, op0->field[0]);
                    it->adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    AddrRange rar2(it->raddr, it->raddr + nbyte - 1);
                    it->addsrc(Parameter::MEM, rar2);
                    it->adddst(Parameter::MEM, rar2);
                }
                else {
                    cout << "2-op error: op0 not REG or MEM." << endl;
                    return 1;
                }
            }
            break;
        }

        case 3:
        {
            // e.g. imul reg, reg, imm
            Operand *op0 = it->oprd[0];
            Operand *op1 = it->oprd[1];
            Operand *op2 = it->oprd[2];

            if (it->opcstr == "imul" &&
                op0->ty == Operand::REG &&
                op1->ty == Operand::REG &&
                op2->ty == Operand::IMM)
            {
                it->addsrc(Parameter::IMM, op2->field[0]);
                it->addsrc(Parameter::REG, op1->field[0]);
                it->addsrc(Parameter::REG, op0->field[0]);
                // The result presumably goes into op0->REG as well
                // (If that's how your imul is defined.)
            }
            else {
                cout << "3-op error: not recognized pattern (e.g. 'imul reg, reg, imm')." << endl;
                return 1;
            }
            break;
        }

        default:
            cout << "error: instruction has more than 3 operands or unknown form." << endl;
            return 1;
        }
    }

    return 0;
}

/*
 * Print the instructions in L along with their src/dst parameters.
 */
void printInstParameter(list<Inst> &L)
{
    for (auto it = L.begin(); it != L.end(); ++it) {
        cout << it->id << " " << it->addr << " " << it->assembly << "\t";
        cout << "src: ";

        // Print src parameters
        for (size_t i = 0; i < it->src.size(); i++) {
            Parameter &p = it->src[i];
            if (p.ty == Parameter::IMM) {
                cout << "(IMM ";
                printf("0x%llx) ", (unsigned long long)p.idx);
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg)
                     << p.idx << ") ";
            }
            else if (p.ty == Parameter::MEM) {
                cout << "(MEM ";
                printf("0x%llx) ", (unsigned long long)p.idx);
            }
            else {
                cout << "[Error] Unknown src Parameter type!\n";
            }
        }

        cout << ", dst: ";
        // Print dst parameters
        for (size_t i = 0; i < it->dst.size(); i++) {
            Parameter &p = it->dst[i];
            if (p.ty == Parameter::IMM) {
                cout << "(IMM ";
                printf("0x%llx) ", (unsigned long long)p.idx);
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg)
                     << p.idx << ") ";
            }
            else if (p.ty == Parameter::MEM) {
                cout << "(MEM ";
                printf("0x%llx) ", (unsigned long long)p.idx);
            }
            else {
                cout << "[Error] Unknown dst Parameter type!\n";
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

    // Start from the last instruction's sources
    auto rit = L.rbegin();
    for (size_t i = 0; i < rit->src.size(); i++) {
        wl.insert(rit->src[i]);
    }
    sl.push_front(*rit);
    ++rit;

    // Walk instructions in reverse
    while (rit != L.rend()) {
        bool isdep1 = false;
        bool isdep2 = false; // for xchg’s second set of destinations, if any

        // If no dst, skip
        if (rit->dst.empty()) {
            // do nothing
        }
        else if (rit->opcstr == "xchg") {
            // xchg can have dst and dst2
            // Check main dst
            for (auto &dstParam : rit->dst) {
                auto sit1 = wl.find(dstParam);
                if (sit1 != wl.end()) {
                    isdep1 = true;
                    wl.erase(sit1);
                }
            }
            // Check secondary dst2
            for (auto &dstParam2 : rit->dst2) {
                auto sit2 = wl.find(dstParam2);
                if (sit2 != wl.end()) {
                    isdep2 = true;
                    wl.erase(sit2);
                }
            }
            // If we depend on the main dst
            if (isdep1) {
                for (auto &src2Param : rit->src2) {
                    wl.insert(src2Param);
                }
                sl.push_front(*rit);
            }
            // If we depend on the second dst
            if (isdep2) {
                for (auto &srcParam : rit->src) {
                    wl.insert(srcParam);
                }
                sl.push_front(*rit);
            }
        }
        else {
            // Regular single-dst or multi-dst but not xchg
            for (auto &dstParam : rit->dst) {
                auto sit = wl.find(dstParam);
                if (sit != wl.end()) {
                    isdep1 = true;
                    wl.erase(sit);
                }
            }
            if (isdep1) {
                // Insert non-IMM sources into the worklist
                for (auto &srcParam : rit->src) {
                    if (!srcParam.isIMM()) {
                        wl.insert(srcParam);
                    }
                }
                sl.push_front(*rit);
            }
        }
        ++rit;
    }

    // Print any leftover parameters in the working list
    for (auto &p : wl) {
        p.show();  // Make sure show() prints in 64-bit format if needed
    }
    cout << endl;

    // Print the sliced instructions
    printInstParameter(sl);

    // Write the slices out to separate files in your custom format
    printTraceHuman(sl, "slice.human.trace");
    printTraceLLSE(sl, "slice.llse.trace");

    return 0;
}

/*
 * MAIN
 */
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <tracefile>\n", argv[0]);
        return 1;
    }

    // Open and parse the trace
    ifstream infile(argv[1]);
    if (!infile.is_open()) {
        fprintf(stderr, "Open file error!\n");
        return 1;
    }
    parseTrace(&infile, &instlist);
    infile.close();

    // Parse operands from strings to structured Operands
    parseOperand(instlist.begin(), instlist.end());

    // Build fine-grained parameter sets
    buildParameter(instlist);

    // Perform a backward slice
    backslice(instlist);

    return 0;
}
