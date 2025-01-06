// mg-symengine.cpp

#include "mg-symengine.hpp" // Include the corrected header
#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <queue>
#include <bitset>
#include <sstream>
#include <cstdint>      // for uint64_t
#include <cstdio>       // for printf, FILE, etc.
#include <cstdlib>      // for size_t, stoull

// Use the standard namespace
using namespace std;

// Define Operand Types as Enums for Clarity
enum OperandType { IMM = 1, REG = 2, MEM = 3 };

// Assuming Operand and Inst are defined in mg-symengine.hpp or other included headers
// No need to redefine them here

// Initialize the static member outside the class
int Value::idseed = 0;

// Implementation of Value Constructors
Value::Value(ValueTy vty) : opr(NULL), len(64) {
    id = ++idseed;
    valty = vty;
}

Value::Value(ValueTy vty, int l) : opr(NULL), len(l) {
    id = ++idseed;
    valty = vty;
}

Value::Value(ValueTy vty, string con) : opr(NULL), conval(con), len(64) {
    id = ++idseed;
    valty = vty;
    uint64_t tmp = 0;
    if (!con.empty()) {
        // Handle possible “0x” prefix
        tmp = stoull(con, nullptr, 16);
    }
    bsconval = bitset<64>(tmp);
    brange = {0, 63};
}

Value::Value(ValueTy vty, string con, int l) : opr(NULL), conval(con), len(l) {
    id = ++idseed;
    valty = vty;
}

Value::Value(ValueTy vty, bitset<64> bs) : opr(NULL), bsconval(bs), len(64) {
    id = ++idseed;
    valty = vty;
}

Value::Value(ValueTy vty, Operation *oper) : opr(oper), len(64) {
    id = ++idseed;
    valty = vty;
}

Value::Value(ValueTy vty, Operation *oper, int l) : opr(oper), len(l) {
    id = ++idseed;
    valty = vty;
}

// Implementation of Operation Constructors
Operation::Operation(string opt, Value *v1) : opty(opt) {
    val[0] = v1;
    val[1] = nullptr;
    val[2] = nullptr;
}

Operation::Operation(string opt, Value *v1, Value *v2) : opty(opt) {
    val[0] = v1;
    val[1] = v2;
    val[2] = nullptr;
}

Operation::Operation(string opt, Value *v1, Value *v2, Value *v3) : opty(opt) {
    val[0] = v1;
    val[1] = v2;
    val[2] = v3;
}

// Helper Functions to Build Operations
Value *buildop1(string opty, Value *v1) {
    Operation *oper = new Operation(opty, v1);
    Value *result;

    if (v1->isSymbol())
        result = new Value(SYMBOL, oper);
    else
        result = new Value(CONCRETE, oper);

    return result;
}

Value *buildop2(string opty, Value *v1, Value *v2) {
    Operation *oper = new Operation(opty, v1, v2);
    Value *result;

    if (v1->isSymbol() || v2->isSymbol())
        result = new Value(SYMBOL, oper);
    else
        result = new Value(CONCRETE, oper);

    return result;
}

Value *buildop3(string opty, Value *v1, Value *v2, Value *v3) {
    Operation *oper = new Operation(opty, v1, v2, v3);
    Value *result;

    if (v1->isSymbol() || v2->isSymbol() || v3->isSymbol())
        result = new Value(SYMBOL, oper);
    else
        result = new Value(CONCRETE, oper);

    return result;
}

// Implementation of SEEngine Methods

// Helper Functions Implementations
bool SEEngine::memfind(const AddrRange& ar) {
    return mem.find(ar) != mem.end();
}

bool SEEngine::memfind(ADDR64 b, ADDR64 e) {
    AddrRange ar(b, e);
    return mem.find(ar) != mem.end();
}

bool SEEngine::issubset(const AddrRange& ar, AddrRange *superset) {
    for (auto it = mem.begin(); it != mem.end(); ++it) {
        AddrRange curar = it->first;
        if (curar.start <= ar.start && curar.end >= ar.end) {
            *superset = curar;
            return true;
        }
    }
    return false;
}

bool SEEngine::issuperset(const AddrRange& ar, AddrRange *subset) {
    for (auto it = mem.begin(); it != mem.end(); ++it) {
        AddrRange curar = it->first;
        if (curar.start >= ar.start && curar.end <= ar.end) {
            *subset = curar;
            return true;
        }
    }
    return false;
}

bool SEEngine::isnew(const AddrRange& ar) {
    for (auto it = mem.begin(); it != mem.end(); ++it) {
        AddrRange curar = it->first;
        // Overlap check
        if ((curar.start <= ar.start && curar.end >= ar.start) ||
            (curar.start <= ar.end && curar.end >= ar.end)) {
            return false;
        }
    }
    return true;
}

