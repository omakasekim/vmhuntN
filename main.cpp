#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <cstdint>  // for 64-bit types like uint64_t

using namespace std;

// Your headers, now presumably updated for 64-bit operation:
#include "core.hpp"
#include "mg-symengine.hpp"
#include "parser.hpp"

list<Inst> instlist1, instlist2;  // all instructions in the trace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <target>\n", argv[0]);
        return 1;
    }

    ifstream infile1(argv[1]);
    if (!infile1.is_open()) {
        fprintf(stderr, "Open file error!\n");
        return 1;
    }

    // Parse the trace from the input file (assumed 64-bit capable)
    parseTrace(&infile1, &instlist1);
    infile1.close();

    // Parse operands (assumed 64-bit capable)
    parseOperand(instlist1.begin(), instlist1.end());

    // Create our symbolic execution engine (64-bit)
    SEEngine *se1 = new SEEngine();

    // Initialize all 64-bit registers as symbolic
    se1->initAllRegSymol(instlist1.begin(), instlist1.end());

    // Execute symbolically
    se1->symexec();

    // Dump the formula (now for "rax" instead of "eax")
    se1->dumpreg("rax");

    return 0;
}
