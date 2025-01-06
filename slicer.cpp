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
    "jcxz","jecxz","ret","cmp","call"
};

/*
 * Build fine-grained parameters (src/dst) for each instruction in L.
 * 
 * This uses operand info (op0->ty, op0->field[0], etc.) plus
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
            Operand *op0 = ins.oprd[0];
            int nbyte = 0;

            if (ins.opcstr == "push") {
                // On a 64-bit system, pushing is 8 bytes
                nbyte = (op0->bit / 8 > 0) ? (op0->bit / 8) : 8;  
                // But if it's truly 64-bit push, override to 8 if needed
                nbyte = 8;  

                if (op0->ty == Operand::IMM) {
                    ins.addsrc(Parameter::IMM, op0->field[0]);
                    AddrRange ar(ins.waddr, ins.waddr + nbyte - 1);
                    ins.adddst(Parameter::MEM, ar);
                }
                else if (op0->ty == Operand::REG) {
                    ins.addsrc(Parameter::REG, op0->field[0]);
                    AddrRange ar(ins.waddr, ins.waddr + nbyte - 1);
                    ins.adddst(Parameter::MEM, ar);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    if (nbyte == 0) nbyte = 8;  // fallback
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc(Parameter::MEM, rar);

                    AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                    ins.adddst(Parameter::MEM, war);
                }
                else {
                    cerr << "[push error] Unknown operand type for op0!\n";
                    return 1;
                }
            }
            else if (ins.opcstr == "pop") {
                // On 64-bit, pop also fetches 8 bytes
                nbyte = (op0->bit / 8 > 0) ? (op0->bit / 8) : 8;  
                nbyte = 8;  

                if (op0->ty == Operand::REG) {
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc(Parameter::MEM, rar);
                    ins.adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc(Parameter::MEM, rar);
                    AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                    ins.adddst(Parameter::MEM, war);
                }
                else {
                    cerr << "[pop error] op0 is not REG or MEM!\n";
                    return 1;
                }
            }
            else {
                // Single-operand instructions: inc [mem], dec reg, neg reg, etc.
                if (op0->ty == Operand::REG) {
                    ins.addsrc(Parameter::REG, op0->field[0]);
                    ins.adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc(Parameter::MEM, rar);
                    AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                    ins.adddst(Parameter::MEM, war);
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
            Operand *op0 = ins.oprd[0];
            Operand *op1 = ins.oprd[1];
            int nbyte = 0;

            // Common instructions: mov, movzx, etc.
            if (ins.opcstr == "mov" || ins.opcstr == "movzx") {
                if (op0->ty == Operand::REG) {
                    if (op1->ty == Operand::IMM) {
                        ins.addsrc(Parameter::IMM, op1->field[0]);
                        ins.adddst(Parameter::REG, op0->field[0]);
                    }
                    else if (op1->ty == Operand::REG) {
                        ins.addsrc(Parameter::REG, op1->field[0]);
                        ins.adddst(Parameter::REG, op0->field[0]);
                    }
                    else if (op1->ty == Operand::MEM) {
                        nbyte = op1->bit / 8;
                        if (nbyte == 0) nbyte = 8;
                        AddrRange rar(ins.raddr, ins.raddr + nbyte - 1);
                        ins.addsrc(Parameter::MEM, rar);
                        ins.adddst(Parameter::REG, op0->field[0]);
                    }
                    else {
                        cerr << "[mov error] op0=REG, op1 not IMM/REG/MEM\n";
                        return 1;
                    }
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    if (nbyte == 0) nbyte = 8;

                    if (op1->ty == Operand::IMM) {
                        ins.addsrc(Parameter::IMM, op1->field[0]);
                        AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                        ins.adddst(Parameter::MEM, war);
                    }
                    else if (op1->ty == Operand::REG) {
                        ins.addsrc(Parameter::REG, op1->field[0]);
                        AddrRange war(ins.waddr, ins.waddr + nbyte - 1);
                        ins.adddst(Parameter::MEM, war);
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
                if (op0->ty != Operand::REG || op1->ty != Operand::MEM) {
                    cerr << "[lea error] op0 must be REG, op1 must be MEM\n";
                    break;
                }
                // For simplicity, only handle a few tags, or handle them all if your code does
                switch (op1->tag) {
                case 5: // e.g. rax+rbx*2
                    ins.addsrc(Parameter::REG, op1->field[0]); // base
                    ins.addsrc(Parameter::REG, op1->field[1]); // index
                    // The result goes into op0
                    ins.adddst(Parameter::REG, op0->field[0]);
                    break;
                // Add other cases (tag 3,4,6,7) if needed
                default:
                    cerr << "[lea error] unhandled address tag: " << op1->tag << endl;
                    break;
                }
            }
            else if (ins.opcstr == "xchg") {
                // xchg => each operand is both src and dst
                // We'll store them as separate sets: main (src/dst) vs. second (src2/dst2)
                if (op1->ty == Operand::REG) {
                    ins.addsrc(Parameter::REG, op1->field[0]);
                    ins.adddst2(Parameter::REG, op1->field[0]);
                }
                else if (op1->ty == Operand::MEM) {
                    nbyte = op1->bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange ar(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc(Parameter::MEM, ar);
                    ins.adddst2(Parameter::MEM, ar);
                }
                else {
                    cerr << "[xchg error] op1 is not REG or MEM\n";
                    return 1;
                }

                if (op0->ty == Operand::REG) {
                    ins.addsrc2(Parameter::REG, op0->field[0]);
                    ins.adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange ar(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc2(Parameter::MEM, ar);
                    ins.adddst(Parameter::MEM, ar);
                }
                else {
                    cerr << "[xchg error] op0 is not REG or MEM\n";
                    return 1;
                }
            }
            else {
                // Generic 2-operand instruction (like add, sub, and, or, etc.)
                // 1) handle second operand as source
                if (op1->ty == Operand::IMM) {
                    ins.addsrc(Parameter::IMM, op1->field[0]);
                }
                else if (op1->ty == Operand::REG) {
                    ins.addsrc(Parameter::REG, op1->field[0]);
                }
                else if (op1->ty == Operand::MEM) {
                    nbyte = op1->bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange rar1(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc(Parameter::MEM, rar1);
                }
                else {
                    cerr << "[2-op error] op1 not IMM/REG/MEM\n";
                    return 1;
                }

                // 2) handle first operand as source+dest
                if (op0->ty == Operand::REG) {
                    ins.addsrc(Parameter::REG, op0->field[0]);
                    ins.adddst(Parameter::REG, op0->field[0]);
                }
                else if (op0->ty == Operand::MEM) {
                    nbyte = op0->bit / 8;
                    if (nbyte == 0) nbyte = 8;
                    AddrRange rar2(ins.raddr, ins.raddr + nbyte - 1);
                    ins.addsrc(Parameter::MEM, rar2);
                    ins.adddst(Parameter::MEM, rar2);
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
            Operand *op0 = ins.oprd[0];
            Operand *op1 = ins.oprd[1];
            Operand *op2 = ins.oprd[2];

            if (ins.opcstr == "imul" &&
                op0->ty == Operand::REG &&
                op1->ty == Operand::REG &&
                op2->ty == Operand::IMM)
            {
                ins.addsrc(Parameter::IMM, op2->field[0]);
                ins.addsrc(Parameter::REG, op1->field[0]);
                ins.addsrc(Parameter::REG, op0->field[0]);
                // The result presumably goes into op0->REG as well.
                ins.adddst(Parameter::REG, op0->field[0]);
            }
            else {
                cerr << "[3-op error] unrecognized pattern, e.g. 'imul reg, reg, imm'\n";
                return 1;
            }
            break;
        }

        default:
            cerr << "[error] instruction has " << ins.oprnum 
                 << " operands (more than 3?) or unknown form\n";
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
    for (auto &ins : L) {
        cout << ins.id << " " << ins.addr << " " << ins.assembly << "\t";
        cout << "src: ";

        // Print src parameters
        for (auto &p : ins.src) {
            if (p.ty == Parameter::IMM) {
                cout << "(IMM 0x" << std::hex << p.idx << std::dec << ") ";
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg) << p.idx << ") ";
            }
            else if (p.ty == Parameter::MEM) {
                cout << "(MEM 0x" << std::hex << p.idx << std::dec << ") ";
            }
            else {
                cout << "[Error] Unknown src Parameter type! ";
            }
        }

        cout << ", dst: ";
        for (auto &p : ins.dst) {
            if (p.ty == Parameter::IMM) {
                cout << "(IMM 0x" << std::hex << p.idx << std::dec << ") ";
            }
            else if (p.ty == Parameter::REG) {
                cout << "(REG " << reg2string(p.reg) << p.idx << ") ";
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
    for (auto &param : rit->src) {
        wl.insert(param);
    }
    // Also consider xchg's src2 if relevant
    for (auto &param : rit->src2) {
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
            for (auto &dstParam : rit->dst) {
                auto found = wl.find(dstParam);
                if (found != wl.end()) {
                    isdepMain = true;
                    wl.erase(found);
                }
            }
            // Check secondary dst2
            for (auto &dstParam2 : rit->dst2) {
                auto found2 = wl.find(dstParam2);
                if (found2 != wl.end()) {
                    isdepXchgSecond = true;
                    wl.erase(found2);
                }
            }
            // If we depend on the main dst
            if (isdepMain) {
                // push src2 into worklist
                for (auto &src2Param : rit->src2) {
                    wl.insert(src2Param);
                }
                sl.push_front(*rit);
            }
            // If we depend on the second dst
            if (isdepXchgSecond) {
                // push main src into worklist
                for (auto &srcParam : rit->src) {
                    wl.insert(srcParam);
                }
                sl.push_front(*rit);
            }
        }
        else {
            // Normal single-dst or multi-dst instructions
            bool dependent = false;
            for (auto &dstParam : rit->dst) {
                auto found = wl.find(dstParam);
                if (found != wl.end()) {
                    dependent = true;
                    wl.erase(found);
                }
            }
            if (dependent) {
                // Insert non-IMM sources into the worklist
                for (auto &srcParam : rit->src) {
                    if (!srcParam.isIMM()) {
                        wl.insert(srcParam);
                    }
                }
                // Possibly also handle src2 if relevant:
                for (auto &src2Param : rit->src2) {
                    if (!src2Param.isIMM()) {
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
        for (auto &p : wl) {
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
    parseTrace(&infile, &instlist);
    infile.close();

    // Convert string operands into structured 'Operand'
    parseOperand(instlist.begin(), instlist.end());

    // Build the fine-grained parameter sets (src/dst)
    if (buildParameter(instlist) != 0) {
        cerr << "[Error] buildParameter failed.\n";
        return 1;
    }

    // Perform a backward slice from the last instruction
    if (backslice(instlist) != 0) {
        cerr << "[Error] backslice encountered an issue.\n";
        return 1;
    }

    return 0;
}
