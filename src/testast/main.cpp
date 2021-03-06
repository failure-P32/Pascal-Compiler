#include <iostream>
#include "utils/ast.hpp"
#include "ASTvis.hpp"
#include "parser.hpp"

extern FILE *yyin;
std::shared_ptr<spc::ProgramNode> program;

int main(int argc, char *argv[])
{
    if (argc < 2) return 1;
    yyin = fopen(argv[1], "r");
    parser p;
    p.parse();
    spc::ASTvis vis;
    vis.travAST(program);
    return 0;
}