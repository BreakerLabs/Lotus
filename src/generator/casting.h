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

#include "scopeStack.h"
#include "typeSystem.h"

#include "../diagnostics/generator.h"

namespace casting {

// helper function to create a llvm cast instruction
static llvm::Value*
createCast(llvm::Value* value,
const std::shared_ptr<typeSystem::Type>& sourceType,
const std::shared_ptr<typeSystem::Type>& destinationType,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    if (sourceType->equals(destinationType)) {
        // no cast needed
        return value;
    }

    llvm::Type* llvmDestinationType = destinationType->toLLVMType(builder);

    // if the source type is bool
    if (sourceType->isBooleanType()) {
        if (destinationType->isIntegerType()) {
            return builder->CreateCast(llvm::Instruction::ZExt, value,
                                       llvmDestinationType, "tmpcast");
        } else if (destinationType->isFloatingPointType()) {
            return builder->CreateCast(llvm::Instruction::UIToFP, value,
                                       llvmDestinationType, "tmpcast");
        }
    }

    // if the destination type is bool, then check if the value does not equal 0
    if (destinationType->isBooleanType()) {
        if (sourceType->isIntegerType()) {
            return builder->CreateICmpNE(
                value, llvm::ConstantInt::get(value->getType(), 0),
                "cmptozero");
        } else if (sourceType->isFloatingPointType()) {
            return builder->CreateFCmpONE(
                value, llvm::ConstantFP::get(value->getType(), 0), "cmptozero");
        }
    }

    // if the source type and destination type is an integer or floating point
    if ((sourceType->isIntegerType() || sourceType->isFloatingPointType()) &&
        (destinationType->isIntegerType() ||
         destinationType->isFloatingPointType())) {
        llvm::CastInst::CastOps castOperation = llvm::CastInst::getCastOpcode(
            value, sourceType->isSigned(), llvmDestinationType,
            destinationType->isSigned());
        return builder->CreateCast(castOperation, value, llvmDestinationType,
                                   "tmpcast");
    }

    // throw error, cast is not supported
    generator::fatal_error(std::chrono::high_resolution_clock::now(),
                           "Invalid cast",
                           "Cannot cast from '" + sourceType->toString() +
                               "' to '" + destinationType->toString() + "'");
    return nullptr;
}

// helper function for getting the boolean representation of a llvm::Value*
static llvm::Value*
toBoolean(llvm::Value* value,
const std::shared_ptr<typeSystem::Type>& sourceType,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    return createCast(value, sourceType,
                      std::make_shared<typeSystem::BooleanType>(), builder);
}
}  // namespace casting
