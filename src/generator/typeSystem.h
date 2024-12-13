// Copyright 2024 Lucas Norman

#pragma once

#include <cstdint>
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

class Type {
 public:
    [[nodiscard]] virtual inline std::string toString() const = 0;
    [[nodiscard]] virtual inline llvm::Type*
    toLLVMType(const std::unique_ptr<llvm::IRBuilder<>>& builder) const = 0;
    [[nodiscard]] virtual inline bool
    equals(const std::shared_ptr<Type>& otherType) const = 0;
    [[nodiscard]] virtual inline std::shared_ptr<Type> toSigned() const = 0;
    [[nodiscard]] virtual inline bool isSigned() const { return true; }
    [[nodiscard]] virtual inline bool isPointerType() const { return false; }
    [[nodiscard]] virtual inline bool isArrayType() const { return false; }
    [[nodiscard]] virtual inline bool isMutablePointerType() const {
        return false;
    }
    [[nodiscard]] virtual inline bool isBooleanType() const { return false; }
    [[nodiscard]] virtual inline bool isIntegerType() const { return false; }
    [[nodiscard]] virtual inline bool isFloatingPointType() const {
        return false;
    }
    [[nodiscard]] virtual inline bool isVoidType() const { return false; }
    [[nodiscard]] virtual inline std::shared_ptr<Type>
    getPointerElementType() const {
        return nullptr;
    }
    [[nodiscard]] virtual inline std::shared_ptr<Type>
    getArrayElementType() const {
        return nullptr;
    }
    [[nodiscard]] virtual inline std::uint64_t getArraySize() const {
        return 0;
    }
};

class IntegerType : public Type {
 public:
    std::uint32_t bitwidth;
    bool isSignedInteger;

    IntegerType(std::uint32_t _bitwidth, bool _isSigned)
        : bitwidth(_bitwidth), isSignedInteger(_isSigned) {}

    [[nodiscard]] inline std::string toString() const override {
        return (isSignedInteger ? "i" : "u") + std::to_string(bitwidth);
    };

    [[nodiscard]] inline llvm::Type* toLLVMType(
        const std::unique_ptr<llvm::IRBuilder<>>& builder) const override {
        return builder->getIntNTy(bitwidth);
    };

    [[nodiscard]] inline bool
    equals(const std::shared_ptr<Type>& otherType) const override {
        auto* castedType = dynamic_cast<IntegerType*>(otherType.get());
        if (castedType == nullptr)
            return false;
        return bitwidth == castedType->bitwidth &&
               isSignedInteger == castedType->isSignedInteger;
    }

    [[nodiscard]] inline std::shared_ptr<Type> toSigned() const override {
        return std::make_shared<IntegerType>(bitwidth, true);
    }

    [[nodiscard]] inline bool isSigned() const override {
        return isSignedInteger;
    }

    [[nodiscard]] inline bool isIntegerType() const override { return true; }
};

class CharacterType : public IntegerType {
 public:
    // 32 is the bitwidth of the character type
    CharacterType() : IntegerType(32, true) {}

    [[nodiscard]] inline std::string toString() const override {
        return "char";
    };

    [[nodiscard]] inline bool
    equals(const std::shared_ptr<Type>& otherType) const override {
        return dynamic_cast<CharacterType*>(otherType.get()) != nullptr;
    }

    [[nodiscard]] inline std::shared_ptr<Type> toSigned() const override {
        return std::make_shared<CharacterType>();
    }
};

class BooleanType : public Type {
 public:
    [[nodiscard]] inline std::string toString() const override {
        return "bool";
    };

    [[nodiscard]] inline llvm::Type* toLLVMType(
        const std::unique_ptr<llvm::IRBuilder<>>& builder) const override {
        return builder->getInt1Ty();
    };

    [[nodiscard]] inline bool
    equals(const std::shared_ptr<Type>& otherType) const override {
        return dynamic_cast<BooleanType*>(otherType.get()) != nullptr;
    }

    [[nodiscard]] inline std::shared_ptr<Type> toSigned() const override {
        return std::make_shared<BooleanType>();
    }

    [[nodiscard]] inline bool isBooleanType() const override { return true; }
};

class FloatingPointType : public Type {
 public:
    std::uint32_t bitwidth;

    explicit FloatingPointType(std::uint32_t _bitwidth) : bitwidth(_bitwidth) {}

    [[nodiscard]] inline std::string toString() const override {
        return "f" + std::to_string(bitwidth);
    };

