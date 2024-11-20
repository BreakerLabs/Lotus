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
createPointerTypeOperation(llvm::Value* leftValue, llvm::Value* rightValue,
const std::string& operation,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    if (operation == "==") {
        return builder->CreateICmpEQ(leftValue, rightValue, "cmptmpequals");
    } else if (operation == "!=") {
        return builder->CreateICmpNE(leftValue, rightValue, "cmptmpnotequals");
    }
    // if there is no operation that matches
    return nullptr;
}

// helper function for the createBinaryOperation function
static llvm::Value*
createBooleanTypeOperation(llvm::Value* leftValue, llvm::Value* rightValue,
const std::string& operation,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    if (operation == "==") {
        return builder->CreateICmpEQ(leftValue, rightValue, "cmptmpequals");
    } else if (operation == "!=") {
        return builder->CreateICmpNE(leftValue, rightValue, "cmptmpnotequals");
    }
    // if there is no operation that matches
    return nullptr;
}

// helper function for the createBinaryOperation function
static llvm::Value*
createIntegerTypeOperation(llvm::Value* leftValue, llvm::Value* rightValue,
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
static llvm::Value* createFloatingPointTypeOperation(
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
static std::tuple<llvm::Value*, std::shared_ptr<typeSystem::Type>>
createBinaryOperation(llvm::Value* leftValue, llvm::Value* rightValue,
const std::shared_ptr<typeSystem::Type>& leftType,
const std::shared_ptr<typeSystem::Type>& rightType,
const std::string& operation,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    // these can be performed with different types because both sides are cast
    // to booleans
    if (operation == "&&") {
        return {builder->CreateAnd(
                    casting::toBoolean(leftValue, leftType, builder),
                    casting::toBoolean(rightValue, rightType, builder),
                    "andtmp"),
                std::make_shared<typeSystem::BooleanType>()};
    } else if (operation == "||") {
        return {builder->CreateOr(
                    casting::toBoolean(leftValue, leftType, builder),
                    casting::toBoolean(rightValue, rightType, builder),
                    "ortmp"),
                std::make_shared<typeSystem::BooleanType>()};
    }

    // check if the left and right expression have the same type
    if (!leftType->equals(rightType)) {
        generator::fatal_error(std::chrono::high_resolution_clock::now(),
                               "Type mismatch in binary operation",
                               "Cannot perform the binary operation '" +
                                   operation + "' with the types '" +
                                   leftType->toString() + "' and '" +
                                   rightType->toString() + "'");
        return {};
    }

    // if it is an integer that is not an i1 (boolean)
    llvm::Value* resultValue;
    if (leftType->isIntegerType()) {
        resultValue = createIntegerTypeOperation(
            leftValue, rightValue, operation, leftType->isSigned(), builder);
    } else if (leftType->isFloatingPointType()) {
        resultValue = createFloatingPointTypeOperation(leftValue, rightValue,
                                                       operation, builder);
    } else if (leftType->isPointerType()) {
        resultValue = createPointerTypeOperation(leftValue, rightValue,
                                                 operation, builder);
    } else if (leftType->isBooleanType()) {
        resultValue = createBooleanTypeOperation(leftValue, rightValue,
                                                 operation, builder);
    } else {
        generator::fatal_error(std::chrono::high_resolution_clock::now(),
                               "Invalid binary operation",
                               "Cannot perform the binary operation '" +
                                   operation + "' with the types '" +
                                   leftType->toString() + "'");
        return {};
    }

    // if no operators matched, then throw an error
    if (resultValue == nullptr) {
        generator::fatal_error(std::chrono::high_resolution_clock::now(),
                               "Invalid binary operator",
                               "The binary operator '" + operation +
                                   "' is not supported with the type '" +
                                   leftType->toString() + "'");
        return {};
    }

    // some operations always result in a boolean type and others don't change
    // the type
    std::shared_ptr<typeSystem::Type> resultType =
        (operation == "==" || operation == "!=" || operation == "<" ||
         operation == ">" || operation == "<=" || operation == ">=")
            ? std::make_shared<typeSystem::BooleanType>()
            : leftType;

    // return the result value together with the resulting type (the leftType
    // and rightType are the same here)
    return {resultValue, resultType};
}

// helper function to create unary operations
static std::tuple<llvm::Value*, std::shared_ptr<typeSystem::Type>>
createUnaryOperation(llvm::Value* value,
const std::shared_ptr<typeSystem::Type>& type,
const std::string& operation,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    bool isFloatingPointOperation = type->isFloatingPointType();
    bool isIntegerOperation = type->isIntegerType();

    if (operation == "!") {
        return {
            builder->CreateNot(casting::toBoolean(value, type, builder),
                               "nottmp"),
            // the "!" operator casts the type to bool, so it's always a bool
            std::make_shared<typeSystem::BooleanType>()};
    } else if (operation == "-") {
        if (isFloatingPointOperation) {
            return {builder->CreateFNeg(value, "negtmp"), type};
        } else if (isIntegerOperation) {
            return {builder->CreateNeg(value, "negtmp"),
                    // if the "-" operation is used on an integer, the result
                    // should always be signed
                    type->toSigned()};
        }
    } else if (operation == "+") {
        // does not change the value (but check if it is used with valid types
        // anyway)
        if (isFloatingPointOperation || isIntegerOperation)
            return {value, type};
    }

    // if no operators matched, then throw an error
    generator::fatal_error(
        std::chrono::high_resolution_clock::now(), "Invalid unary operator",
        "The unary operator '" + operation +
            "' is not supported with the type '" + type->toString() + "'");
    return {};
}
}  // namespace operators
