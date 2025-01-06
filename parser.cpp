// parser.cpp
#include "parser.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <regex>
#include <algorithm> // for std::transform

using namespace std;

// Helper: big pattern for "64-bit registers" (r0..r15 plus classic ones)
static const char* reg64_pattern = R"(rip|r(1[0-5]|[8-9])|rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)";

// Function to create a memory operand from a string
static Operand createAddrOperand(const string& s)
{
    // Build subpatterns using reg64_pattern
    string r64 = "(" + string(reg64_pattern) + ")";
    
    // Define regex patterns with expanded register set
    regex addr1(R"(0x[[:xdigit:]]+)", regex::icase);
    regex addr2(r64, regex::icase);  // single 64-bit reg
    regex addr3("(" + r64 + R"(\*([[:digit:]]+))", regex::icase); // e.g., rax*2
    regex addr4("(" + r64 + R"(([+-])(0x[[:xdigit:]]+))", regex::icase); // e.g., rax+0x10 or rax-0x123
    regex addr5("(" + r64 + R"(\+)" + r64 + R"(\*([[:digit:]]+))", regex::icase); // e.g., rax+rbx*2
    regex addr6("(" + r64 + R"(\*([[:digit:]]+))([+-])(0x[[:xdigit:]]+))", regex::icase); // e.g., rax*2+0x10 or rax*2-0x10
    regex addr7("(" + r64 + R"(\+)" + r64 + R"(\*([[:digit:]]+))([+-])(0x[[:xdigit:]]+))", regex::icase); // e.g., rax+rbx*2+0x10

    smatch m;
    Operand opr;
    
    if      (regex_match(s, m, addr7)) {
        // e.g., rax+rbx*2+0x10
        opr.ty = Operand::MEM;
        opr.tag = 7;
        opr.field[0] = m[1]; // first reg
        opr.field[1] = m[2]; // second reg
        opr.field[2] = m[3]; // scale
        opr.field[3] = m[4]; // + or -
        opr.field[4] = m[5]; // displacement
    }
    else if (regex_match(s, m, addr6)) {
        // e.g., rax*2+0x10 or rax*2-0x10
        opr.ty = Operand::MEM;
        opr.tag = 6;
        opr.field[0] = m[1]; // reg
        opr.field[1] = m[2]; // scale
        opr.field[2] = m[3]; // + or -
        opr.field[3] = m[4]; // displacement
    }
    else if (regex_match(s, m, addr5)) {
        // e.g., rax+rbx*2
        opr.ty = Operand::MEM;
        opr.tag = 5;
        opr.field[0] = m[1]; // first reg
        opr.field[1] = m[2]; // second reg
        opr.field[2] = m[3]; // scale
    }
    else if (regex_match(s, m, addr4)) {
        // e.g., rax+0x10 or rax-0x123
        opr.ty = Operand::MEM;
        opr.tag = 4;
        opr.field[0] = m[1]; // reg
        opr.field[1] = m[2]; // + or -
        opr.field[2] = m[3]; // displacement
    }
    else if (regex_match(s, m, addr3)) {
        // e.g., rax*2
        opr.ty = Operand::MEM;
        opr.tag = 3;
        opr.field[0] = m[1]; // reg
        opr.field[1] = m[2]; // scale
    }
    else if (regex_match(s, m, addr1)) {
        // e.g., 0x12345678
        opr.ty = Operand::MEM;
        opr.tag = 1;
        opr.field[0] = m[0];
    }
    else if (regex_match(s, m, addr2)) {
        // e.g., rax or rip
        opr.ty = Operand::MEM;
        opr.tag = 2;
        opr.field[0] = m[0];
    }
    else {
        cerr << "Unknown memory operand format: " << s << endl;
        opr.ty = Operand::UNK;
    }
    
    return opr;
}