    [[nodiscard]] inline llvm::Type* toLLVMType(
        const std::unique_ptr<llvm::IRBuilder<>>& builder) const override {
        switch (bitwidth) {
        case 32:
            return builder->getFloatTy();
        case 64:
            return builder->getDoubleTy();
        default:
            // this code should not be reachable, although throw an error just
            // in case
            generator::fatal_error(
                std::chrono::high_resolution_clock::now(),
                "Invalid floating point type",
                "Floating point types are not supported for the bitwidth '" +
                    std::to_string(bitwidth) + "'");
            return nullptr;
        }
    };

    [[nodiscard]] inline bool
    equals(const std::shared_ptr<Type>& otherType) const override {
        auto* castedType = dynamic_cast<FloatingPointType*>(otherType.get());
        if (castedType == nullptr)
            return false;
        return bitwidth == castedType->bitwidth;
    }

    [[nodiscard]] inline std::shared_ptr<Type> toSigned() const override {
        return std::make_shared<FloatingPointType>(bitwidth);
    }

    [[nodiscard]] inline bool isFloatingPointType() const override {
        return true;
    }
};

class PointerType : public Type {
 public:
    std::shared_ptr<Type> elementType;
    bool isMutable;

    explicit PointerType(std::shared_ptr<Type> _elementType, bool _isMutable)
        : elementType(std::move(_elementType)), isMutable(_isMutable) {}

    [[nodiscard]] inline std::string toString() const override {
        if (isMutable)
            return "*mut " + elementType->toString();
        else
            return "*" + elementType->toString();
    };

    [[nodiscard]] inline llvm::Type* toLLVMType(
        const std::unique_ptr<llvm::IRBuilder<>>& builder) const override {
        return llvm::PointerType::get(elementType->toLLVMType(builder), 0);
    };

    [[nodiscard]] inline bool
    equals(const std::shared_ptr<Type>& otherType) const override {
        auto* castedType = dynamic_cast<PointerType*>(otherType.get());
        if (castedType == nullptr)
            return false;
        return elementType->equals(castedType->elementType) &&
               isMutable == castedType->isMutable;
    }

    [[nodiscard]] inline std::shared_ptr<Type> toSigned() const override {
        return std::make_shared<PointerType>(elementType, isMutable);
    }

    [[nodiscard]] inline bool isPointerType() const override { return true; }

    [[nodiscard]] inline bool isMutablePointerType() const override {
        return isMutable;
    }

    [[nodiscard]] inline std::shared_ptr<Type>
    getPointerElementType() const override {
        return elementType;
    }
};

class ArrayType : public Type {
 public:
    std::shared_ptr<Type> elementType;
    std::uint64_t size;

    ArrayType(std::shared_ptr<Type> _elementType, std::uint64_t _size)
        : elementType(std::move(_elementType)), size(_size) {}

    [[nodiscard]] inline std::string toString() const override {
        return "[" + elementType->toString() + "|" + std::to_string(size) + "]";
    };

    [[nodiscard]] inline llvm::Type* toLLVMType(
        const std::unique_ptr<llvm::IRBuilder<>>& builder) const override {
        return llvm::ArrayType::get(elementType->toLLVMType(builder), size);
    };

    [[nodiscard]] inline bool
    equals(const std::shared_ptr<Type>& otherType) const override {
        auto* castedType = dynamic_cast<ArrayType*>(otherType.get());
        if (castedType == nullptr)
            return false;
        return elementType->equals(castedType->elementType) &&
               size == castedType->size;
    }

    [[nodiscard]] inline std::shared_ptr<Type> toSigned() const override {
        return std::make_shared<ArrayType>(elementType, size);
    }

    [[nodiscard]] inline bool isArrayType() const override { return true; }

    [[nodiscard]] inline std::uint64_t getArraySize() const override {
        return size;
    }

    [[nodiscard]] inline std::shared_ptr<Type>
    getArrayElementType() const override {
        return elementType;
    }
};

class VoidType : public Type {
 public:
    [[nodiscard]] inline std::string toString() const override {
        return "void";
    };

    [[nodiscard]] inline llvm::Type* toLLVMType(
        const std::unique_ptr<llvm::IRBuilder<>>& builder) const override {
        return builder->getVoidTy();
    };

    [[nodiscard]] inline bool
    equals(const std::shared_ptr<Type>& otherType) const override {
        return dynamic_cast<VoidType*>(otherType.get()) != nullptr;
    }