Value* SEEngine::readReg(const string &s) {
    // 64-bit full register
    if (s == "rax" || s == "rbx" || s == "rcx" || s == "rdx" ||
        s == "rsi" || s == "rdi" || s == "rsp" || s == "rbp") {
        return ctx[s];
    }
    // 32-bit sub-registers: eax, ebx, ecx, edx
    else if (s == "eax" || s == "ebx" || s == "ecx" || s == "edx") {
        string rname = "r" + s.substr(1); // "eax" -> "rax"
        if (hasVal(ctx[rname], 0, 31)) {
            return readVal(ctx[rname], 0, 31);
        } else {
            // Create a concrete mask for the lower 32 bits
            Value *v1 = new Value(CONCRETE, "0x00000000ffffffff");
            // AND operation
            Value *v = buildop2("and", ctx[rname], v1);
            return v;
        }
    }
    // 16-bit sub-registers: ax, bx, cx, dx, si, di, bp, sp
    else if (s == "ax" || s == "bx" || s == "cx" || s == "dx" ||
             s == "si" || s == "di" || s == "bp" || s == "sp") {
        string rname;
        if (s == "si" || s == "di" || s == "bp" || s == "sp")
            rname = "r" + s; // "si" -> "rsi"
        else
            rname = "r" + s.substr(0, 1) + "x"; // "ax" -> "rax"

        if (hasVal(ctx[rname], 0, 15)) {
            return readVal(ctx[rname], 0, 15);
        } else {
            // Create a concrete mask for the lower 16 bits
            Value *v1 = new Value(CONCRETE, "0x000000000000ffff");
            // AND operation
            Value *v = buildop2("and", ctx[rname], v1);
            return v;
        }
    }
    // 8-bit sub-registers: al, bl, cl, dl, sil, dil, bpl, spl
    else if (s == "al" || s == "bl" || s == "cl" || s == "dl" ||
             s == "sil" || s == "dil" || s == "bpl" || s == "spl") {
        string rname;
        if (s == "sil" || s == "dil" || s == "bpl" || s == "spl")
            rname = "r" + s.substr(0, 1) + "si"; // e.g., "sil" -> "rsi"
        else
            rname = "r" + s.substr(0, 1) + "x"; // "al" -> "rax"

        // Create a concrete mask for the lower 8 bits
        Value *v1 = new Value(CONCRETE, "0x00000000000000ff");
        // AND operation
        Value *v = buildop2("and", ctx[rname], v1);
        return v;
    }
    // 8-bit high registers: ah, bh, ch, dh
    else if (s == "ah" || s == "bh" || s == "ch" || s == "dh") {
        string rname = "r" + s.substr(0, 1) + "x"; // "ah" -> "rax"
        // Create a mask for bits 8-15
        Value *v1 = new Value(CONCRETE, "0x000000000000ff00");
        Value *v2 = buildop2("and", ctx[rname], v1);
        // Shift right by 8 bits
        Value *v3 = new Value(CONCRETE, "0x8");
        Value *v = buildop2("shr", v2, v3);
        return v;
    }
    else {
        cerr << "Unknown register name: " << s << endl;
        return nullptr;
    }
}

void SEEngine::writeReg(const string &s, Value *v) {
    // 64-bit full register
    if (s == "rax" || s == "rbx" || s == "rcx" || s == "rdx" ||
        s == "rsi" || s == "rdi" || s == "rsp" || s == "rbp") {
        ctx[s] = v;
    }
    // 32-bit sub-registers: eax, ebx, ecx, edx
    else if (s == "eax" || s == "ebx" || s == "ecx" || s == "edx") {
        string rname = "r" + s.substr(1); // "eax" -> "rax"
        // Mask out the lower 32 bits
        Value *mask = new Value(CONCRETE, "0xffffffff00000000");
        Value *masked = buildop2("and", ctx[rname], mask);
        // OR with the new value
        Value *new_val = buildop2("or", masked, v);
        ctx[rname] = new_val;
    }
    // 16-bit sub-registers: ax, bx, cx, dx, si, di, bp, sp
    else if (s == "ax" || s == "bx" || s == "cx" || s == "dx" ||
             s == "si" || s == "di" || s == "bp" || s == "sp") {
        string rname;
        if (s == "si" || s == "di" || s == "bp" || s == "sp")
            rname = "r" + s; // "si" -> "rsi"
        else
            rname = "r" + s.substr(0, 1) + "x"; // "ax" -> "rax"

        // Mask out the lower 16 bits
        Value *mask = new Value(CONCRETE, "0x000000000000ffff");
        Value *masked = buildop2("and", ctx[rname], mask);
        // OR with the new value
        Value *new_val = buildop2("or", masked, v);
        ctx[rname] = new_val;
    }
    // 8-bit low registers: al, bl, cl, dl, sil, dil, bpl, spl
    else if (s == "al" || s == "bl" || s == "cl" || s == "dl" ||
             s == "sil" || s == "dil" || s == "bpl" || s == "spl") {
        string rname;
        if (s == "sil" || s == "dil" || s == "bpl" || s == "spl")
            rname = "r" + s.substr(0, 1) + "si"; // e.g., "sil" -> "rsi"
        else
            rname = "r" + s.substr(0, 1) + "x"; // "al" -> "rax"

        // Mask out the lower 8 bits
        Value *mask = new Value(CONCRETE, "0x00000000000000ff");
        Value *masked = buildop2("and", ctx[rname], mask);
        // OR with the new value
        Value *new_val = buildop2("or", masked, v);
        ctx[rname] = new_val;
    }
    // 8-bit high registers: ah, bh, ch, dh
    else if (s == "ah" || s == "bh" || s == "ch" || s == "dh") {
        string rname = "r" + s.substr(0, 1) + "x"; // "ah" -> "rax"
        // Mask out bits 8-15
        Value *mask = new Value(CONCRETE, "0x000000000000ff00");
        Value *masked = buildop2("and", ctx[rname], mask);
        // Shift left by 8 bits
        Value *shift_val = new Value(CONCRETE, "0x8");
        Value *shifted = buildop2("shl", v, shift_val);
        // OR with the masked value
        Value *new_val = buildop2("or", masked, shifted);
        ctx[rname] = new_val;
    }
    else {
        cerr << "Unknown register name to write: " << s << endl;
    }
}

