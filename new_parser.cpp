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

using namespace std;

#include "new_core.hpp"
#include "parser.hpp"

Operand *createAddrOperand(string s)
{
    // regular expressions addresses
    regex addr1("0x[[:xdigit:]]+");
    regex addr2("rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp");
    regex addr3("(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)\\*([[:digit:]])");

    regex addr4("(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)(\\+|-)(0x[[:xdigit:]]+)");
    regex addr5("(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)\\+(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)\\*([[:digit:]])");
    regex addr6("(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)\\*([[:digit:]])(\\+|-)(0x[[:xdigit:]]+)");

    regex addr7("(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)\\+(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp)\\*([[:digit:]])(\\+|-)(0x[[:xdigit:]]+)");

    Operand *opr = new Operand();
    smatch m;

    if (regex_search(s, m, addr7)) { // addr7: rax+rbx*2+0xfffffffffffff1
        opr->ty = Operand::MEM;
        opr->tag = 7;
        opr->field[0] = m[1]; // rax
        opr->field[1] = m[2]; // rbx
        opr->field[2] = m[3]; // 2
        opr->field[3] = m[4]; // + or -
        opr->field[4] = m[5]; // 0xfffffffffffff1
    } else if (regex_search(s, m, addr4)) { // addr4: rax+0xfffffffffffff1
        opr->ty = Operand::MEM;
        opr->tag = 4;
        opr->field[0] = m[1]; // rax
        opr->field[1] = m[2]; // + or -
        opr->field[2] = m[3]; // 0xfffffffffffff1
    } else if (regex_search(s, m, addr5)) { // addr5: rax+rbx*2
        opr->ty = Operand::MEM;
        opr->tag = 5;
        opr->field[0] = m[1]; // rax
        opr->field[1] = m[2]; // rbx
        opr->field[2] = m[3]; // 2
    } else if (regex_search(s, m, addr6)) { // addr6: rax*2+0xfffffffffffff1
        opr->ty = Operand::MEM;
        opr->tag = 6;
        opr->field[0] = m[1]; // rax
        opr->field[1] = m[2]; // 2
        opr->field[2] = m[3]; // + or -
        opr->field[3] = m[4]; // 0xfffffffffffff1
    } else if (regex_search(s, m, addr3)) { // addr3: rax*2
        opr->ty = Operand::MEM;
        opr->tag = 3;
        opr->field[0] = m[1]; // rax
        opr->field[1] = m[2]; // 2
    } else if (regex_search(s, m, addr1)) { // addr1: Immediate value address
        opr->ty = Operand::MEM;
        opr->tag = 1;
        opr->field[0] = m[0];
    } else if (regex_search(s, m, addr2)) { // addr2: 64-bit register address
        opr->ty = Operand::MEM;
        opr->tag = 2;
        opr->field[0] = m[0];
    } else {
        cout << "Unknown addr operands: " << s << endl;
    }

    return opr;
}

Operand *createDataOperand(string s)
{
    regex immvalue("0x[[:xdigit:]]+");
    regex reg8("al|ah|bl|bh|cl|ch|dl|dh");
    regex reg16("ax|bx|cx|dx|si|di|bp|cs|ds|es|fs|gs|ss");
    regex reg32("eax|ebx|ecx|edx|esi|edi|esp|ebp");
    regex reg64("rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp");

    Operand *opr = new Operand();
    smatch m;
    if (regex_search(s, m, reg64)) { // 64-bit register
        opr->ty = Operand::REG;
        opr->bit = 64;
        opr->field[0] = m[0];
    } else if (regex_search(s, m, reg32)) { // 32-bit register
        opr->ty = Operand::REG;
        opr->bit = 32;
        opr->field[0] = m[0];
    } else if (regex_search(s, m, reg16)) { // 16-bit register
        opr->ty = Operand::REG;
        opr->bit = 16;
        opr->field[0] = m[0];
    } else if (regex_search(s, m, reg8)) { // 8-bit register
        opr->ty = Operand::REG;
        opr->bit = 8;
        opr->field[0] = m[0];
    } else if (regex_search(s, m, immvalue)) { // Immediate value
        opr->ty = Operand::IMM;
        opr->bit = 64;
        opr->field[0] = m[0];
    } else {
        cout << "Unknown data operands: " << s << endl;
    }

    return opr;
}

void parseTrace(ifstream *infile, list<Inst> *L)
{
    string line;
    int num = 1;

    while (infile->good()) {
        getline(*infile, line);
        if (line.empty()) { continue; }

        istringstream strbuf(line);
        string temp, disasstr;

        Inst *ins = new Inst();
        ins->id = num++;

        getline(strbuf, ins->addr, ';');
        ins->addrn = stoull(ins->addr, 0, 16); // 64-bit address

        getline(strbuf, disasstr, ';');
        ins->assembly = disasstr;

        istringstream disasbuf(disasstr);
        getline(disasbuf, ins->opcstr, ' ');

        while (disasbuf.good()) {
            getline(disasbuf, temp, ',');
            if (temp.find_first_not_of(' ') != string::npos)
                ins->oprs.push_back(temp);
        }
        ins->oprnum = ins->oprs.size();

        // Parse 8 context register values (64-bit)
        for (int i = 0; i < 8; ++i) {
            getline(strbuf, temp, ',');
            ins->ctxreg[i] = stoull(temp, 0, 16);
        }

        // Parse memory access addresses (64-bit)
        getline(strbuf, temp, ',');
        ins->raddr = stoull(temp, 0, 16);
        getline(strbuf, temp, ',');
        ins->waddr = stoull(temp, 0, 16);

        L->push_back(*ins);
    }
}