    [[nodiscard]] inline std::shared_ptr<Type> toSigned() const override {
        return std::make_shared<VoidType>();
    }

    [[nodiscard]] inline bool isVoidType() const override { return true; }
};

namespace strings {

inline bool isPointerType(const std::string& type) {
    return type.starts_with('*') || type.starts_with("*mut ");
}
inline bool isArrayType(const std::string& type) {
    // if it is surrounded with brackets "[ ]"
    return type[0] == '[' && type[type.length() - 1] == ']';
}
inline bool isMutablePointerType(const std::string& type) {
    return type.starts_with("*mut ");
}
inline std::string getPointerElementType(const std::string& type) {
    if (isMutablePointerType(type))
        // remove the "*mut "
        return type.substr(5);
    else
        // remove the "*"
        return type.substr(1);
}
inline std::string getArrayElementType(const std::string& type) {
    // remove the first "[" and the "|integer]" sequence at the end
    return std::regex_replace(type.substr(1), std::regex("\\|[0-9]+\\]$"), "");
}
inline std::uint64_t getArraySize(const std::string& type) {
    // replace everything up to the last number and then remove the last "]"
    return std::stoull(
        std::regex_replace(type, std::regex("\\[.+\\|([0-9]+)\\]$"), "$1"));
}
}  // namespace strings

static std::shared_ptr<Type> createTypeFromString(const std::string& type) {
    // if it's an array type, then create an array type by recursively getting
    // the element type
    if (strings::isArrayType(type)) {
        // check that the size is not 0
        if (strings::getArraySize(type) <= 0) {
            generator::fatal_error(
                std::chrono::high_resolution_clock::now(), "Invalid array type",
                "Array types must have a length greater than zero");
            return nullptr;
        }
        return std::make_shared<ArrayType>(
            createTypeFromString(strings::getArrayElementType(type)),
            strings::getArraySize(type));
    }

    // if it's a pointer type, then create a pointer type by recursively getting
    // the element type
    if (strings::isPointerType(type)) {
        return std::make_shared<PointerType>(
            createTypeFromString(strings::getPointerElementType(type)),
            strings::isMutablePointerType(type));
    }

    // checks for all primitive types
    if (type == "bool")
        return std::make_shared<BooleanType>();
    if (type == "char")
        return std::make_shared<CharacterType>();
    if (type == "i8" || type == "u8")
        return std::make_shared<IntegerType>(8, type.starts_with("i"));
    if (type == "i16" || type == "u16")
        return std::make_shared<IntegerType>(16, type.starts_with("i"));
    if (type == "i32" || type == "u32")
        return std::make_shared<IntegerType>(32, type.starts_with("i"));
    if (type == "i64" || type == "u64")
        return std::make_shared<IntegerType>(64, type.starts_with("i"));
    if (type == "f32")
        return std::make_shared<FloatingPointType>(32);
    if (type == "f64")
        return std::make_shared<FloatingPointType>(64);
    if (type == "str")
        return std::make_shared<PointerType>(std::make_shared<CharacterType>(),
                                             false);
    if (type.empty())
        return std::make_shared<VoidType>();

    // invalid type, throw error
    generator::fatal_error(std::chrono::high_resolution_clock::now(),
                           "Invalid type",
                           "'" + type + "' is not a valid type");
    return nullptr;
}

// helper function to get the type from an integer (since it can vary
// in bit-width)
static std::shared_ptr<Type> getIntegerType(std::uint64_t number) {
    if (number <= 2147483647) {
        // limit for signed i32
        return std::make_shared<IntegerType>(32, true);
    } else if (number <= 9223372036854775807) {
        // limit for signed i64
        return std::make_shared<IntegerType>(64, true);
    } else {
        generator::fatal_error(
            std::chrono::high_resolution_clock::now(),
            "Invalid integer literal",
            "The integer is to large to be represented as an integer");
        return nullptr;
    }
}

// helper function to get the llvm::Value* from an integer (since it can vary
// in bit-width)
static llvm::Value*
getIntegerValue(std::uint64_t number,
const std::unique_ptr<llvm::IRBuilder<>>& builder) {
    if (number <= 2147483647) {
        // limit for i32
        return builder->getInt32(number);
    } else if (number <= 9223372036854775807) {
        // limit for i64
        return builder->getInt64(number);
    } else {
        generator::fatal_error(
            std::chrono::high_resolution_clock::now(),
            "Invalid integer literal",
            "The integer value is to large to be represented as an integer");
        return nullptr;
    }
}
}  // namespace typeSystem
