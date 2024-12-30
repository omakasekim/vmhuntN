/*
 * A pin tool to record all instructions in a binary execution (64-bit version).
 */

#include <stdio.h>
#include <pin.H>
#include <map>
#include <iostream>
#include <string>

const char *tracefile = "instrace.txt";
std::map<ADDRINT, std::string> opcmap;
FILE *fp;

// This function is called before every instruction is executed.
// It logs the instruction pointer (addr), the disassembled instruction,
// the 64-bit register values, and the memory read/write addresses (if any).
void getctx(ADDRINT addr, CONTEXT *fromctx, ADDRINT raddr, ADDRINT waddr)
{
    // Use "%llx" for 64-bit hex printing and cast to unsigned long long
    fprintf(fp, 
        "%llx;%s;"
        "%llx,%llx,%llx,%llx,%llx,%llx,%llx,%llx,"
        "%llx,%llx,\n",
        (unsigned long long)addr,                  // Instruction address
        opcmap[addr].c_str(),                      // Disassembled instruction
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RAX),
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RBX),
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RCX),
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RDX),
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RSI),
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RDI),
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RSP),
        (unsigned long long)PIN_GetContextReg(fromctx, REG_RBP),
        (unsigned long long)raddr,                // Memory read address
        (unsigned long long)waddr                 // Memory write address
    );
}

// Instrumentation callback: called once for every instruction Pin sees
static void instruction(INS ins, void *v)
{
    ADDRINT addr = INS_Address(ins);

    // If this address has not been encountered, store its disassembly
    if (opcmap.find(addr) == opcmap.end()) {
        opcmap.insert(std::make_pair(addr, INS_Disassemble(ins)));
    }

    // Insert a call to 'getctx' BEFORE the instruction is executed
    if (INS_IsMemoryRead(ins) && INS_IsMemoryWrite(ins)) {
        INS_InsertCall(
            ins, IPOINT_BEFORE,
            (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_MEMORYREAD_EA, 
            IARG_MEMORYWRITE_EA,
            IARG_END
        );
    }
    else if (INS_IsMemoryRead(ins)) {
        INS_InsertCall(
            ins, IPOINT_BEFORE,
            (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_MEMORYREAD_EA,
            IARG_ADDRINT, 0,
            IARG_END
        );
    }
    else if (INS_IsMemoryWrite(ins)) {
        INS_InsertCall(
            ins, IPOINT_BEFORE,
            (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_ADDRINT, 0,
            IARG_MEMORYWRITE_EA,
            IARG_END
        );
    }
    else {
        INS_InsertCall(
            ins, IPOINT_BEFORE,
            (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_ADDRINT, 0,
            IARG_ADDRINT, 0,
            IARG_END
        );
    }
}

// Called when the application exits
static void on_fini(INT32 code, void *v)
{
    fclose(fp);
}

int main(int argc, char *argv[])
{
    // Initialize Pin
    if (PIN_Init(argc, argv)) {
        fprintf(stderr, "PIN command line initialization error\n");
        return 1;
    }

    // Open trace file
    fp = fopen(tracefile, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", tracefile);
        return 1;
    }

    // Initialize symbol processing (optional, but often helpful)
    PIN_InitSymbols();

    // Register functions to be called
    PIN_AddFiniFunction(on_fini, 0);
    INS_AddInstrumentFunction(instruction, NULL);

    // Start the program; this call never returns
    PIN_StartProgram();

    return 0;
}
