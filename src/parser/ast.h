// Copyright 2024 Lucas Norman

#pragma once

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "../generator/scopeStack.h"
#include "../generator/typeSystem.h"

#include "../diagnostics/generator.h"

enum CodegenResultType {
    // values that store a type, value and pointer (usually allocaInst)
    L_VALUE_CODEGEN_RESULT,
    // values that just store a type and value
    R_VALUE_CODEGEN_RESULT,
    PARAM_CODEGEN_RESULT,
    FUNCTION_CODEGEN_RESULT
};

// type to store parameter result in CodegenResult, since it has two fields
struct ParamCodegenResult {
    std::string identifier;
    std::shared_ptr<typeSystem::Type> type;
    bool isMutable;

    ParamCodegenResult(std::string identifier,
                       const std::shared_ptr<typeSystem::Type>& type,
                       bool isMutable)
        : identifier(std::move(identifier)), type(type), isMutable(isMutable) {}
    ~ParamCodegenResult() = default;
};

// type to return from codegen methods,
// to handle multiple return types like llvm::Value* and llvm::Function
struct CodegenResult {
    union {
        struct {
            llvm::Value* value;
            std::shared_ptr<typeSystem::Type> type;
        } rValue;
        struct {
            llvm::Value* value;
            std::shared_ptr<typeSystem::Type> type;
            llvm::Value* pointer;
            bool isMutable;
        } lValue;
        ParamCodegenResult param;
        llvm::Function* fn;
    };
    CodegenResultType resultType;

    // create an rValue with a value and type only
    CodegenResult(llvm::Value* value,
                  const std::shared_ptr<typeSystem::Type>& type)
        : rValue(value, type), resultType(R_VALUE_CODEGEN_RESULT) {}
    // create an lValue with a value, type, pointer and isMutable
    CodegenResult(llvm::Value* value,
                  const std::shared_ptr<typeSystem::Type>& type,
                  llvm::Value* pointer, bool isMutable)
        : lValue(value, type, pointer, isMutable),
          resultType(L_VALUE_CODEGEN_RESULT) {}
    explicit CodegenResult(const ParamCodegenResult& param)
        : param(param), resultType(PARAM_CODEGEN_RESULT) {}
    explicit CodegenResult(llvm::Function* fn)
        : fn(fn), resultType(FUNCTION_CODEGEN_RESULT) {}

    ~CodegenResult() {}

    // is used to check if the codegen result contains any type of value
    [[nodiscard]] bool isValueCodegenResultType() const {
        return resultType == L_VALUE_CODEGEN_RESULT ||
               resultType == R_VALUE_CODEGEN_RESULT;
    }

    // is used to get the llvm::Value of an lValue and rValue
    [[nodiscard]] llvm::Value* getValue() const {
        if (resultType == R_VALUE_CODEGEN_RESULT)
            return rValue.value;
        return lValue.value;
    }

    // is used to get the type, of an lValue and rValue
    [[nodiscard]] const std::shared_ptr<typeSystem::Type>& getType() const {
        if (resultType == R_VALUE_CODEGEN_RESULT)
            return rValue.type;
        return lValue.type;
    }

    // is used to get the pointer if it's an lValue
    [[nodiscard]] llvm::Value* getPointer() const {
        if (resultType == L_VALUE_CODEGEN_RESULT)
            return lValue.pointer;
        return nullptr;
    }

    // is used to check if it is mutable or not
    [[nodiscard]] bool isMutable() const {
        if (resultType == L_VALUE_CODEGEN_RESULT)
            return lValue.isMutable;
        // just return false as a default value
        return false;
    }
};

class ASTNode {
 public:
    virtual ~ASTNode() = default;

    virtual void print(int depth) const = 0;

    [[nodiscard]] virtual std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const = 0;
};

class AST {
    std::vector<ASTNode*> rootNodes;

 public:
    explicit AST(std::vector<ASTNode*> rootNodes)
        : rootNodes(std::move(rootNodes)) {}
    ~AST() {
        // delete each node the vector
        for (ASTNode* node : rootNodes) {
            delete node;
        }
    }