// Function to create a data operand (register or immediate) from a string
static Operand createDataOperand(const string& s)
{
    // Define regex patterns for different register sizes
    regex reg64(R"(rip|r(1[0-5]|[8-9])|rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)", regex::icase);
    regex reg32(R"(r(1[0-5]|[8-9])d|eax|ebx|ecx|edx|esi|edi|esp|ebp)", regex::icase);
    regex reg16(R"(r(1[0-5]|[8-9])w|ax|bx|cx|dx|si|di|sp|bp)", regex::icase);
    regex reg8(R"(r(1[0-5]|[8-9])b|al|ah|bl|bh|cl|ch|dl|dh|spl|bpl|sil|dil)", regex::icase);
    regex immvalue(R"(0x[[:xdigit:]]+)", regex::icase);
    
    smatch m;
    Operand opr;
    
    if      (regex_match(s, m, reg64)) {
        // e.g., rax or rip
        opr.ty = Operand::REG;
        if (s.substr(0, 3) == "xmm" || s.substr(0, 3) == "ymm" || s.substr(0, 3) == "zmm") {
            // Vector registers will be handled separately if needed
            // Assuming they are already part of reg64 in core.hpp
            opr.bit = 64; // Update if vector registers have different bit sizes
        }
        else {
            opr.bit = 64;
        }
        opr.field[0] = m[0];
    }
    else if (regex_match(s, m, reg32)) {
        // e.g., eax
        opr.ty = Operand::REG;
        opr.bit = 32;
        opr.field[0] = m[0];
    }
    else if (regex_match(s, m, reg16)) {
        // e.g., ax
        opr.ty = Operand::REG;
        opr.bit = 16;
        opr.field[0] = m[0];
    }
    else if (regex_match(s, m, reg8)) {
        // e.g., al
        opr.ty = Operand::REG;
        opr.bit = 8;
        opr.field[0] = m[0];
    }
    else if (regex_match(s, m, immvalue)) {
        // e.g., 0x1A2B
        opr.ty = Operand::IMM;
        opr.bit = 64; // Assuming 64-bit immediate
        opr.field[0] = m[0];
    }
    else {
        cerr << "Unknown data operand format: " << s << endl;
        opr.ty = Operand::UNK;
    }
    
    return opr;
}

// Parses the trace file and populates the instruction list
void parseTrace(ifstream *infile, list<Inst> *L)
{
    string line;
    int num = 1;

    while (getline(*infile, line)) {
        if (line.empty()) continue;

        istringstream strbuf(line);
        string temp, disasstr;

        Inst ins; // Use stack allocation to avoid manual memory management
        ins.id = num++;

        // 1) Instruction address (string) up to ';'
        if (!getline(strbuf, ins.addr, ';')) {
            cerr << "Failed to read instruction address from line: " << line << endl;
            continue;
        }

        try {
            ins.addrn = stoull(ins.addr, nullptr, 16); // convert to 64-bit
        } catch (const std::invalid_argument& e) {
            cerr << "Invalid address format: " << ins.addr << " in line: " << line << endl;
            continue;
        } catch (const std::out_of_range& e) {
            cerr << "Address out of range: " << ins.addr << " in line: " << line << endl;
            continue;
        }

        // 2) Disassembly string
        if (!getline(strbuf, disasstr, ';')) {
            cerr << "Failed to read disassembly from line: " << line << endl;
            continue;
        }
        ins.assembly = disasstr;

        // Extract opcode (ins.opcstr) from the first token in disasstr
        istringstream disasbuf(disasstr);
        if (!(disasbuf >> ins.opcstr)) {
            cerr << "Failed to extract opcode from disassembly: " << disasstr << endl;
            continue;
        }

        // Extract the comma-separated operands
        while (getline(disasbuf, temp, ',')) {
            // Trim leading/trailing spaces
            size_t start = temp.find_first_not_of(" \t");
            size_t end = temp.find_last_not_of(" \t");
            if (start != string::npos && end != string::npos) {
                temp = temp.substr(start, end - start + 1);
            } else {
                temp = ""; // All whitespace
            }
            if (!temp.empty()) {
                ins.oprs.push_back(temp);
            }
        }
        ins.oprnum = ins.oprs.size();

        // 3) Next 8 comma-separated hex strings => context registers
        for (int i = 0; i < 8; i++) {
            if (!getline(strbuf, temp, ',')) {
                cerr << "Insufficient context registers in line: " << line << endl;
                break; 
            }
            try {
                ins.ctxreg[i] = stoull(temp, nullptr, 16);
            } catch (const std::invalid_argument& e) {
                cerr << "Invalid context register value: " << temp << " in line: " << line << endl;
                ins.ctxreg[i] = 0;
            } catch (const std::out_of_range& e) {
                cerr << "Context register value out of range: " << temp << " in line: " << line << endl;
                ins.ctxreg[i] = 0;
            }
        }

        // 4) Next 2 for read/write addresses
        if (getline(strbuf, temp, ',')) {
            try {
                ins.raddr = stoull(temp, nullptr, 16);
            } catch (...) {
                cerr << "Invalid read address: " << temp << " in line: " << line << endl;
                ins.raddr = 0;
            }
        }
        if (getline(strbuf, temp, ',')) {
            try {
                ins.waddr = stoull(temp, nullptr, 16);
            } catch (...) {
                cerr << "Invalid write address: " << temp << " in line: " << line << endl;
                ins.waddr = 0;
            }
        }

        // Push the instruction into the list
        L->push_back(ins);
    }
}

