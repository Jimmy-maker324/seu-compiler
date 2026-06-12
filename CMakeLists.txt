cmake_minimum_required(VERSION 3.16)
project(seu_compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GEN_DIR "${CMAKE_SOURCE_DIR}/generated")
set(SEULEX_DIR "${CMAKE_SOURCE_DIR}/SeuLex")
set(SEUYACC_DIR "${CMAKE_SOURCE_DIR}/SeuYacc")

include_directories(
    ${GEN_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/Frontend
    ${SEULEX_DIR}
    ${SEUYACC_DIR}
)

file(MAKE_DIRECTORY ${GEN_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/output)

add_executable(seulex
    SeuLex/LexFileParser.cpp SeuLex/RegExpParser.cpp SeuLex/NFAConstructor.cpp
    SeuLex/DFAConverter.cpp SeuLex/DFAMinimizer.cpp SeuLex/CodeGenerator.cpp
    SeuLex/lex_main.cpp)
target_compile_options(seulex PRIVATE -Wall)

add_executable(seuyacc
    SeuYacc/grammar.cpp SeuYacc/first_set.cpp SeuYacc/lr1_dfa.cpp SeuYacc/lalr.cpp
    SeuYacc/parsing_table.cpp SeuYacc/code_gen.cpp SeuYacc/yacc_main.cpp
    SeuYacc/common.cpp)
target_compile_options(seuyacc PRIVATE -Wall)

add_custom_command(
    OUTPUT ${GEN_DIR}/lex.yy.cpp ${GEN_DIR}/lex.yy.h
    COMMAND seulex ${CMAKE_SOURCE_DIR}/grammar/lex.l
    DEPENDS seulex ${CMAKE_SOURCE_DIR}/grammar/lex.l
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating lex.yy.cpp"
)

add_custom_command(
    OUTPUT ${GEN_DIR}/yyparse.cpp
    COMMAND seuyacc --lalr ${CMAKE_SOURCE_DIR}/grammar/yacc.y ${GEN_DIR}/yyparse.cpp
    DEPENDS seuyacc ${CMAKE_SOURCE_DIR}/grammar/yacc.y
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating yyparse.cpp"
)

set(FRONTEND_SOURCES
    Frontend/main_front.cpp
    Frontend/type.cpp Frontend/symbol.cpp Frontend/typecheck.cpp
    Frontend/ir.cpp Frontend/irgen.cpp Frontend/ir_opt.cpp
    Frontend/compile_pipeline.cpp
    Frontend/ast_format.cpp Frontend/ast_printer.cpp Frontend/ast_dot.cpp
    Frontend/token_names.cpp
    ${GEN_DIR}/lex.yy.cpp
    ${GEN_DIR}/yyparse.cpp
)

add_executable(compiler ${FRONTEND_SOURCES})
target_compile_options(compiler PRIVATE -Wall)
target_compile_definitions(compiler PRIVATE DEBUG=0)
if(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(compiler PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()