Value* SEEngine::readMem(ADDR64 addr, int nbyte) {
    ADDR64 end = addr + nbyte - 1;
    AddrRange ar(addr, end), res;

    // If exact range is found in mem, return it
    if (memfind(ar)) return mem[ar];

    // If range is brand new, create a new symbolic Value
    if (isnew(ar)) {
        Value *v = new Value(SYMBOL, nbyte * 8); // nbyte*8 bits
        mem[ar] = v;
        meminput[v] = ar;
        return v;
    }
    // If it is a subset of an existing range
    else if (issubset(ar, &res)) {
        ADDR64 b1 = ar.start, e1 = ar.end;
        ADDR64 b2 = res.start, e2 = res.end;

        // Create a mask for the subset
        // e.g., for [b1, e1] within [b2, e2], mask bits outside [b1, e1] as 0
        // Assuming little-endian and contiguous memory
        // Construct the mask as a concrete value
        // Example: if [b2, e2] = [0x1000, 0x1007], [b1, e1] = [0x1002, 0x1005]
        // Mask = 0x0000ff00ffff0000 for 8 bytes
        // This example uses bitmask construction for simplicity

        // Calculate the bitmask
        // Each byte is 8 bits, so total bits = (e2 - b2 + 1) * 8
        int total_bytes = e2 - b2 + 1;
        int total_bits = total_bytes * 8;
        bitset<64> mask_bs;
        mask_bs.reset();

        for (ADDR64 i = b2; i <= e2; ++i) {
            if (i >= b1 && i <= e1)
                mask_bs[i - b2] = 1;
        }

        stringstream mask_ss;
        mask_ss << "0x" << hex << mask_bs.to_ullong();

        Value *v0 = mem[res];
        Value *v1 = new Value(CONCRETE, mask_ss.str());
        Value *v2 = buildop2("and", v0, v1);
        // Shift right by (b1 - b2)*8 bits to align
        ADDR64 shift_bits = (b1 - b2) * 8;
        stringstream shift_ss;
        shift_ss << "0x" << hex << shift_bits;
        Value *v3 = new Value(CONCRETE, shift_ss.str());
        Value *v4 = buildop2("shr", v2, v3); // shift right
        return v4;
    }
    else {
        cerr << "readMem: Partial overlapping symbolic memory access not implemented!\n";
        return nullptr;
    }
}

void SEEngine::writeMem(ADDR64 addr, int nbyte, Value *v) {
    ADDR64 end = addr + nbyte - 1;
    AddrRange ar(addr, end), res;

    // If exact range or new
    if (memfind(ar) || isnew(ar)) {
        mem[ar] = v;
        return;
    }
    // If new range is a superset of an existing range
    else if (issuperset(ar, &res)) {
        mem.erase(res);
        mem[ar] = v;
        return;
    }
    // If new range is subset of an existing range
    else if (issubset(ar, &res)) {
        ADDR64 b1 = ar.start, e1 = ar.end;
        ADDR64 b2 = res.start, e2 = res.end;

        // Create a mask to zero out [b1..e1] in the old value
        bitset<64> mask_bs;
        mask_bs.reset();
        for (ADDR64 i = b2; i <= e2; ++i) {
            if (i >= b1 && i <= e1)
                mask_bs[i - b2] = 0; // Zero out these bits
            else
                mask_bs[i - b2] = 1; // Keep these bits
        }

        stringstream mask_ss;
        mask_ss << "0x" << hex << mask_bs.to_ullong();

        Value *v0 = mem[res];
        Value *v1 = new Value(CONCRETE, mask_ss.str());
        Value *v2 = buildop2("and", v0, v1);
        // Shift the new value left by (b1 - b2)*8 bits
        ADDR64 shift_bits = (b1 - b2) * 8;
        stringstream shift_ss;
        shift_ss << "0x" << hex << shift_bits;
        Value *v3 = new Value(CONCRETE, shift_ss.str());
        Value *v4 = buildop2("shl", v, v3);
        // OR the masked old value with the shifted new value
        Value *v5 = buildop2("or", v2, v4);
        mem[res] = v5;
    }
    else {
        cerr << "writeMem: Partial overlapping symbolic memory access not implemented!\n";
    }
}

