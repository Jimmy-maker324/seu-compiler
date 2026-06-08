@echo off
REM build.bat - One-step build for Seu compiler (SeuLex + SeuYacc + frontend)

if not exist generated mkdir generated
if not exist build mkdir build
if not exist output mkdir output

set INC=-Igenerated -Iinclude -IFrontend -ISeuLex -ISeuYacc
set CXXFLAGS=-std=c++11 -Wall %INC%

echo === Building SeuLex ===
g++ -o build\seulex.exe SeuLex\LexFileParser.cpp SeuLex\RegExpParser.cpp SeuLex\NFAConstructor.cpp SeuLex\DFAConverter.cpp SeuLex\DFAMinimizer.cpp SeuLex\CodeGenerator.cpp SeuLex\lex_main.cpp %CXXFLAGS%
if errorlevel 1 goto error

echo === Running SeuLex ===
build\seulex.exe grammar\lex.l
if errorlevel 1 goto error

echo === Building SeuYacc ===
g++ -o build\seuyacc.exe SeuYacc\grammar.cpp SeuYacc\first_set.cpp SeuYacc\lr1_dfa.cpp SeuYacc\lalr.cpp SeuYacc\parsing_table.cpp SeuYacc\code_gen.cpp SeuYacc\yacc_main.cpp SeuYacc\common.cpp %CXXFLAGS%
if errorlevel 1 goto error

echo === Running SeuYacc (generate yyparse.cpp) ===
build\seuyacc.exe --lalr grammar\yacc.y generated\yyparse.cpp
if errorlevel 1 goto error

echo === Compiling generated code ===
g++ -c generated\yyparse.cpp -o build\yyparse.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c generated\lex.yy.cpp -o build\lex.yy.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\type.cpp -o build\type.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\symbol.cpp -o build\symbol.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\typecheck.cpp -o build\typecheck.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\ir.cpp -o build\ir.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\irgen.cpp -o build\irgen.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\ir_opt.cpp -o build\ir_opt.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\ast_format.cpp -o build\ast_format.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\token_names.cpp -o build\token_names.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\ast_printer.cpp -o build\ast_printer.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\ast_dot.cpp -o build\ast_dot.o %CXXFLAGS%
if errorlevel 1 goto error
g++ -c Frontend\main_front.cpp -o build\main_front.o %CXXFLAGS% -DDEBUG=0 -finput-charset=UTF-8 -fexec-charset=UTF-8
if errorlevel 1 goto error

echo === Linking ===
g++ -o build\compiler.exe build\main_front.o build\lex.yy.o build\yyparse.o build\type.o build\symbol.o build\typecheck.o build\ir.o build\irgen.o build\ir_opt.o build\ast_format.o build\ast_printer.o build\ast_dot.o build\token_names.o -std=c++11 -Wall
if errorlevel 1 goto error

echo Build successful! Run: build\compiler.exe examples\test.c
goto end
:error
echo Build failed.
:end