    void print() const {
        // print each node in the vector
        for (ASTNode* node : rootNodes) {
            node->print(0);
        }
    }

    void codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
                 const std::unique_ptr<llvm::IRBuilder<>>& builder,
                 const std::unique_ptr<llvm::Module>& moduleLLVM) const {
        // TODO(anyone): remove the hardcoded types for these functions
        scopes::setFunctionData(
            "printf", std::make_shared<typeSystem::IntegerType>(32, true),
            {std::make_shared<typeSystem::PointerType>(
                std::make_shared<typeSystem::CharacterType>(), false)},
            true);

        scopes::setFunctionData(
            "scanf", std::make_shared<typeSystem::IntegerType>(32, true),
            {std::make_shared<typeSystem::PointerType>(
                std::make_shared<typeSystem::CharacterType>(), false)},
            true);

        scopes::setFunctionData(
            "rand", std::make_shared<typeSystem::IntegerType>(32, true), {});

        scopes::setFunctionData(
            "srand", std::make_shared<typeSystem::VoidType>(),
            {std::make_shared<typeSystem::IntegerType>(32, true)});

        scopes::setFunctionData(
            "time", std::make_shared<typeSystem::IntegerType>(64, true),
            {std::make_shared<typeSystem::IntegerType>(64, true)});

        // codegen each node the vector
        for (ASTNode* node : rootNodes) {
            // no need to use the return value, for they are top level nodes...
            (void)node->codegen(ctx, builder, moduleLLVM);
        }

        // error if the main function is not present
        if (scopes::getFunctionData("main") == nullptr) {
            generator::fatal_error(std::chrono::high_resolution_clock::now(),
                                   "Missing main function",
                                   "The program must contain a main function");
        }
    }
};

class ASTVariableExpression : public ASTNode {
    std::string identifier;

