// Copyright 2024 Lucas Norman

#pragma once

#include <map>
#include <ranges>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "typeSystem.h"

#include "../diagnostics/generator.h"

namespace scopes {

// helper function to create an alloca instruction in the entry block of a
// function. Used with mutable variables
static llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn,
const std::string& variableName,
llvm::Type* type) {
    llvm::IRBuilder<> temporaryBuilder(&fn->getEntryBlock(),
                                       fn->getEntryBlock().begin());
    return temporaryBuilder.CreateAlloca(type, nullptr, variableName);
}

struct FunctionData {
    std::shared_ptr<typeSystem::Type> returnType;
    std::vector<std::shared_ptr<typeSystem::Type>> parameterTypes;
    bool isVarArg{};
};

// to store function types to use in for example function calls
static std::map<std::string, FunctionData> functionTypes;

// helper function to get the type from a function
static FunctionData* getFunctionData(const std::string& identifier) {
    if (functionTypes.contains(identifier))
        return &functionTypes[identifier];
    return nullptr;
}

// helper function to store the type for a function
static void setFunctionData(
const std::string& identifier,
const std::shared_ptr<typeSystem::Type>& returnType,
const std::vector<std::shared_ptr<typeSystem::Type>>& parameterTypes,
bool isVarArg = false) {
    functionTypes[identifier] = {returnType, parameterTypes, isVarArg};
}

// helper class to get and set the current function (as a string)
class CurrentFunction {
 private:
    std::string currentFunction;
    static CurrentFunction& instance() {
        static CurrentFunction instance;
        return instance;
    }
    CurrentFunction() = default;

 public:
    static const std::string& get() { return instance().currentFunction; }
    static void set(const std::string& function) {
        instance().currentFunction = function;
    }
};

struct AllocationData {
    llvm::AllocaInst* allocaInst{};
    std::shared_ptr<typeSystem::Type> type;
    bool isMutable{};
};

// to keep track of the variables that are available in the current
// scope/function when generating LLVM IR. Each map in the vector is a scope
static std::vector<std::map<std::string, AllocationData>> scopeStack;

// helper function to find a variable in the scopeStack
static AllocationData* getAllocationData(const std::string& identifier) {
    // go over each scope and check if the variable is defined (reverse the
    // scopeStack to check the most nested scopes first)
    for (auto& scope : std::ranges::reverse_view(scopeStack)) {
        auto it = scope.find(identifier);
        if (it != scope.end())
            return &it->second;
    }
    return nullptr;
}

// helper function to set a variable in the scopeStack
static void setAllocationData(const std::string& identifier,
llvm::AllocaInst* allocaInst,
const std::shared_ptr<typeSystem::Type>& type,
bool isMutable) {
    if (scopeStack.empty())
        scopeStack.emplace_back();
    scopeStack.back()[identifier] = {allocaInst, type, isMutable};
}

static void clearScopes() { scopeStack.clear(); }

static void startScope() { scopeStack.emplace_back(); }

static void endScope() {
    if (!scopeStack.empty()) {
        scopeStack.pop_back();
    }
}

static bool existsInCurrentScope(const std::string& identifier) {
    if (scopeStack.empty())
        return false;
    return scopeStack.back().find(identifier) != scopeStack.back().end();
}
}  // namespace scopes