ADDR64 SEEngine::getRegConVal(const string &reg) const {
    // Retrieve the concrete value from the instruction's context register
    if (reg == "rax")
        return ip->ctxreg[0];
    else if (reg == "rbx")
        return ip->ctxreg[1];
    else if (reg == "rcx")
        return ip->ctxreg[2];
    else if (reg == "rdx")
        return ip->ctxreg[3];
    else if (reg == "rsi")
        return ip->ctxreg[4];
    else if (reg == "rdi")
        return ip->ctxreg[5];
    else if (reg == "rsp")
        return ip->ctxreg[6];
    else if (reg == "rbp")
        return ip->ctxreg[7];
    else {
        cerr << "Now only get 64-bit register's concrete value for [rax..rbp]." << endl;
        return 0;
    }
}

ADDR64 SEEngine::calcAddr(const Operand &opr) const {
    ADDR64 r1, r2, c;
    int64_t n;
    switch (opr.tag) {
    case 7: // addr7 = r1 + r2*n + c
        r1 = getRegConVal(opr.field[0]);
        r2 = getRegConVal(opr.field[1]);
        n  = stoll(opr.field[2]);
        c  = stoull(opr.field[4], nullptr, 16);
        if (opr.field[3] == "+")
            return r1 + (r2 * n) + c;
        else if (opr.field[3] == "-")
            return r1 + (r2 * n) - c;
        else {
            cerr << "Unrecognized addr: tag 7" << endl;
            return 0;
        }
    case 4: // addr4 = r1 + c
        r1 = getRegConVal(opr.field[0]);
        c = stoull(opr.field[2], nullptr, 16);
        if (opr.field[1] == "+")
            return r1 + c;
        else if (opr.field[1] == "-")
            return r1 - c;
        else {
            cerr << "Unrecognized addr: tag 4" << endl;
            return 0;
        }
    case 5: // addr5 = r1 + r2*n
        r1 = getRegConVal(opr.field[0]);
        r2 = getRegConVal(opr.field[1]);
        n  = stoll(opr.field[2]);
        return r1 + r2 * n;
    case 6: // addr6 = r2*n + c
        r2 = getRegConVal(opr.field[0]);
        n = stoll(opr.field[1]);
        c = stoull(opr.field[3], nullptr, 16);
        if (opr.field[2] == "+")
            return r2 * n + c;
        else if (opr.field[2] == "-")
            return r2 * n - c;
        else {
            cerr << "Unrecognized addr: tag 6" << endl;
            return 0;
        }
    case 3: // addr3 = r2*n
        r2 = getRegConVal(opr.field[0]);
        n = stoll(opr.field[1]);
        return r2 * n;
    case 1: // addr1 = c
        c = stoull(opr.field[0], nullptr, 16);
        return c;
    case 2: // addr2 = r1
        r1 = getRegConVal(opr.field[0]);
        return r1;
    default:
        cerr << "Unrecognized addr tag" << endl;
        return 0;
    }
}

