#include "utils/ast.hpp"
#include "codegen_context.hpp"

namespace spc
{

    llvm::Value *VarDeclNode::createGlobalArray(CodegenContext &context, const std::shared_ptr<ArrayTypeNode> &arrTy)
    {
        // std::shared_ptr<ArrayTypeNode> arrTy = cast_node<ArrayTypeNode>(this->type);
        std::cout << "Creating array" << std::endl;
        auto *ty = arrTy->itemType->getLLVMType(context);
        llvm::Constant *z; // zero
        if (ty->isIntegerTy()) 
            z = llvm::ConstantInt::get(ty, 0);
        else if (ty->isDoubleTy())
            z = llvm::ConstantFP::get(ty, 0.0);
        else 
            throw CodegenException("Unknown type");

        llvm::ConstantInt *startIdx = llvm::dyn_cast<llvm::ConstantInt>(arrTy->range_start->codegen(context));
        if (!startIdx)
            throw CodegenException("Start index invalid");
        else if (startIdx->getSExtValue() < 0)
            throw CodegenException("Start index must be greater than zero!");
        
        int len = 0;
        llvm::ConstantInt *endIdx = llvm::dyn_cast<llvm::ConstantInt>(arrTy->range_end->codegen(context));
        if (!endIdx || endIdx->getSExtValue() < 0)
            throw CodegenException("End index invalid");
        else if (endIdx->getBitWidth() <= 32)
            len = endIdx->getSExtValue() + 1;
        else
            throw CodegenException("End index overflow");

        llvm::ArrayType* arr = llvm::ArrayType::get(ty, len);
        std::vector<llvm::Constant *> initVector;
        for (int i = 0; i < len; i++)
            initVector.push_back(z);
        auto *variable = llvm::ConstantArray::get(arr, initVector);

        std::cout << "Created array" << std::endl;

        return new llvm::GlobalVariable(*context.getModule(), variable->getType(), false, llvm::GlobalVariable::ExternalLinkage, variable, this->name->name);
    }

    llvm::Value *VarDeclNode::createArray(CodegenContext &context, const std::shared_ptr<ArrayTypeNode> &arrTy)
    {
        // std::shared_ptr<ArrayTypeNode> arrTy = cast_node<ArrayTypeNode>(this->type);
        std::cout << "Creating array" << std::endl;
        auto *ty = arrTy->itemType->getLLVMType(context);
        llvm::Constant *constant;
        if (ty->isIntegerTy()) 
            constant = llvm::ConstantInt::get(ty, 0);
        else if (ty->isDoubleTy())
            constant = llvm::ConstantFP::get(ty, 0.0);
        else if (ty->isStructTy())
            constant = nullptr;
        else
            throw CodegenException("Unknown type");

        llvm::ConstantInt *startIdx = llvm::dyn_cast<llvm::ConstantInt>(arrTy->range_start->codegen(context));
        if (!startIdx)
            throw CodegenException("Start index invalid");
        else if (startIdx->getSExtValue() < 0)
            throw CodegenException("Start index must be greater than zero!");
        
        int len = 0;
        llvm::ConstantInt *endIdx = llvm::dyn_cast<llvm::ConstantInt>(arrTy->range_end->codegen(context));
        if (!endIdx || endIdx->getSExtValue() < 0)
            throw CodegenException("End index invalid");
        else if (endIdx->getBitWidth() <= 32)
            len = endIdx->getSExtValue() + 1;
        else
            throw CodegenException("End index overflow");

        llvm::ConstantInt *space = llvm::ConstantInt::get(context.getBuilder().getInt32Ty(), len);
        auto *local = context.getBuilder().CreateAlloca(ty, space);
        auto success = context.setLocal(context.getTrace() + "_" + this->name->name, local);
        if (!success) throw CodegenException("Duplicate identifier in var section of function " + context.getTrace() + ": " + this->name->name);
        std::cout << "Created array" << std::endl;
        return local;
    }
    
