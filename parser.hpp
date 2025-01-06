// parser.hpp
#ifndef PARSER_HPP
#define PARSER_HPP

#include <list>
#include <string>
#include <fstream>
#include "core.hpp"

// Parses operands for instructions in the range [begin, end)
void parseOperand(std::list<Inst>::iterator begin, std::list<Inst>::iterator end);

// Parses the trace file and populates the instruction list
void parseTrace(std::ifstream &infile, std::list<Inst> &L);

// Prints the first 3 instructions for debugging
void printFirst3Inst(const std::list<Inst> &L);

// Prints the trace in LLSE format (semicolon-separated)
void printTraceLLSE(const std::list<Inst> &L, const std::string &fname);

// Prints the trace in a human-readable format
void printTraceHuman(const std::list<Inst> &L, const std::string &fname);

#endif // PARSER_HPP
