cmake_minimum_required(VERSION 3.26)
project(Lotus)

set(CMAKE_CXX_STANDARD 23)

# Define the output directory for binaries
set(OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# Specify the directory for the binary outputs (executables and libraries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIRECTORY})

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
find_package(LLVM REQUIRED CONFIG)

# Generate lexer file with flex
flex_target(Lexer src/lexer/lexer.l src/lexer/lex.yy.cpp)

# Generate parser file with bison
bison_target(Parser src/parser/parser.y src/parser/parser.tab.cpp DEFINES_FILE src/parser/parser.tab.h)

include_directories(src/parser ${LLVM_INCLUDE_DIRS})

# Add preprocessor definitions necessary for building against llvm
add_definitions(${LLVM_DEFINITIONS})

add_executable(Lotus src/main.cpp
        tests/errors/test_errors.cpp
        tests/lexer/test_lexer.cpp
        tests/parser/test_parser.cpp
        tests/performance/test_performance.cpp
        tests/preprocessor/test_preprocessor.cpp
        src/diagnostics/internal/internal.h
        src/diagnostics/common.h
        src/diagnostics/lexer.h
        src/diagnostics/parser.h
        src/diagnostics/preprocessor.h
        src/diagnostics/generator.h
        src/preprocessor/preprocessor.h
        src/preprocessor/preprocessor.cpp
        src/parser/wrappers.h
        src/parser/ast.h
        src/parser/parse.h
        src/generator/generator.h
        src/generator/codegen.h
        src/generator/scopeStack.h
        src/generator/loopContext.h
        ${BISON_Parser_OUTPUTS}
        ${FLEX_Lexer_OUTPUTS}
        src/generator/typeSystem.h
        src/generator/casting.h
        src/generator/operators.h
)

# Ensures that llvm links against the correct libraries
llvm_map_components_to_libnames(llvm_libs support core irreader)
target_link_libraries(Lotus ${llvm_libs})

set_target_properties(Lotus PROPERTIES OUTPUT_NAME "lotus")
