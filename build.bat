@echo off
REM build.bat - One-step build for Seu compiler (SeuLex + SeuYacc + frontend)
REM Recompiles a .cpp only when source is newer than the matching .o (incremental).

if not exist generated mkdir generated
if not exist build mkdir build
if not exist output mkdir output

set INC=-Igenerated -Iinclude -IFrontend -ISeuLex -ISeuYacc
set CXXFLAGS=-std=c++11 -Wall %INC%

echo === Building SeuLex ===
call :link_if_newer build\seulex.exe SeuLex\LexFileParser.cpp SeuLex\RegExpParser.cpp SeuLex\NFAConstructor.cpp SeuLex\DFAConverter.cpp SeuLex\DFAMinimizer.cpp SeuLex\CodeGenerator.cpp SeuLex\lex_main.cpp
if errorlevel 1 goto error
if "%NEED_LINK%"=="1" (
    g++ -o build\seulex.exe SeuLex\LexFileParser.cpp SeuLex\RegExpParser.cpp SeuLex\NFAConstructor.cpp SeuLex\DFAConverter.cpp SeuLex\DFAMinimizer.cpp SeuLex\CodeGenerator.cpp SeuLex\lex_main.cpp %CXXFLAGS%
    if errorlevel 1 goto error
)

echo === Running SeuLex ===
if exist grammar\lex.l (
    for %%I in (grammar\lex.l) do set LEX_TIME=%%~tI
    for %%I in (generated\lex.yy.cpp) do set YY_TIME=%%~tI
    if not exist generated\lex.yy.cpp (
        build\seulex.exe grammar\lex.l
        if errorlevel 1 goto error
    ) else if "%LEX_TIME%" gtr "%YY_TIME%" (
        build\seulex.exe grammar\lex.l
        if errorlevel 1 goto error
    )
)

echo === Building SeuYacc ===
call :link_if_newer build\seuyacc.exe SeuYacc\grammar.cpp SeuYacc\first_set.cpp SeuYacc\lr1_dfa.cpp SeuYacc\lalr.cpp SeuYacc\parsing_table.cpp SeuYacc\code_gen.cpp SeuYacc\yacc_main.cpp SeuYacc\common.cpp
if errorlevel 1 goto error
if "%NEED_LINK%"=="1" (
    g++ -o build\seuyacc.exe SeuYacc\grammar.cpp SeuYacc\first_set.cpp SeuYacc\lr1_dfa.cpp SeuYacc\lalr.cpp SeuYacc\parsing_table.cpp SeuYacc\code_gen.cpp SeuYacc\yacc_main.cpp SeuYacc\common.cpp %CXXFLAGS%
    if errorlevel 1 goto error
)

echo === Running SeuYacc (generate yyparse.cpp) ===
if exist grammar\yacc.y (
    for %%I in (grammar\yacc.y) do set YACC_TIME=%%~tI
    for %%I in (generated\yyparse.cpp) do set PARSE_TIME=%%~tI
    if not exist generated\yyparse.cpp (
        build\seuyacc.exe --lalr grammar\yacc.y generated\yyparse.cpp
        if errorlevel 1 goto error
    ) else if "%YACC_TIME%" gtr "%PARSE_TIME%" (
        build\seuyacc.exe --lalr grammar\yacc.y generated\yyparse.cpp
        if errorlevel 1 goto error
    )
)

echo === Compiling generated code ===
call :compile generated\yyparse.cpp build\yyparse.o
if errorlevel 1 goto error
call :compile generated\lex.yy.cpp build\lex.yy.o
if errorlevel 1 goto error
call :compile Frontend\type.cpp build\type.o
if errorlevel 1 goto error
call :compile Frontend\symbol.cpp build\symbol.o
if errorlevel 1 goto error
call :compile Frontend\typecheck.cpp build\typecheck.o
if errorlevel 1 goto error
call :compile Frontend\ir.cpp build\ir.o
if errorlevel 1 goto error
call :compile Frontend\irgen.cpp build\irgen.o
if errorlevel 1 goto error
call :compile Frontend\ir_opt.cpp build\ir_opt.o
if errorlevel 1 goto error
call :compile Frontend\compile_pipeline.cpp build\compile_pipeline.o
if errorlevel 1 goto error
call :compile Frontend\ast_format.cpp build\ast_format.o
if errorlevel 1 goto error
call :compile Frontend\token_names.cpp build\token_names.o
if errorlevel 1 goto error
call :compile Frontend\ast_printer.cpp build\ast_printer.o
if errorlevel 1 goto error
call :compile Frontend\ast_dot.cpp build\ast_dot.o
if errorlevel 1 goto error
if not exist build\main_front.o goto compile_main
for %%I in (Frontend\main_front.cpp) do set MFT=%%~tI
for %%I in (build\main_front.o) do set MOT=%%~tI
if "%MFT%" gtr "%MOT%" goto compile_main
goto main_front_done
:compile_main
g++ -c Frontend\main_front.cpp -o build\main_front.o %CXXFLAGS% -DDEBUG=0 -finput-charset=UTF-8 -fexec-charset=UTF-8
if errorlevel 1 goto error
:main_front_done

echo === Linking ===
set LINK_NEEDED=1
if exist build\compiler.exe (
    set LINK_NEEDED=0
    for %%O in (
        build\main_front.o build\lex.yy.o build\yyparse.o build\type.o build\symbol.o
        build\typecheck.o build\ir.o build\irgen.o build\ir_opt.o build\compile_pipeline.o
        build\ast_format.o build\ast_printer.o build\ast_dot.o build\token_names.o
    ) do (
        for %%I in ("%%~O") do set OT=%%~tI
        for %%E in ("build\compiler.exe") do set ET=%%~tE
        if "%%~tO" gtr "%%~tE" set LINK_NEEDED=1
    )
)
if "%LINK_NEEDED%"=="1" (
    g++ -o build\compiler.exe build\main_front.o build\lex.yy.o build\yyparse.o build\type.o build\symbol.o build\typecheck.o build\ir.o build\irgen.o build\ir_opt.o build\compile_pipeline.o build\ast_format.o build\ast_printer.o build\ast_dot.o build\token_names.o -std=c++11 -Wall
    if errorlevel 1 goto error
) else (
    echo   compiler.exe up to date
)

echo Build successful! Run: build\compiler.exe examples\test.c
goto end

:compile
set "SRC=%~1"
set "OBJ=%~2"
set "EXTRA=%~3 %~4 %~5"
if not exist "%OBJ%" goto do_compile
for %%I in ("%SRC%") do set ST=%%~tI
for %%I in ("%OBJ%") do set OT=%%~tI
if "%ST%" gtr "%OT%" goto do_compile
exit /b 0
:do_compile
g++ -c "%SRC%" -o "%OBJ%" %CXXFLAGS% %EXTRA%
exit /b %ERRORLEVEL%

:link_if_newer
set NEED_LINK=0
set "OUT=%~1"
shift
if not exist "%OUT%" (
    set NEED_LINK=1
    exit /b 0
)
:link_check
if "%~1"=="" exit /b 0
for %%I in ("%~1") do set ST=%%~tI
for %%I in ("%OUT%") do set OT=%%~tI
if "%ST%" gtr "%OT%" set NEED_LINK=1
shift
goto link_check

:error
echo Build failed.
:end
