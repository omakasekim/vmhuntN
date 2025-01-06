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
#include <mutex>      // For std::mutex
#include <iomanip>    // For std::hex and std::setw

/*
 * The output trace file.
 */
static const char *tracefile = "tracefile.txt";

/*
 * A map from instruction address to disassembly string (for logging).
 * Protected by a mutex for thread safety.
 */
static std::map<ADDRINT, std::string> opcmap;
static std::mutex opcmap_mutex;

/*
 * Our output file pointer.
 * Protected by a mutex for thread safety.
 */
static FILE *fp = nullptr;
static std::mutex file_mutex;

/*
 * Sentinel value for "no memory access"
 */
static const ADDRINT NO_ACCESS = 0xFFFFFFFFFFFFFFFF;

/*
 * getctx: Called before each instruction to dump:
 *  - Instruction address
 *  - Disassembly
 *  - 64-bit register values (RAX..RBP)
 *  - Memory read/write addresses
 */
static VOID getctx(ADDRINT addr, CONTEXT *fromctx, ADDRINT raddr, ADDRINT waddr)
{
    std::lock_guard<std::mutex> lock(file_mutex); // Ensure thread-safe access

    // Fetch disassembly from opcmap
    std::string disasm;
    {
        std::lock_guard<std::mutex> lock_map(opcmap_mutex);
        auto it = opcmap.find(addr);
        if (it != opcmap.end()) {
            disasm = it->second;
        }
        else {
            disasm = "UNKNOWN";
        }
    }

    // Replace "0" with sentinel for no access
    ADDRINT read_addr = (raddr == 0) ? NO_ACCESS : raddr;
    ADDRINT write_addr = (waddr == 0) ? NO_ACCESS : waddr;

    // Print addresses in 64-bit hex with leading zeros => "%016llx".
    // We also cast to (unsigned long long) for the format specifier.
    // We fetch 64-bit GPRs: RAX, RBX, RCX, RDX, RSI, RDI, RSP, RBP.
    fprintf(fp,
            "%016llx,\"%s\","
            "%016llx,%016llx,%016llx,%016llx,%016llx,%016llx,%016llx,%016llx,"
            "%016llx,%016llx\n",
            (unsigned long long)addr,
            disasm.c_str(),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RAX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RBX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RCX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RDX),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RSI),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RDI),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RSP),
            (unsigned long long)PIN_GetContextReg(fromctx, REG_RBP),
            (unsigned long long)read_addr,
            (unsigned long long)write_addr
    );

    fflush(fp); // Ensure data is written immediately
}

/*
 * instruction: Pin calls this for every static instruction once,
 * letting us insert the function getctx(...) before the instruction executes.
 */
static VOID instruction(INS ins, VOID *v)
{
    ADDRINT addr = INS_Address(ins);

    // Insert disassembly into opcmap if not already present
    {
        std::lock_guard<std::mutex> lock(opcmap_mutex);
        if (opcmap.find(addr) == opcmap.end()) {
            opcmap[addr] = INS_Disassemble(ins);
        }
    }

    // Determine memory access types and insert appropriate calls
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
            IARG_ADDRINT, NO_ACCESS,
            IARG_END
        );
    }
    else if (INS_IsMemoryWrite(ins)) {
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_ADDRINT, NO_ACCESS,
            IARG_MEMORYWRITE_EA,
            IARG_END
        );
    }
    else {
        // No memory read/write => pass sentinel values
        INS_InsertCall(
            ins, IPOINT_BEFORE, (AFUNPTR)getctx,
            IARG_INST_PTR,
            IARG_CONST_CONTEXT,
            IARG_ADDRINT, NO_ACCESS,
            IARG_ADDRINT, NO_ACCESS,
            IARG_END
        );
    }
}

/*
 * on_fini: Called at the end of the program.
 */
static VOID on_fini(INT32 code, VOID *v)
{
    std::lock_guard<std::mutex> lock(file_mutex); // Ensure thread-safe access
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

    // Write header to tracefile.txt
    {
        std::lock_guard<std::mutex> lock(file_mutex);
        fprintf(fp,
                "Instruction_Address,Disassembly,"
                "RAX,RBX,RCX,RDX,RSI,RDI,RSP,RBP,"
                "Memory_Read_EA,Memory_Write_EA\n"
        );
        fflush(fp);
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
