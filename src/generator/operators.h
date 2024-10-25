// Copyright 2024 Lucas Norman

#pragma once

#include <map>
#include <ranges>
#include <regex>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "casting.h"
#include "typeSystem.h"

#include "../diagnostics/generator.h"

namespace operators {

// helper function for the createBinaryOperation function
static llvm::Value*
createIntegerOperation(llvm::Value* leftValue, llvm::Value* rightValue,
const std::string& operation, bool isSignedOperation,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    if (operation == "+") {
        return builder->CreateAdd(leftValue, rightValue, "addtmp");
    } else if (operation == "-") {
        return builder->CreateSub(leftValue, rightValue, "subtmp");
    } else if (operation == "*") {
        return builder->CreateMul(leftValue, rightValue, "multmp");
    } else if (operation == "/") {
        if (isSignedOperation) {
            return builder->CreateSDiv(leftValue, rightValue, "divtmp");
        } else {
            return builder->CreateUDiv(leftValue, rightValue, "divtmp");
        }
    } else if (operation == "%") {
        if (isSignedOperation) {
            return builder->CreateSRem(leftValue, rightValue, "remtmp");
        } else {
            return builder->CreateURem(leftValue, rightValue, "remtmp");
        }
    } else if (operation == "==") {
        return builder->CreateICmpEQ(leftValue, rightValue, "cmptmpequals");
    } else if (operation == "!=") {
        return builder->CreateICmpNE(leftValue, rightValue, "cmptmpnotequals");
    } else if (operation == "<") {
        if (isSignedOperation) {
            return builder->CreateICmpSLT(leftValue, rightValue, "cmptmpless");
        } else {
            return builder->CreateICmpULT(leftValue, rightValue, "cmptmpless");
        }
    } else if (operation == ">") {
        if (isSignedOperation) {
            return builder->CreateICmpSGT(leftValue, rightValue,
                                          "cmptmpgreater");
        } else {
            return builder->CreateICmpUGT(leftValue, rightValue,
                                          "cmptmpgreater");
        }
    } else if (operation == "<=") {
        if (isSignedOperation) {
            return builder->CreateICmpSLE(leftValue, rightValue,
                                          "cmptmplessequals");
        } else {
            return builder->CreateICmpULE(leftValue, rightValue,
                                          "cmptmplessequals");
        }
    } else if (operation == ">=") {
        if (isSignedOperation) {
            return builder->CreateICmpSGE(leftValue, rightValue,
                                          "cmptmpgreaterequals");
        } else {
            return builder->CreateICmpUGE(leftValue, rightValue,
                                          "cmptmpgreaterequals");
        }
    }
    // if there is no operation that matches
    return nullptr;
}

// helper function for the createBinaryOperation function
static llvm::Value* createFloatingPointOperation(
llvm::Value* leftValue, llvm::Value* rightValue,
const std::string& operation,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    if (operation == "+") {
        return builder->CreateFAdd(leftValue, rightValue, "addfloattmp");
    } else if (operation == "-") {
        return builder->CreateFSub(leftValue, rightValue, "subfloattmp");
    } else if (operation == "*") {
        return builder->CreateFMul(leftValue, rightValue, "mulfloattmp");
    } else if (operation == "/") {
        return builder->CreateFDiv(leftValue, rightValue, "divfloattmp");
    } else if (operation == "%") {
        return builder->CreateFRem(leftValue, rightValue, "remfloattmp");
    } else if (operation == "==") {
        return builder->CreateFCmpOEQ(leftValue, rightValue,
                                      "cmpfloattmpequals");
    } else if (operation == "!=") {
        return builder->CreateFCmpONE(leftValue, rightValue,
                                      "cmpfloattmpnotequals");
    } else if (operation == "<") {
        return builder->CreateFCmpOLT(leftValue, rightValue, "cmpfloattmpless");
    } else if (operation == ">") {
        return builder->CreateFCmpOGT(leftValue, rightValue,
                                      "cmpfloattmpgreater");
    } else if (operation == "<=") {
        return builder->CreateFCmpOLE(leftValue, rightValue,
                                      "cmpfloattmplessequals");
    } else if (operation == ">=") {
        return builder->CreateFCmpOGE(leftValue, rightValue,
                                      "cmpfloattmpgreaterequals");
    }
    // if there is no operation that matches
    return nullptr;
}

// helper function to create binary operations
static std::tuple<llvm::Value*, typeSystem::Type>
createBinaryOperation(llvm::Value* leftValue, llvm::Value* rightValue,
const typeSystem::Type& leftType,
const typeSystem::Type& rightType,
const std::string& operation,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    // these can be performed with different types because both sides are cast
    // to booleans
    if (operation == "&&") {
        return {builder->CreateAnd(
                    casting::toBoolean(leftValue, leftType, builder),
                    casting::toBoolean(rightValue, rightType, builder),
                    "andtmp"),
                typeSystem::Type{"bool"}};
    } else if (operation == "||") {
        return {builder->CreateOr(
                    casting::toBoolean(leftValue, leftType, builder),
                    casting::toBoolean(rightValue, rightType, builder),
                    "ortmp"),
                typeSystem::Type{"bool"}};
    }

    // check if the left and right expression have the same type
    if (leftType != rightType) {
        generator::fatal_error(std::chrono::high_resolution_clock::now(),
                               "Type mismatch in binary operation",
                               "Cannot perform the binary operation '" +
                                   operation + "' with the types '" +
                                   leftType.toString() + "' and '" +
                                   rightType.toString() + "'");
        return {};
    }

    // if it is an integer that is not an i1 (boolean)
    llvm::Value* resultValue =
        leftType.isIntegerType()
            ? createIntegerOperation(leftValue, rightValue, operation,
                                     leftType.isSigned(), builder)
            : createFloatingPointOperation(leftValue, rightValue, operation,
                                           builder);

    // some operations always result in a boolean type and others don't change
    // the type
    typeSystem::Type resultType =
        (operation == "==" || operation == "!=" || operation == "<" ||
         operation == ">" || operation == "<=" || operation == ">=")
            ? resultType
            : leftType;

    // if no operators matched, then throw an error
    if (resultValue == nullptr) {
        generator::fatal_error(std::chrono::high_resolution_clock::now(),
                               "Invalid binary operator",
                               "The binary operator '" + operation +
                                   "' is not supported with the type '" +
                                   leftType.toString() + "'");
        return {};
    }
    // return the result value together with the resulting type (the leftType
    // and rightType are the same here)
    return {resultValue, resultType};
}
}  // namespace operators