int SEEngine::symexec() {
    for (auto it = start; it != end; ++it) {
        ip = it;

        // Skip no-effect instructions
        if (noeffectinst.find(it->opcstr) != noeffectinst.end())
            continue;

        switch (it->oprnum) {
        case 0:
            // No operands
            break;
        case 1:
        {
            Operand *op0 = it->oprd[0];
            Value *v0, *res, *temp;
            int nbyte;

            if (it->opcstr == "push") {
                if (op0->ty == IMM) {
                    v0 = new Value(CONCRETE, op0->field[0]);
                    writeMem(it->waddr, 8, v0); // 64-bit push => 8 bytes
                }
                else if (op0->ty == REG) {
                    nbyte = op0->bit / 8;
                    temp = readReg(op0->field[0]);
                    writeMem(it->waddr, nbyte, temp);
                }
                else if (op0->ty == MEM) {
                    nbyte = op0->bit / 8;
                    v0 = readMem(it->raddr, nbyte);
                    writeMem(it->waddr, nbyte, v0);
                }
                else {
                    cerr << "push error: operand not Imm/Reg/Mem!" << endl;
                    return 1;
                }
            }
            else if (it->opcstr == "pop") {
                if (op0->ty == REG) {
                    nbyte = op0->bit / 8;
                    temp = readMem(it->raddr, nbyte);
                    writeReg(op0->field[0], temp);
                }
                else if (op0->ty == MEM) {
                    nbyte = op0->bit / 8;
                    temp = readMem(it->raddr, nbyte);
                    writeMem(it->waddr, nbyte, temp);
                }
                else {
                    cerr << "pop error: operand not Reg/Mem!" << endl;
                    return 1;
                }
            }
            else {
                // 1-operand instructions, e.g., "neg", "inc", etc.
                if (op0->ty == REG) {
                    v0 = readReg(op0->field[0]);
                    res = buildop1(it->opcstr, v0);
                    writeReg(op0->field[0], res);
                }
                else if (op0->ty == MEM) {
                    nbyte = op0->bit / 8;
                    v0 = readMem(it->raddr, nbyte);
                    res = buildop1(it->opcstr, v0);
                    writeMem(it->waddr, nbyte, res);
                }
                else {
                    cerr << "[Error] " << it->id << ": Unknown 1-op instruction!\n";
                    return 1;
                }
            }
            break;
        }
        case 2:
        {
            Operand *op0 = it->oprd[0];
            Operand *op1 = it->oprd[1];
            Value *v0, *v1, *res, *temp;
            int nbyte;

            if (it->opcstr == "mov") {
                // mov
                if (op0->ty == REG) {
                    if (op1->ty == IMM) {
                        v1 = new Value(CONCRETE, op1->field[0]);
                        writeReg(op0->field[0], v1);
                    }
                    else if (op1->ty == REG) {
                        temp = readReg(op1->field[0]);
                        writeReg(op0->field[0], temp);
                    }
                    else if (op1->ty == MEM) {
                        nbyte = op1->bit / 8;
                        v1 = readMem(it->raddr, nbyte);
                        writeReg(op0->field[0], v1);
                    }
                    else {
                        cerr << "mov error: op1 not Imm/Reg/Mem\n";
                        return 1;
                    }
                }
                else if (op0->ty == MEM) {
                    if (op1->ty == IMM) {
                        temp = new Value(CONCRETE, op1->field[0]);
                        nbyte = op0->bit / 8;
                        writeMem(it->waddr, nbyte, temp);
                    }
                    else if (op1->ty == REG) {
                        temp = readReg(op1->field[0]);
                        nbyte = op0->bit / 8;
                        writeMem(it->waddr, nbyte, temp);
                    }
                    else {
                        cerr << "mov error: op0=MEM, op1 not Imm/Reg\n";
                        return 1;
                    }
                }
                else {
                    cerr << "mov error: op0 is not REG or MEM\n";
                    return 1;
                }
            }
            else if (it->opcstr == "lea") {
                // lea
                if (op0->ty != REG || op1->ty != MEM) {
                    cerr << "lea format error!\n";
                    return 1;
                }
                switch (op1->tag) {
                case 5:
                {
                    // e.g., lea reg, [r1 + r2*n]
                    Value *f0 = readReg(op1->field[0]);
                    Value *f1 = readReg(op1->field[1]);
                    Value *f2 = new Value(CONCRETE, op1->field[2]);
                    Value *imul = buildop2("imul", f1, f2);
                    Value *add = buildop2("add", f0, imul);
                    writeReg(op0->field[0], add);
                    break;
                }
                // Add other cases (tags) as needed
                default:
                    cerr << "lea error: unhandled address tag: " << op1->tag << endl;
                    return 1;
                }
            }
            else if (it->opcstr == "xchg") {
                // xchg
                if (op1->ty == REG) {
                    Value *v1 = readReg(op1->field[0]);
                    if (op0->ty == REG) {
                        Value *v0 = readReg(op0->field[0]);
                        writeReg(op1->field[0], v0);
                        writeReg(op0->field[0], v1);
                    }
                    else if (op0->ty == MEM) {
                        nbyte = op0->bit / 8;
                        Value *v0 = readMem(it->raddr, nbyte);
                        writeReg(op1->field[0], v0);
                        writeMem(it->waddr, nbyte, v1);
                    }
                    else {
                        cerr << "xchg error: op0 not REG or MEM\n";
                        return 1;
                    }
                }
                else if (op1->ty == MEM) {
                    nbyte = op1->bit / 8;
                    Value *v1 = readMem(it->raddr, nbyte);
                    if (op0->ty == REG) {
                        Value *v0 = readReg(op0->field[0]);
                        writeReg(op0->field[0], v1);
                        writeMem(it->waddr, nbyte, v0);
                    }
                    else {
                        cerr << "xchg error: op0 not REG\n";
                        return 1;
                    }
                }
                else {
                    cerr << "xchg error: op1 not REG or MEM\n";
                    return 1;
                }
            }
            else {
                // Handle other 2-operand instructions: add, sub, xor, and, or, shl, shr, etc.
                // 1) Handle second operand as source
                if (op1->ty == IMM) {
                    v1 = new Value(CONCRETE, op1->field[0]);
                }
                else if (op1->ty == REG) {
                    v1 = readReg(op1->field[0]);
                }
                else if (op1->ty == MEM) {
                    nbyte = op1->bit / 8;
                    v1 = readMem(it->raddr, nbyte);
                }
                else {
                    cerr << "2-op error: op1 not IMM/REG/MEM\n";
                    return 1;
                }

                // 2) Handle first operand as source and destination
                if (op0->ty == REG) {
                    v0 = readReg(op0->field[0]);
                    res = buildop2(it->opcstr, v0, v1);
                    writeReg(op0->field[0], res);
                }
                else if (op0->ty == MEM) {
                    nbyte = op0->bit / 8;
                    v0 = readMem(it->raddr, nbyte);
                    res = buildop2(it->opcstr, v0, v1);
                    writeMem(it->waddr, nbyte, res);
                }
                else {
                    cerr << "2-op error: op0 not REG or MEM\n";
                    return 1;
                }
            }
            break;
        }
        case 3:
        {
            // Three-operand instructions, e.g., "imul reg, reg, imm"
            Operand *op0 = it->oprd[0];
            Operand *op1 = it->oprd[1];
            Operand *op2 = it->oprd[2];
            Value *v1, *v2, *res;

            if (it->opcstr == "imul" &&
                op0->ty == REG &&
                op1->ty == REG &&
                op2->ty == IMM)
            {
                v1 = readReg(op1->field[0]);
                v2 = new Value(CONCRETE, op2->field[0]);
                res = buildop2("imul", v1, v2);
                writeReg(op0->field[0], res);
            }
            else {
                cerr << "[3-op error] Unrecognized pattern, e.g., 'imul reg, reg, imm'\n";
                return 1;
            }
            break;
        }
        default:
            cerr << "All instructions: operand count > 3 not handled!\n";
            break;
        }
    }
    return 0;
}

