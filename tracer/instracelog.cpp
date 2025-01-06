/*
 * A Pin tool to record all instructions in a 64-bit binary execution.
 *
 * Build (example):
 *   pin -t ./obj-intel64/instracelog.so -- /path/to/64-bit-program
 *
 */

#include <stdio.h>
#include <pin.H>      // The main Pin include
#include <map>
#include <iostream>
#include <string>

/*
 * The output trace file.
 */
static const char *tracefile = "instrace.txt";

/*
 * A map from instruction address to disassembly string (for logging).
 */
static std::map<ADDRINT, std::string> opcmap;

/*
 * Our output file pointer.
 */
static FILE *fp = nullptr;

/*
 * getctx: Called before each instruction to dump:
 *  - Instruction address
 *  - Disassembly
 *  - 64-bit register values (RAX..RBP)
 *  - Memory read/write addresses
 */
static VOID getctx(ADDRINT addr, CONTEXT *fromctx, ADDRINT raddr, ADDRINT waddr)
{
    // Print addresses in 64-bit hex with leading zeros => "%016llx".
    // We also cast to (unsigned long long) for the format specifier.
    // We fetch 64-bit GPRs: RAX, RBX, RCX, RDX, RSI, RDI, RSP, RBP.
    fprintf(fp,
            "%016llx;%s;"
            "%016llx,%016llx,%016llx,%016llx,%016llx,%016llx,%016llx,%016llx,"
            "%016llx,%016llx,\n",
            (unsigned long long)addr,
            opcmap[addr].c_str(),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RAX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RBX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RCX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RDX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RSI),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RDI),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RSP),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RBP),
            (unsigned long long)raddr,
            (unsigned long long)waddr
    );
}

/*
 * instruction: Pin calls this for every static instruction once,
 * letting us insert the function getctx(...) before the instruction executes.
 */
static VOID instruction(INS ins, VOID *v)
{
    ADDRINT addr = INS_Address(ins);

    // If we haven’t seen this address, store its disassembly in opcmap
    if (opcmap.find(addr) == opcmap.end()) {
        opcmap[addr] = INS_Disassemble(ins);
    }

    // We want to gather both read and write addresses if the instruction does both,
    // only read if it does a read, only write if it does a write, etc.
    if (INS_IsMemoryRead(ins) && INS_IsMemoryWrite(ins)) {
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_MEMORYREAD_EA,
            IARG_MEMORYWRITE_EA,
            IARG_END
        );
    }
    else if (INS_IsMemoryRead(ins)) {
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_MEMORYREAD_EA,
            // We pass zero for the write address
            IARG_ADDRINT, (ADDRINT)0,
            IARG_END
        );
    }
    else if (INS_IsMemoryWrite(ins)) {
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            // We pass zero for the read address
            IARG_ADDRINT, (ADDRINT)0,
            IARG_MEMORYWRITE_EA,
            IARG_END
        );
    }
    else {
        // No memory read/write => pass both as zero
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_ADDRINT, (ADDRINT)0,
            IARG_ADDRINT, (ADDRINT)0,
            IARG_END
        );
    }
}

/*
 * on_fini: Called at the end of the program.
 */
static VOID on_fini(INT32 code, VOID *v)
{
    if (fp) {
        fclose(fp);
        fp = nullptr;
    }
}

/*
 * main: The Pin tool entry point
 */
int main(int argc, char *argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) {
        std::cerr << "PIN_Init failed, check command line.\n";
        return 1;
    }

    // Open the output file
    fp = fopen(tracefile, "w");
    if (!fp) {
        std::cerr << "Failed to open " << tracefile << " for writing.\n";
        return 1;
    }

    // Initialize symbol processing
    PIN_InitSymbols();

    // Register our finalization callback
    PIN_AddFiniFunction(on_fini, 0);

    // Register our instrumentation function for instructions
    INS_AddInstrumentFunction(instruction, nullptr);

    // Start the program (never returns)
    PIN_StartProgram();

    return 0;
}