 public:
    explicit ASTVariableExpression(std::string identifier)
        : identifier(std::move(identifier)) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ')
                  << "Variable Expression: " << identifier << '\n';
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTInteger : public ASTNode {
    std::uint64_t number;
    std::string typeSuffix;

 public:
    explicit ASTInteger(std::uint64_t number) : number(number) {}

    explicit ASTInteger(std::string string) {
        // remove any underscores
        string.erase(std::remove(string.begin(), string.end(), '_'),
                     string.end());

        // get an iterator to the first char that is not a number
        auto typeSuffixStartIterator =
            std::find_if(string.begin(), string.end(),
                         [](char c) { return !std::isdigit(c); });
        try {
            number = std::stoull(
                std::string(string.begin(), typeSuffixStartIterator));
        } catch (const std::out_of_range& e) {
            generator::fatal_error(
                std::chrono::high_resolution_clock::now(),
                "Invalid type annotated integer literal",
                "The integer value '" +
                    std::string(string.begin(), typeSuffixStartIterator) +
                    "' is to large");
            return;
        }
        typeSuffix = std::string(typeSuffixStartIterator, string.end());
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Integer: " << number
                  << '\n';
        if (!typeSuffix.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ')
                      << "Type suffix: " << typeSuffix << '\n';
        }
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTBool : public ASTNode {
    bool value;

 public:
    explicit ASTBool(bool value) : value(value) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Boolean: " << value
                  << '\n';
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTFloat : public ASTNode {
    double number;
    std::string typeSuffix;

 public:
    explicit ASTFloat(double number) : number(number) {}

    explicit ASTFloat(std::string string) {
        // remove any underscores
        string.erase(std::remove(string.begin(), string.end(), '_'),
                     string.end());

        // get an iterator to the first char that is not a number or a dot
        auto typeSuffixStartIterator =
            std::find_if(string.begin(), string.end(),
                         [](char c) { return !std::isdigit(c) && c != '.'; });
        try {
            number =
                std::stod(std::string(string.begin(), typeSuffixStartIterator));
        } catch (const std::out_of_range& e) {
            generator::fatal_error(
                std::chrono::high_resolution_clock::now(),
                "Invalid type annotated floating point literal",
                "The floating point value '" +
                    std::string(string.begin(), typeSuffixStartIterator) +
                    "' is to large");
            return;
        }
        typeSuffix = std::string(typeSuffixStartIterator, string.end());
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Float: " << number << '\n';
        if (!typeSuffix.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ')
                      << "Type suffix: " << typeSuffix << '\n';
        }
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTString : public ASTNode {
    std::string text;

 public:
    explicit ASTString(std::string text) : text(std::move(text)) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "String: " << text << '\n';
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTChar : public ASTNode {
    char character;

 public:
    explicit ASTChar(char character) : character(character) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Character: " << character
                  << '\n';
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTTypeCast : public ASTNode {
    ASTNode* expression;
    std::string type;

 public:
    explicit ASTTypeCast(ASTNode* expression, std::string type)
        : expression(expression), type(std::move(type)) {}
    ~ASTTypeCast() override { delete expression; }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Type Cast: " << type
                  << '\n';
        expression->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

// this could also be called a scope
class ASTCompoundStatement : public ASTNode {
    std::vector<ASTNode*> statementList;

 public:
    explicit ASTCompoundStatement(std::vector<ASTNode*> statementList)
        : statementList(std::move(statementList)) {}
    // also add an empty constructor for no statements
    ASTCompoundStatement() = default;
    ~ASTCompoundStatement() override {
        // delete each node the vector
        for (ASTNode* statement : statementList) {
            delete statement;
        }
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Compound Statement:\n";
        // print each node in the vector
        for (ASTNode* statement : statementList) {
            statement->print(depth + 1);
        }
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTBinaryOperator : public ASTNode {
    ASTNode* left;
    ASTNode* right;
    std::string operation;

 public:
    ASTBinaryOperator(ASTNode* left, ASTNode* right, std::string operation)
        : left(left), right(right), operation(std::move(operation)) {}
    ~ASTBinaryOperator() override {
        delete left;
        delete right;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ')
                  << "Binary Operator: " << operation << '\n';
        left->print(depth + 1);
        right->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTUnaryOperator : public ASTNode {
    ASTNode* expression;
    std::string operation;

 public:
    ASTUnaryOperator(ASTNode* expression, std::string operation)
        : expression(expression), operation(std::move(operation)) {}
    ~ASTUnaryOperator() override { delete expression; }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ')
                  << "Unary Operator: " << operation << '\n';
        expression->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTIncrementDecrementOperator : public ASTNode {
    std::string identifier;
    std::string operation;

 public:
    explicit ASTIncrementDecrementOperator(std::string identifier,
                                           std::string operation)
        : identifier(std::move(identifier)), operation(std::move(operation)) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ')
                  << "Increment/Decrement Operator:\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Identifier: " << identifier << "\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Operator Type: " << operation << "\n";
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTAddressOfOperator : public ASTNode {
    std::string identifier;
    bool isMutable;

 public:
    explicit ASTAddressOfOperator(std::string identifier, bool isMutable)
        : identifier(std::move(identifier)), isMutable(isMutable) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Address Of Operator:\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Is Mutable: " << isMutable << "\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Identifier: " << identifier << "\n";
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTDereferenceOperator : public ASTNode {
    ASTNode* expression;

 public:
    explicit ASTDereferenceOperator(ASTNode* expression)
        : expression(expression) {}
    ~ASTDereferenceOperator() override { delete expression; }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Dereference Operator:\n";
        expression->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTArrayIndex : public ASTNode {
    ASTNode* array;
    ASTNode* index;

 public:
    explicit ASTArrayIndex(ASTNode* array, ASTNode* index)
        : array(array), index(index) {}
    ~ASTArrayIndex() override {
        delete array;
        delete index;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Array Index:\n";
        array->print(depth + 1);
        index->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTParameter : public ASTNode {
    std::string identifier;
    std::string type;
    bool isMutable;

 public:
    ASTParameter(std::string identifier, std::string type, bool isMutable)
        : identifier(std::move(identifier)), type(std::move(type)),
          isMutable(isMutable) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Parameter:\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Is Mutable: " << isMutable << "\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Identifier: " << identifier << "\n";
        std::cout << std::string((depth + 1) * 2, ' ') << "Type: " << type
                  << "\n";
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTFunctionPrototype : public ASTNode {
    std::string identifier;
    std::vector<ASTNode*> parameterList;
    std::string returnType;

 public:
    ASTFunctionPrototype(std::string identifier,
                         std::vector<ASTNode*> parameterList,
                         std::string returnType = "")
        : identifier(std::move(identifier)),
          parameterList(std::move(parameterList)),
          returnType(std::move(returnType)) {}
    ~ASTFunctionPrototype() override {
        // delete each node the vector
        for (ASTNode* parameter : parameterList) {
            delete parameter;
        }
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Function Prototype:\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Identifier: " << identifier << "\n";
        if (!parameterList.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ') << "Parameters:\n";
            // print each node in the vector
            for (ASTNode* parameter : parameterList) {
                parameter->print((depth + 1) + 1);
            }
        } else {
            std::cout << std::string((depth + 1) * 2, ' ') << "No Parameters\n";
        }
        // check if string is not empty
        if (!returnType.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ')
                      << "Return type: " << returnType << "\n";
        } else {
            std::cout << std::string((depth + 1) * 2, ' ')
                      << "No Return Type\n";
        }
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTFunctionDefinition : public ASTNode {
    ASTNode* prototype;
    ASTNode* body;

 public:
    ASTFunctionDefinition(ASTNode* prototype, ASTNode* body)
        : prototype(prototype), body(body) {}
    ~ASTFunctionDefinition() override {
        delete prototype;
        delete body;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Function Definition:\n";
        prototype->print(depth + 1);
        body->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTWhileLoop : public ASTNode {
    ASTNode* expression;
    ASTNode* loopBody;

 public:
    explicit ASTWhileLoop(ASTNode* expression, ASTNode* loopBody)
        : expression(expression), loopBody(loopBody) {}
    ~ASTWhileLoop() override {
        delete expression;
        delete loopBody;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "While Loop:\n";
        expression->print(depth + 1);
        loopBody->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTForLoop : public ASTNode {
    ASTNode* initStatement;
    ASTNode* expression;
    ASTNode* updateStatement;
    ASTNode* loopBody;

 public:
    explicit ASTForLoop(ASTNode* initStatement, ASTNode* expression,
                        ASTNode* updateStatement, ASTNode* loopBody)
        : initStatement(initStatement), expression(expression),
          updateStatement(updateStatement), loopBody(loopBody) {}
    ~ASTForLoop() override {
        delete initStatement;
        delete expression;
        delete updateStatement;
        delete loopBody;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "For Loop:\n";
        initStatement->print(depth + 1);
        expression->print(depth + 1);
        updateStatement->print(depth + 1);
        loopBody->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTReturnStatement : public ASTNode {
    ASTNode* expression;

 public:
    explicit ASTReturnStatement(ASTNode* expression) : expression(expression) {}
    ASTReturnStatement() : expression(nullptr) {}
    ~ASTReturnStatement() override {
        // no need to check (deleting nullptr has no effect)
        delete expression;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Return Statement:\n";
        if (expression != nullptr) {
            expression->print(depth + 1);
        } else {
            std::cout << std::string((depth + 1) * 2, ' ') << "No Expression\n";
        }
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTContinueStatement : public ASTNode {
 public:
    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Continue Statement:\n";
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTBreakStatement : public ASTNode {
 public:
    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Break Statement:\n";
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTVariableDeclaration : public ASTNode {
    std::string identifier;
    std::string type;
    bool isMutable;

 public:
    ASTVariableDeclaration(std::string identifier, std::string type,
                           bool isMutable = false)
        : identifier(std::move(identifier)), type(std::move(type)),
          isMutable(isMutable) {}

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Variable Declaration:\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Identifier: " << identifier << "\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Mutable: " << isMutable << "\n";
        std::cout << std::string((depth + 1) * 2, ' ') << "Type: " << type
                  << "\n";
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTVariableAssignment : public ASTNode {
    ASTNode* leftExpression;
    ASTNode* rightExpression;
    std::string operation;

 public:
    ASTVariableAssignment(ASTNode* leftExpression, ASTNode* rightExpression,
                          std::string operation = "")
        : leftExpression(leftExpression), rightExpression(rightExpression),
          operation(std::move(operation)) {}
    ~ASTVariableAssignment() override {
        delete leftExpression;
        delete rightExpression;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Variable Assignment:\n";
        leftExpression->print(depth + 1);
        if (!operation.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ')
                      << "Operation: " << operation << "\n";
        }
        rightExpression->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTVariableDefinition : public ASTNode {
    std::string identifier;
    std::string type;
    ASTNode* expression;
    bool isMutable;

 public:
    ASTVariableDefinition(std::string identifier, std::string type,
                          ASTNode* expression, bool isMutable = false)
        : identifier(std::move(identifier)), type(std::move(type)),
          expression(expression), isMutable(isMutable) {}
    ~ASTVariableDefinition() override { delete expression; }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Variable Definition:\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Identifier: " << identifier << "\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Mutable: " << isMutable << "\n";
        std::cout << std::string((depth + 1) * 2, ' ') << "Type: " << type
                  << "\n";
        expression->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTIfStatement : public ASTNode {
    ASTNode* expression;
    ASTNode* body;

 public:
    explicit ASTIfStatement(ASTNode* expression, ASTNode* body)
        : expression(expression), body(body) {}
    ~ASTIfStatement() override {
        delete expression;
        delete body;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "If Statement:\n";
        expression->print(depth + 1);
        body->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTIfElseStatement : public ASTNode {
    ASTNode* expression;
    ASTNode* thenBody;
    ASTNode* elseBody;

 public:
    explicit ASTIfElseStatement(ASTNode* expression, ASTNode* thenBody,
                                ASTNode* elseBody)
        : expression(expression), thenBody(thenBody), elseBody(elseBody) {}
    ~ASTIfElseStatement() override {
        delete expression;
        delete thenBody;
        delete elseBody;
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "If-else Statement:\n";
        expression->print(depth + 1);
        thenBody->print(depth + 1);
        elseBody->print(depth + 1);
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTFunctionCall : public ASTNode {
    std::string identifier;
    std::vector<ASTNode*> argumentList;

 public:
    ASTFunctionCall(std::string identifier, std::vector<ASTNode*> argumentList)
        : identifier(std::move(identifier)),
          argumentList(std::move(argumentList)) {}
    ~ASTFunctionCall() override {
        // delete each node the vector
        for (ASTNode* argument : argumentList) {
            delete argument;
        }
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Function Call:\n";
        std::cout << std::string((depth + 1) * 2, ' ')
                  << "Identifier: " << identifier << "\n";
        if (!argumentList.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ') << "Arguments\n";
            // print each node in the vector
            for (ASTNode* argument : argumentList) {
                argument->print(depth + 2);
            }
        } else {
            std::cout << std::string((depth + 1) * 2, ' ') << "No Arguments\n";
        }
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};

class ASTArrayInitList : public ASTNode {
    std::vector<ASTNode*> elementList;
    std::string type;

 public:
    explicit ASTArrayInitList(std::vector<ASTNode*> elementList,
                              std::string type = "")
        : elementList(std::move(elementList)), type(std::move(type)) {}
    ~ASTArrayInitList() override {
        // delete each node the vector
        for (ASTNode* element : elementList) {
            delete element;
        }
    }

    void print(int depth) const override {
        std::cout << std::string(depth * 2, ' ') << "Array Init List:\n";
        // print the elements
        if (!elementList.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ') << "Elements\n";
            // print each node in the vector
            for (ASTNode* element : elementList) {
                element->print(depth + 2);
            }
        } else {
            std::cout << std::string((depth + 1) * 2, ' ') << "No Elements\n";
        }
        // print the type
        if (!type.empty()) {
            std::cout << std::string((depth + 1) * 2, ' ') << "Type: " << type
                      << "\n";
        } else {
            std::cout << std::string((depth + 1) * 2, ' ') << "No Type\n";
        }
    }

    [[nodiscard]] std::unique_ptr<CodegenResult>
    codegen(const std::unique_ptr<llvm::LLVMContext>& ctx,
            const std::unique_ptr<llvm::IRBuilder<>>& builder,
            const std::unique_ptr<llvm::Module>& moduleLLVM) const override;
};