// Recursive function to traverse and print a Value's formula
void traverse(Value *v) {
    if (!v) return;
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE)
            cout << v->conval;
        else
            cout << "sym" << v->id;
    }
    else {
        cout << "(" << op->opty << " ";
        traverse(op->val[0]);
        cout << " ";
        traverse(op->val[1]);
        cout << ")";
    }
}

// Another traverse function for debugging with hybrid values
void traverse2(Value *v) {
    if (!v) return;
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE)
            cout << v->conval;
        else if (v->valty == SYMBOL)
            cout << "sym" << v->id << " ";
        else if (v->valty == HYBRID) {
            cout << "[hyb" << v->id << " ";
            for (auto &kv : v->childs) {
                cout << "[" << kv.first.first << "," << kv.first.second << "]:";
                traverse2(kv.second);
            }
            cout << "]";
        }
        else {
            cout << "unknown type\n";
            return;
        }
    }
    else {
        cout << "(" << op->opty << " ";
        traverse2(op->val[0]);
        cout << " ";
        traverse2(op->val[1]);
        cout << ")";
    }
}

// Output the formula for a given register
void SEEngine::outputFormula(string reg) {
    Value *v = ctx[reg];
    if (!v) {
        cout << reg << " is null\n";
        return;
    }
    cout << "sym" << v->id << " =\n";
    traverse(v);
    cout << endl;
}

// Dump the formula of a register for debugging
void SEEngine::dumpreg(string reg) {
    Value *v = ctx[reg];
    if (!v) {
        cout << "reg " << reg << " is null\n";
        return;
    }
    cout << "reg " << reg << " =\n";
    traverse2(v);
    cout << endl;
}

// Gather all Values from registers + memory that have a non-null Operation
vector<Value*> SEEngine::getAllOutput() {
    vector<Value*> outputs;
    // Check registers
    {
        Value *v = ctx["rax"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rbx"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rcx"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rdx"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rsi"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rdi"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rsp"];
        if (v && v->opr) outputs.push_back(v);
        v = ctx["rbp"];
        if (v && v->opr) outputs.push_back(v);
    }
    // Check memory
    for (auto const& x : mem) {
        Value *v = x.second;
        if (v && v->opr)
            outputs.push_back(v);
    }

    return outputs;
}

// Print all register formulas
void SEEngine::printAllRegFormulas() {
    // Iterate through all registers
    vector<string> regs = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp"};
    for (const auto &reg : regs) {
        cout << reg << ": ";
        outputFormula(reg);
        printInputSymbols(reg);
        cout << endl;
    }
}

// Print all memory formulas
void SEEngine::printAllMemFormulas() {
    for (auto &x : mem) {
        AddrRange ar = x.first;
        Value *v = x.second;
        if (!v) continue;
        printf("[0x%llx,0x%llx]: ", (long long)ar.start, (long long)ar.end);
        cout << "sym" << v->id << "=\n";
        traverse(v);
        cout << endl << endl;
    }
}

