// Copyright 2024 Pontus Henriksson, Neo Mannskär & Lucas Norman

#pragma once

#include <iostream>
#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include "../parser/parse.h"
#include "codegen.h"
#include "generator.h"

class Generator {
 private:
    std::unique_ptr<llvm::IRBuilder<>> builder;
    std::unique_ptr<llvm::LLVMContext> ctx;
    std::unique_ptr<llvm::Module> moduleLLVM;

 public:
    explicit Generator(const std::string& inputFilename,
                       const std::string& outputFilename) {
        ctx = std::make_unique<llvm::LLVMContext>();
        moduleLLVM = std::make_unique<llvm::Module>("LotusLLVM", *ctx);
        builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
        setupExternFunctions();
        generate(inputFilename, outputFilename);
    }

 private:
    void generate(const std::string& inputFilename,
                  const std::string& outputFilename) {
        // create the ast
        std::unique_ptr<AST>& ast = parse(inputFilename);
        std::cout << "[Parsed AST]\n";
        ast->print();

        // generates the LLVM IR for the ast
        ast->codegen(ctx, builder, moduleLLVM);
        std::cout << "\n[Generated IR]\n";
        moduleLLVM->print(llvm::outs(), nullptr);

        // save the module IR to a file
        if (!outputFilename.empty())
            saveModuleToFile(outputFilename);
    }

    void setupExternFunctions() {
        moduleLLVM->getOrInsertFunction(
            "printf", llvm::FunctionType::get(
                          builder->getInt32Ty(),
                          builder->getInt8Ty()->getPointerTo(), true));
        moduleLLVM->getOrInsertFunction(
            "scanf", llvm::FunctionType::get(
                          builder->getInt32Ty(),
                          builder->getInt8Ty()->getPointerTo(), true));
        moduleLLVM->getOrInsertFunction(
            "rand", llvm::FunctionType::get(
                        builder->getInt32Ty(), false));
        moduleLLVM->getOrInsertFunction(
            "srand", llvm::FunctionType::get(
                         builder->getVoidTy(),
                         builder->getInt32Ty(), false));
        moduleLLVM->getOrInsertFunction(
            "time", llvm::FunctionType::get(
                        builder->getInt64Ty(),
                        builder->getInt64Ty()->getPointerTo(), false));
    }

    void saveModuleToFile(const std::string& filename) {
        std::error_code errorCode;
        llvm::raw_fd_ostream stream(filename, errorCode);
        moduleLLVM->print(stream, nullptr);
    }
};
