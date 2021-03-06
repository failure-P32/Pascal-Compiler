#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include "utils/ast.hpp"
#include "utils/ASTvis.hpp"
#include "utils/ASTopt.hpp"
#include "codegen/codegen_context.hpp"
#include "parser.hpp"

extern FILE *yyin;
std::shared_ptr<spc::ProgramNode> program;

void emit_target(llvm::raw_fd_ostream &dest, llvm::TargetMachine::CodeGenFileType type, llvm::Module &module)
{
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto target_triple = llvm::sys::getDefaultTargetTriple();
    module.setTargetTriple(target_triple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target)
    { llvm::errs() << error; exit(1); }

    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto rm = llvm::Optional<llvm::Reloc::Model>();
    auto target_machine = target->createTargetMachine(target_triple, cpu, features, opt, rm);
    module.setDataLayout(target_machine->createDataLayout());

    llvm::legacy::PassManager pass;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, type))
    {
        llvm::errs() << "The target machine cannot emit an object file";
        exit(1);
    }

    llvm::verifyModule(module, &llvm::errs());
    pass.run(module);

    dest.flush();
}

int main(int argc, char* argv[])
{
    enum Target { UNDEFINED, LLVM, ASM, OBJ };

    Target target = Target::UNDEFINED;
    char *input = nullptr, *outputP = nullptr;
    bool opt = false, optAst = false;
    bool printTable = false;
    bool printLLVM = false;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-ir") == 0) target = Target::LLVM;
        else if (strcmp(argv[i], "-S") == 0) target = Target::ASM;
        else if (strcmp(argv[i], "-c") == 0) target = Target::OBJ;
        else if (strcmp(argv[i], "-O") == 0) opt = true;
        else if (strcmp(argv[i], "-opt-ast") == 0) optAst = true;
        else if (strcmp(argv[i], "-print-table") == 0) printTable = true;
        else if (strcmp(argv[i], "-print-llvm") == 0) printLLVM = true;
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (i == argc - 1) 
            {
                std::cerr << "Error: unspecified output file" << std::endl;
                exit(1);
            }
            outputP = argv[++i];
        }
        else if (argv[i][0] == '-')
        { 
            fprintf(stderr, "Error: unknown argument: %s", argv[i]); 
            exit(1); 
        }
        else input = argv[i];
    }
    if (target == Target::UNDEFINED || input == nullptr)
    {
        puts("USAGE: spc <option> <input file>");
        puts("OPTION:");
        puts("  -ir                  Emit LLVM assembly code (.ll)");
        puts("  -S                   Emit assembly code (.s)");
        puts("  -c                   Emit object code (.o)");
        puts(" [-o <output file>]    Specify output file");
        puts(" [-O]                  Enable LLVM optimizations");
        puts(" [-opt-ast]            Enable AST optimizations");
        puts(" [-print-table]        Print the symbol table");
        puts(" [-print-llvm]         Print the LLVM IR");
        exit(1);
    }

    if ((yyin = fopen(input, "r")) == nullptr)
    {
        std::cerr << "Error: cannot open iutput file" << std::endl;
        exit(1);
    }

    spc::parser pars;

    try
    {
        pars.parse();
    }
    catch(const std::invalid_argument& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << "Terminated due to error during scanning" << std::endl;
        exit(1);
    }
    catch(const std::logic_error& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << "Terminated due to error during parsing" << std::endl;
        exit(1);
    }

    std::cout << "Scanning & Parsing completed!" << std::endl;

    if (optAst)
    {
        spc::ASTopt astOpt;
        astOpt(spc::cast_node<spc::BaseRoutineNode>(program));
    }
    

    std::string astVisName = input;
    astVisName.erase(astVisName.rfind('.'));
    astVisName.append(".output.tex");
    spc::ASTvis astVis(astVisName);
    astVis.travAST(program);

    std::cout << "AST verification completed! Output AST structure to " << astVisName << std::endl;

    spc::CodegenContext genContext("main", opt);
    try 
    {
        program->codegen(genContext);
    } 
    catch (spc::CodegenException &e) 
    {
        std::cerr << "[CODEGEN ERROR] ";
        std::cerr << e.what() << std::endl;
        std::cerr << "Terminated due to error during code generation" << std::endl;
        if (genContext.log().is_open()) genContext.log().close();
        abort();
    }

    if (printTable)
    {
        genContext.printGlobals();
        std::cout << std::endl;
        genContext.printFuncs();
        std::cout << std::endl;
        genContext.printAliases();
        std::cout << std::endl;
        genContext.printArrAliases();
        std::cout << std::endl;
        genContext.printArrTable();
        std::cout << std::endl;
        genContext.printRecAliases();
        std::cout << std::endl;
        genContext.printConsts();
        std::cout << std::endl;
        genContext.printLocals();
        std::cout << std::endl;
        genContext.printConstVals();
        std::cout << std::endl;
    }
    if (printLLVM)
        genContext.dump();
    std::cout << "Code generation completed!" << std::endl;

    std::string output;
    if (outputP == nullptr)
        output = input;
    else
        output = outputP;
    output.erase(output.rfind('.'));
    switch (target)
    {
        case Target::LLVM: output.append(".ll"); break;
        case Target::ASM:  output.append(".s");  break;
        case Target::OBJ:  output.append(".o");  break;
        default: break;
    }
    std::error_code ec;
    llvm::raw_fd_ostream fd(output, ec, llvm::sys::fs::F_None);
    if (ec)
    { 
        llvm::errs() << "Could not open file: " << ec.message(); 
        exit(1); 
    }

    switch (target)
    {
        case Target::LLVM: genContext.getModule()->print(fd, nullptr); break;
        case Target::ASM: emit_target(fd, llvm::TargetMachine::CGFT_AssemblyFile, *(genContext.getModule())); break;
        case Target::OBJ: emit_target(fd, llvm::TargetMachine::CGFT_ObjectFile, *(genContext.getModule())); break;
        default: break;
    }
    std::cout << "Compile result output: " << output << std::endl;

    return 0;
}