// BFS to get all symbolic inputs for Value *output
set<Value*>* getInputs(Value *output) {
    queue<Value*> que;
    que.push(output);

    set<Value*> *inputset = new set<Value*>;

    while (!que.empty()) {
        Value *v = que.front();
        que.pop();
        Operation *op = v->opr;
        if (!op) {
            if (v->valty == SYMBOL)
                inputset->insert(v);
        }
        else {
            for (int i = 0; i < 3; ++i) {
                if (op->val[i]) que.push(op->val[i]);
            }
        }
    }
    return inputset;
}

// Print input symbols for a given register
void SEEngine::printInputSymbols(string reg) {
    Value *v = ctx[reg];
    if (!v) {
        cout << reg << " is null\n";
        return;
    }
    set<Value*> *insyms = getInputs(v);
    cout << insyms->size() << " input symbols: ";
    for (auto &symval : *insyms) {
        cout << "sym" << symval->id << " ";
    }
    cout << endl;
}

// Print formula details: which memory/reg inputs feed into Value *v
void SEEngine::printformula(Value *v) {
    set<Value*> *insyms = getInputs(v);

    cout << insyms->size() << " input symbols: " << endl;
    for (auto &symval : *insyms) {
        cout << "sym" << symval->id << ": ";
        auto it1 = meminput.find(symval);
        if (it1 != meminput.end()) {
            printf("[0x%llx, 0x%llx]\n",
                   (long long)it1->second.start,
                   (long long)it1->second.end);
        }
        else {
            auto it2 = reginput.find(symval);
            if (it2 != reginput.end()) {
                cout << it2->second << endl;
            }
            else {
                cout << "Unknown input source\n";
            }
        }
    }
    cout << endl;

    cout << "sym" << v->id << "=\n";
    traverse(v);
    cout << endl;
}

// Print memory formula for address range [addr1, addr2)
void SEEngine::printMemFormula(ADDR64 addr1, ADDR64 addr2) {
    AddrRange ar(addr1, addr2);
    auto it = mem.find(ar);
    if (it == mem.end()) {
        cout << "No memory formula for [0x" << hex << addr1
             << ", 0x" << addr2 << "]" << endl;
        return;
    }
    Value *v = it->second;
    printformula(v);
}

// Recursive function to evaluate a Value with concrete inputs
static ADDR64 eval(Value *v, map<Value*, ADDR64> *inmap) {
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE) {
            return stoull(v->conval, nullptr, 16);
        }
        else {
            // Symbolic input
            auto it = inmap->find(v);
            if (it != inmap->end())
                return it->second;
            else {
                cerr << "Concrete value not found for symbolic input sym" << v->id << endl;
                return 0;
            }
        }
    }
    else {
        ADDR64 op0 = 0, op1 = 0;
        if (op->val[0]) op0 = eval(op->val[0], inmap);
        if (op->val[1]) op1 = eval(op->val[1], inmap);

        if (op->opty == "add") {
            return op0 + op1;
        }
        else if (op->opty == "sub") {
            return op0 - op1;
        }
        else if (op->opty == "imul") {
            return op0 * op1;
        }
        else if (op->opty == "xor") {
            return op0 ^ op1;
        }
        else if (op->opty == "and") {
            return op0 & op1;
        }
        else if (op->opty == "or") {
            return op0 | op1;
        }
        else if (op->opty == "shl") {
            return op0 << op1;
        }
        else if (op->opty == "shr") {
            return op0 >> op1;
        }
        else if (op->opty == "neg") {
            return static_cast<ADDR64>(-static_cast<int64_t>(op0));
        }
        else if (op->opty == "inc") {
            return op0 + 1;
        }
        else {
            cerr << "Instruction: " << op->opty << " is not interpreted!\n";
            return 0;
        }
    }
}

// Execute a formula v with a set of symbolic inputs inmap
ADDR64 SEEngine::conexec(Value *f, map<Value*, ADDR64> *inmap) {
    set<Value*> *inputsym = getInputs(f);
    set<Value*> inmapkeys;
    for (auto &pair : *inmap) {
        inmapkeys.insert(pair.first);
    }
    // Quick check (not robust) that all symbolic inputs are in inmap
    if (inmapkeys != *inputsym) {
        cout << "Some inputs don't have parameters!\n";
        return 1;
    }
    return eval(f, inmap);
}

// Build an input map from a vector of Values to a vector of 64-bit addresses
map<Value*, ADDR64> buildinmap(vector<Value*> *vv, vector<ADDR64> *input) {
    map<Value*, ADDR64> inmap;
    if (vv->size() != input->size()) {
        cout << "Mismatch in # of input symbols!\n";
        return inmap;
    }
    for (size_t i = 0; i < vv->size(); i++) {
        inmap.insert({(*vv)[i], (*input)[i]});
    }
    return inmap;
}

// Gather all input Values from a symbolic expression
vector<Value*> getInputVector(Value *f) {
    set<Value*> *inset = getInputs(f);
    vector<Value*> vv;
    for (auto &symval : *inset) {
        vv.push_back(symval);
    }
    return vv;
}