    llvm::Value *VarDeclNode::codegen(CodegenContext &context)
    {
        if (context.is_subroutine)
        {
            if (type->type == Type::Alias)
            {
                std::string aliasName = cast_node<AliasTypeNode>(type)->name->name;
                std::cout << "Searching alias " << std::endl;
                std::shared_ptr<ArrayTypeNode> arrTy;
                for (auto rit = context.traces.rbegin(); rit != context.traces.rend(); rit++)
                {
                    if ((arrTy = context.getArrayAlias(*rit + "_" + aliasName)) != nullptr)
                        break;
                }
                if (arrTy == nullptr)
                    arrTy = context.getArrayAlias(aliasName);
                if (arrTy != nullptr)
                {
                    std::cout << "Alias is array" << std::endl;
                    return createArray(context, arrTy);
                }
                std::shared_ptr<RecordTypeNode> recTy = context.getRecordAlias(aliasName);
                if (recTy != nullptr)
                {
                    std::cout << "Alias is record" << std::endl;
                    context.setRecordAlias(context.getTrace() + "_" + name->name, recTy);
                }
            }
            if (type->type == Type::Array)
                return createArray(context, cast_node<ArrayTypeNode>(this->type));
            else if (type->type == Type::String)
            {
                auto arrTy = make_node<ArrayTypeNode>(0, 255, Type::Char);
                return createArray(context, arrTy);
            }
            else
            {
                if (type->type == Type::Record)
                    context.setRecordAlias(context.getTrace() + "_" + name->name, cast_node<RecordTypeNode>(type));
                auto *local = context.getBuilder().CreateAlloca(type->getLLVMType(context));
                auto success = context.setLocal(context.getTrace() + "_" + name->name, local);
                if (!success) throw CodegenException("Duplicate identifier in function " + context.getTrace() + ": " + name->name);
                return local;
            }
        }
        else
        {
            if (type->type == Type::Alias)
            {
                std::string aliasName = cast_node<AliasTypeNode>(type)->name->name;
                std::cout << "Searching alias " << std::endl;
                std::shared_ptr<ArrayTypeNode> arrTy = context.getArrayAlias(aliasName);
                if (arrTy != nullptr)
                {
                    std::cout << "Alias is array" << std::endl;
                    return createGlobalArray(context, arrTy);
                }
                std::shared_ptr<RecordTypeNode> recTy = context.getRecordAlias(aliasName);
                if (recTy != nullptr)
                {
                    std::cout << "Alias is record" << std::endl;
                    context.setRecordAlias(name->name, recTy);
                }
            }
            if (type->type == Type::Array)
                return createGlobalArray(context, cast_node<ArrayTypeNode>(this->type));
            else if (type->type == Type::String)
            {
                auto arrTy = make_node<ArrayTypeNode>(0, 255, Type::Char);
                return createGlobalArray(context, arrTy);
            }
            else
            {
                if (type->type == Type::Record)
                    context.setRecordAlias(name->name, cast_node<RecordTypeNode>(type));
                auto *ty = type->getLLVMType(context);
                llvm::Constant *constant;
                if (ty->isIntegerTy()) 
                    constant = llvm::ConstantInt::get(ty, 0);
                else if (ty->isDoubleTy())
                    constant = llvm::ConstantFP::get(ty, 0.0);
                else if (ty->isStructTy())
       	        {
                    std::vector<llvm::Constant*> zeroes;
                    auto *recTy = llvm::cast<llvm::StructType>(ty);
                    for (auto itr = recTy->element_begin(); itr != recTy->element_end(); itr++)
                        zeroes.push_back(llvm::Constant::getNullValue(*itr));
                    constant = llvm::ConstantStruct::get(recTy, zeroes);
                    // int n = recTy->getNumElements();
                    // for (int i = 0; i < n; i++)
                    // {
                    //     zeroes.push_back(llvm::Constant::getNullValue(recTy->getTypeAtIndex(i)));
                    // }
                }
                else
                    throw CodegenException("Unknown type");
                return new llvm::GlobalVariable(*context.getModule(), ty, false, llvm::GlobalVariable::ExternalLinkage, constant, name->name);
            }
        }
    }