// Parses operands for instructions in the range [begin, end)
void parseOperand(list<Inst>::iterator begin, list<Inst>::iterator end)
{
    for (auto it = begin; it != end; ++it) {
        for (size_t i = 0; i < it->oprs.size() && i < 3; ++i) {
            const string& oprStr = it->oprs[i];
            Operand opr;
            if (oprStr.find('[') != string::npos || oprStr.find(']') != string::npos || oprStr.find("ptr") != string::npos) {
                // Memory operand
                size_t start = oprStr.find('[');
                size_t end_pos = oprStr.rfind(']');
                if (start != string::npos && end_pos != string::npos && end_pos > start) {
                    string inside = oprStr.substr(start + 1, end_pos - start - 1);
                    opr = createAddrOperand(inside);
                } else {
                    cerr << "Malformed memory operand: " << oprStr << endl;
                    opr.ty = Operand::UNK;
                }
            }
            else {
                // Register or immediate
                opr = createDataOperand(oprStr);
            }

            // Assign the parsed Operand to oprd[i]
            it->oprd[i] = opr;

            // Optionally, populate src and dst vectors based on the opcode
            // This requires opcode analysis which isn't detailed here
            // Example:
            /*
            if (it->opcstr == "mov") {
                if (opr.ty == Operand::REG) {
                    it->adddst(Parameter::REG, opr.field[0]);
                }
                else if (opr.ty == Operand::IMM || opr.ty == Operand::MEM) {
                    it->addsrc(/* ... */);
                }
            }
            */
        }

        // Handle instructions with zero operands (e.g., nop)
        if (it->oprs.empty()) {
            // For 'nop', you might want to log or handle differently
            // Currently, nothing is done, which is acceptable
            // Ensure that oprd array remains default
            // Example:
            // cout << "Instruction ID " << it->id << " is a 'nop' with zero operands.\n";
        }
    }
}