// A global postfix for naming (used in external helper functions)
string sympostfix;

// Output the calculation of v in a simplistic CVC-like style
static void outputCVC(Value *v, FILE *fp) {
    if (!v) return;
    Operation *op = v->opr;
    if (!op) {
        if (v->valty == CONCRETE) {
            uint64_t i = stoull(v->conval, nullptr, 16);
            fprintf(fp, "0x%016llx", (long long)i);
        }
        else {
            fprintf(fp, "sym%d%s", v->id, sympostfix.c_str());
        }
    }
    else {
        if (op->opty == "add") {
            fprintf(fp, "(bvadd sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "sub") {
            fprintf(fp, "(bvsub sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "imul") {
            fprintf(fp, "(bvmul sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "xor") {
            fprintf(fp, "(bvxor sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "and") {
            fprintf(fp, "(bvand sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "or") {
            fprintf(fp, "(bvor sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "shl") {
            fprintf(fp, "(bvshl sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "shr") {
            fprintf(fp, "(bvlshr sym%d sym%d)", op->val[0]->id, op->val[1]->id);
        }
        else if (op->opty == "neg") {
            fprintf(fp, "(bvneg sym%d)", op->val[0]->id);
        }
        else if (op->opty == "inc") {
            // Assuming "inc" is equivalent to adding 1
            fprintf(fp, "(bvadd sym%d 0x0000000000000001)", op->val[0]->id);
        }
        else {
            cout << "Instruction: " << op->opty << " not interpreted in CVC!\n";
        }
    }
}

// Output the formula for a given register to "formula.cvc"
void outputCVCFormula(Value *f) {
    const char *cvcfile = "formula.cvc";
    FILE *fp = fopen(cvcfile, "w");
    if (!fp) {
        cerr << "Cannot open formula.cvc for writing!\n";
        return;
    }
    outputCVC(f, fp);
    fclose(fp);
}

// Output equality check between two symbolic Values in CVC format
void outputChkEqCVC(Value *f1, Value *f2, map<int, int> *m) {
    const char *cvcfile = "ChkEq.cvc";
    FILE *fp = fopen(cvcfile, "w");
    if (!fp) {
        cerr << "Cannot open ChkEq.cvc!\n";
        return;
    }

    // Declare symbolic variables based on the mapping
    for (auto &pr : *m) {
        fprintf(fp, "(declare-fun sym%d () (_ BitVec 64))\n", pr.first);
        fprintf(fp, "(declare-fun sym%d () (_ BitVec 64))\n", pr.second);
    }

    // Assert that symda equals symdb for each mapping pair
    for (auto &pr : *m) {
        fprintf(fp, "(assert (= sym%d sym%d))\n", pr.first, pr.second);
    }

    // Query if f1 equals f2
    fprintf(fp, "\n(assert (= ");
    sympostfix = "a";
    outputCVC(f1, fp);
    fprintf(fp, " ");
    sympostfix = "b";
    outputCVC(f2, fp);
    fprintf(fp, "))\n");
    fprintf(fp, "(check-sat)\n");
    fprintf(fp, "(get-model)\n");
    fclose(fp);
}

// Output bit-level comparisons between two symbolic Values in CVC format
void outputBitCVC(Value *f1, Value *f2,
                 vector<Value*> *inv1, vector<Value*> *inv2,
                 list<FullMap> *result) {
    // This function requires a more detailed implementation based on the specific use-case.
    // Below is a simplified placeholder implementation.

    int n = 1;
    for (auto &mpair : *result) {
        string cvcfile = "formula" + to_string(n++) + ".cvc";
        FILE *fp = fopen(cvcfile.c_str(), "w");
        if (!fp) {
            cerr << "Cannot open " << cvcfile << "!\n";
            continue;
        }

        map<int, int> *inmap = &(mpair.first);
        map<int, int> *outmap = &(mpair.second);

        // Declare bit variables (simplified)
        for (int i = 0; i < 64 * (int)inv1->size(); i++) {
            fprintf(fp, "(declare-fun bit%da () (_ BitVec 1))\n", i);
        }
        for (int i = 0; i < 64 * (int)inv2->size(); i++) {
            fprintf(fp, "(declare-fun bit%db () (_ BitVec 1))\n", i);
        }

        // Input mapping assertions
        for (auto &pr : *inmap) {
            fprintf(fp, "(assert (= bit%da bit%db))\n", pr.first, pr.second);
        }

        // Placeholder for the actual bit-level operations
        // This needs to be implemented based on the specific symbolic execution requirements

        fclose(fp);
    }
}

// Show memory inputs
void SEEngine::showMemInput() {
    cout << "Inputs in memory:\n";
    for (auto &mp : meminput) {
        Value *symv = mp.first;
        AddrRange rng = mp.second;
        printf("sym%d: [0x%llx, 0x%llx]\n", symv->id, (long long)rng.start, (long long)rng.end);
    }
    cout << endl;
}