    llvm::Value *ConstDeclNode::codegen(CodegenContext &context)
    {
        if (context.is_subroutine)
        {
            if (val->type == Type::String)
            {
                std::cout << "Const string declare" << std::endl;
                auto strVal = cast_node<StringNode>(val);
                auto *constant = llvm::ConstantDataArray::getString(llvm_context, strVal->val, true);
                bool success = context.setLocal(context.getTrace() + "_" + name->name, constant);
                success &= context.setConst(context.getTrace() + "_" + name->name, constant);
                if (!success) throw CodegenException("Duplicate identifier in const section of function " + context.getTrace() + ": " + name->name);
                std::cout << "Added to symbol table" << std::endl;
                auto *gv = new llvm::GlobalVariable(*context.getModule(), constant->getType(), true, llvm::GlobalVariable::ExternalLinkage, constant, context.getTrace() + "_" + name->name);
                std::cout << "Created global variable" << std::endl;
                return gv;
            }
            else
            {
                std::cout << "Const declare" << std::endl;
                auto *constant = llvm::cast<llvm::Constant>(val->codegen(context));
                assert(constant != nullptr);
                bool success = context.setLocal(context.getTrace() + "_" + name->name, constant);
                success &= context.setConst(context.getTrace() + "_" + name->name, constant);
                success &= context.setConstVal(context.getTrace() + "_" + name->name, constant);
                if (!success) throw CodegenException("Duplicate identifier in const section of function " + context.getTrace() + ": " + name->name);
                std::cout << "Added to symbol table" << std::endl;
                // auto *gv = new llvm::GlobalVariable(*context.getModule(), val->getLLVMType(context), true, llvm::GlobalVariable::ExternalLinkage, constant, context.getTrace() + "_" + name->name);
                // std::cout << "Created global variable" << std::endl;
                // return gv;
                return nullptr;
            } 
        }
        else
        {
            if (val->type == Type::String)
            {
                std::cout << "Const string declare" << std::endl;
                auto strVal = cast_node<StringNode>(val);
                auto *constant = llvm::ConstantDataArray::getString(llvm_context, strVal->val, true);
                context.setConst(name->name, constant);
                std::cout << "Added to symbol table" << std::endl;
                auto *gv = new llvm::GlobalVariable(*context.getModule(), constant->getType(), true, llvm::GlobalVariable::ExternalLinkage, constant, name->name);
                std::cout << "Created global variable" << std::endl;
                return gv;
            }
            else
            {
                std::cout << "Const declare" << std::endl;
                auto *constant = llvm::cast<llvm::Constant>(val->codegen(context));
                bool success = context.setConst(name->name, constant);
                success &= context.setConstVal(name->name, constant);
                if (!success) throw CodegenException("Duplicate identifier in const section of main program: " + name->name);
                std::cout << "Added to symbol table" << std::endl;
                // auto *gv = new llvm::GlobalVariable(*context.getModule(), val->getLLVMType(context), true, llvm::GlobalVariable::ExternalLinkage, constant, name->name);
                // std::cout << "Created global variable" << std::endl;
                // return gv;
                return nullptr;
            } 
        }  
    }

    llvm::Value *TypeDeclNode::codegen(CodegenContext &context)
    {
        if (context.is_subroutine)
        {
            if (type->type == Type::Array)
            {
                bool success = context.setArrayAlias(context.getTrace() + "_" + name->name, cast_node<ArrayTypeNode>(type));
                if (!success) throw CodegenException("Duplicate type alias in function " + context.getTrace() + ": " + name->name);
                std::cout << "Array alias in function " << context.getTrace() << ": " << name->name << std::endl;
            }
            else if (type->type == Type::Record)
            {
                bool success = context.setAlias(context.getTrace() + "_" + name->name, type->getLLVMType(context));
                success &= context.setRecordAlias(context.getTrace() + "_" + name->name, cast_node<RecordTypeNode>(type));
                if (!success) throw CodegenException("Duplicate type alias in function " + context.getTrace() + ": " + name->name);
            }
            else
            {
                bool success = context.setAlias(context.getTrace() + "_" + name->name, type->getLLVMType(context));
                if (!success) throw CodegenException("Duplicate type alias in function " + context.getTrace() + ": " + name->name);
            }
        }
        else
        {
            if (type->type == Type::Array)
            {
                bool success = context.setArrayAlias(name->name, cast_node<ArrayTypeNode>(type));
                if (!success) throw CodegenException("Duplicate type alias in main program: " + name->name);
                std::cout << "Global array alias: " << name->name << std::endl;
            }
            else if (type->type == Type::Record)
            {
                bool success = context.setAlias(name->name, type->getLLVMType(context));
                success &= context.setRecordAlias(name->name, cast_node<RecordTypeNode>(type));
                if (!success) throw CodegenException("Duplicate type alias in main program: " + name->name);
            }
            else
            {
                bool success = context.setAlias(name->name, type->getLLVMType(context));
                if (!success) throw CodegenException("Duplicate type alias in main program: " + name->name);
            }        
        }
        return nullptr;
    }


} // namespace spc