// Prints the first 3 instructions for debugging
void printfirst3inst(list<Inst> *L)
{
    int count = 0;
    for (const auto& inst : *L) {
        if (count >= 3) break;
        cout << "ID: " << inst.id << ", Address: " << inst.addr << ", Assembly: " << inst.assembly << endl;
        cout << "Operands: ";
        for (const auto& opr : inst.oprs) {
            cout << opr << " ";
        }
        cout << "\nParsed Operands:\n";
        for (int i = 0; i < 3; ++i) {
            if (inst.oprd[i].ty == Operand::UNK) continue;
            cout << "  Operand " << i+1 << ": ";
            if (inst.oprd[i].ty == Operand::REG) {
                cout << "REG (" << inst.oprd[i].field[0] << "), " << inst.oprd[i].bit << " bits\n";
            }
            else if (inst.oprd[i].ty == Operand::IMM) {
                cout << "IMM (" << inst.oprd[i].field[0] << "), " << inst.oprd[i].bit << " bits\n";
            }
            else if (inst.oprd[i].ty == Operand::MEM) {
                cout << "MEM, Tag " << inst.oprd[i].tag << ", Fields: ";
                for (int j = 0; j < 5; ++j) {
                    if (!inst.oprd[i].field[j].empty()) {
                        cout << inst.oprd[i].field[j] << " ";
                    }
                }
                cout << ", " << inst.oprd[i].bit << " bits\n";
            }
        }
        cout << "Context Registers: ";
        for (int j = 0; j < 8; j++) {
            printf("0x%llx ", (unsigned long long)inst.ctxreg[j]);
        }
        printf("\nRead Addr: 0x%llx, Write Addr: 0x%llx\n\n",
               (unsigned long long)inst.raddr,
               (unsigned long long)inst.waddr);
        count++;
    }
}

// Prints the trace in LLSE format (semicolon-separated)
void printTraceLLSE(const list<Inst> &L, const string &fname)
{
    ofstream outfile(fname);
    if (!outfile.is_open()) {
        cerr << "Failed to open " << fname << " for writing.\n";
        return;
    }
    for (const auto& inst : L) {
        outfile << inst.id << ";" << inst.addr << ";" << inst.assembly << ";";
        for (int i = 0; i < 8; i++) {
            outfile << "0x" << hex << inst.ctxreg[i] << ",";
        }
        outfile << "0x" << hex << inst.raddr << ",0x" << hex << inst.waddr << ";\n";
    }
    outfile.close();
}

// Prints the trace in a human-readable format
void printTraceHuman(const list<Inst> &L, const string &fname)
{
    ofstream outfile(fname);
    if (!outfile.is_open()) {
        cerr << "Failed to open " << fname << " for writing.\n";
        return;
    }
    for (const auto& inst : L) {
        outfile << "ID: " << inst.id << ", Address: " << inst.addr << ", Assembly: " << inst.assembly << "\n";
        outfile << "Operands: ";
        for (const auto& opr : inst.oprs) {
            outfile << opr << " ";
        }
        outfile << "\nParsed Operands:\n";
        for (int i = 0; i < 3; ++i) {
            if (inst.oprd[i].ty == Operand::UNK) continue;
            outfile << "  Operand " << i+1 << ": ";
            if (inst.oprd[i].ty == Operand::REG) {
                outfile << "REG (" << inst.oprd[i].field[0] << "), " << inst.oprd[i].bit << " bits\n";
            }
            else if (inst.oprd[i].ty == Operand::IMM) {
                outfile << "IMM (" << inst.oprd[i].field[0] << "), " << inst.oprd[i].bit << " bits\n";
            }
            else if (inst.oprd[i].ty == Operand::MEM) {
                outfile << "MEM, Tag " << inst.oprd[i].tag << ", Fields: ";
                for (int j = 0; j < 5; ++j) {
                    if (!inst.oprd[i].field[j].empty()) {
                        outfile << inst.oprd[i].field[j] << " ";
                    }
                }
                outfile << ", " << inst.oprd[i].bit << " bits\n";
            }
        }
        outfile << "Context Registers: ";
        for (int j = 0; j < 8; j++) {
            outfile << "0x" << hex << inst.ctxreg[j] << " ";
        }
        outfile << "\nRead Addr: 0x" << hex << inst.raddr << ", Write Addr: 0x" << hex << inst.waddr << "\n\n";
    }
    outfile.close();
}
