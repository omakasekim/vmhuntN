#ifndef PARSER_HPP
#define PARSER_HPP


#include <fstream>
#include <list>
#include <string>

#include "core.hpp"
using namespace std;
void parseOperand(list<Inst>::iterator begin, list<Inst>::iterator end);
void parseTrace(ifstream *infile, list<Inst> *L);
void printfirst3inst(list<Inst> *L);
void printTraceLLSE(list<Inst> &L, string fname);
void printTraceHuman(list<Inst> &L, string fname);

#endif 
