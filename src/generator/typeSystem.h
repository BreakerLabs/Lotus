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

#include "../diagnostics/generator.h"

namespace typeSystem {

struct Type {
 private:
    std::string type;

 public:
    Type() = default;
    explicit Type(std::string type) : type(std::move(type)) {}
    Type(const Type& other) = default;

    bool operator!=(const Type& otherType) const {
        return type != otherType.type;
    }

    bool operator==(const Type& otherType) const {
        return type == otherType.type;
    }

    [[nodiscard]] std::string toString() const { return type; }
    [[nodiscard]] llvm::Type*
    toLLVMType(const std::unique_ptr<llvm::IRBuilder<>>& builder) const {
        // if the type is an array, then recursively get the array type until
        // it is not an array anymore
        if (isArrayType()) {
            // get the type within the array (element type)
            return llvm::ArrayType::get(
                getArrayElementType().toLLVMType(builder), getArraySize());
        }

        // if the type is a pointer, then recursively get the pointer type until
        // it is not a pointer anymore
        if (isPointerType()) {
            // get the pointer type of the type when removing the asterisk (the
            // element type)
            return llvm::PointerType::get(
                getPointerElementType().toLLVMType(builder), 0);
        }

        if (type == "bool")
            return builder->getInt1Ty();
        if (type == "u8" || type == "i8" || type == "char")
            return builder->getInt8Ty();
        if (type == "u16" || type == "i16")
            return builder->getInt16Ty();
        if (type == "u32" || type == "i32")
            return builder->getInt32Ty();
        if (type == "u64" || type == "i64")
            return builder->getInt64Ty();
        if (type == "f32")
            return builder->getFloatTy();
        if (type == "f64")
            return builder->getDoubleTy();
        if (type == "str")
            return llvm::PointerType::get(builder->getInt8Ty(), 0);
        // return a void type
        return builder->getVoidTy();
    }
    [[nodiscard]] bool isSigned() const {
        return type != "u8" && type != "u16" && type != "u32" && type != "u64";
    }
    [[nodiscard]] Type toSigned() const {
        if (type == "u8") return Type{"i8"};
        if (type == "u16") return Type{"i16"};
        if (type == "u32") return Type{"i32"};
        if (type == "u64") return Type{"i64"};
        return Type{type};
    }
    [[nodiscard]] bool isPointerType() const {
        return type.starts_with('*') || type.starts_with("*mut ");
    }
    [[nodiscard]] bool isArrayType() const {
        // if it is surrounded with brackets "[ ]"
        return type[0] == '[' && type[type.length() - 1] == ']';
    }
    [[nodiscard]] bool isMutablePointerType() const {
        return type.starts_with("*mut ");
    }
    [[nodiscard]] bool isVoidType() const { return type.empty(); }
    [[nodiscard]] bool isBooleanType() const { return type == "bool"; }
    // i8 to i64 and u8 to u64, also including char
    [[nodiscard]] bool isIntegerType() const {
        return type == "i8" || type == "i16" || type == "i32" ||
               type == "i64" || type == "u8" || type == "u16" ||
               type == "u32" || type == "u64" || type == "char";
    }
    [[nodiscard]] bool isFloatingPointType() const {
        return type == "f32" || type == "f64";
    }
    [[nodiscard]] Type createPointerType(bool isMutable) const {
        if (isMutable)
            return Type{"*mut " + type};
        else
            return Type{"*" + type};
    }
    [[nodiscard]] Type createArrayType(uint64_t arraySize) const {
        return Type{"[" + type + "|" + std::to_string(arraySize) + "]"};
    }
    [[nodiscard]] Type getPointerElementType() const {
        if (isMutablePointerType())
            // replace the "*mut "
            return Type{type.substr(5)};
        else
            // replace the "*"
            return Type{type.substr(1)};
    }
    [[nodiscard]] Type getArrayElementType() const {
        // replace the first "[" and the "|integer]" sequence at the end
        return Type{std::regex_replace(type.substr(1),
                                       std::regex("\\|[0-9]+\\]$"), "")};
    }
    [[nodiscard]] uint64_t getArraySize() const {
        // replace everything up to the last number and then remove the last "]"
        return std::stoull(
            std::regex_replace(type, std::regex("\\[.+\\|([0-9]+)\\]$"), "$1"));
    }
};

// helper function to get the llvm::Value* from an integer (since it can vary
// in bit-width). Only used with positive numbers
static Type getIntegerType(uint64_t number) {
    if (number <= 2147483647) {
        // limit for signed i32
        return Type{"i32"};
    } else if (number <= 9223372036854775807) {
        // limit for signed i64
        return Type{"i64"};
    } else {
        generator::fatal_error(
            std::chrono::high_resolution_clock::now(),
            "Invalid integer literal",
            "The integer is to large to be represented as an integer");
        return Type{""};
    }
}

// helper function to get the llvm::Value* from an integer (since it can vary
// in bit-width). Only used with positive numbers
static llvm::Value*
getIntegerValue(uint64_t number,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    if (number <= 2147483647) {
        // limit for signed i32
        return builder->getInt32(number);
    } else if (number <= 9223372036854775807) {
        // limit for signed i64
        return builder->getInt64(number);
    } else {
        generator::fatal_error(
            std::chrono::high_resolution_clock::now(),
            "Invalid integer literal",
            "The integer is to large to be represented as an integer");
        return nullptr;
    }
}
}  // namespace typeSystem
