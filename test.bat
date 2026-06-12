@echo off
REM test.bat - Regression tests for compiler.exe (expected exit codes)
REM Usage:
REM   test.bat           Run all cases (build first if compiler missing)
REM   test.bat --no-build  Skip build even when compiler.exe is missing

setlocal EnableDelayedExpansion

set "COMPILER=build\compiler.exe"
set "DO_BUILD=1"
if /i "%~1"=="--no-build" set "DO_BUILD=0"

if not exist "%COMPILER%" (
    if "!DO_BUILD!"=="0" (
        echo [ERROR] %COMPILER% not found. Run build.bat or omit --no-build.
        exit /b 1
    )
    echo === Compiler missing, running build.bat ===
    call build.bat
    if errorlevel 1 (
        echo [ERROR] build.bat failed.
        exit /b 1
    )
    if not exist "%COMPILER%" (
        echo [ERROR] build succeeded but %COMPILER% still missing.
        exit /b 1
    )
)

set "PASS=0"
set "FAIL=0"

echo === Seu compiler regression tests ===
echo.

call :run_case test.c          0
call :run_case block_scope.c   0
call :run_case for_loop.c      0
call :run_case logic_ops.c     0
call :run_case syntax_err.c    1
call :run_case redef.c         2
call :run_case bad_break.c     2
call :run_case bad_continue.c  2
call :run_case bad_continue_switch.c 2
call :run_case dup_case.c         2
call :run_case bad_void_return.c  2
call :run_case bad_empty_return.c 2
call :run_case bad_return_type.c  2
call :run_case bad_missing_return.c 2
call :run_case bad_undef.c        2
call :run_case bad_call.c         2
call :run_case string_test.c      0
call :run_case bad_string_int.c   2
call :run_no_opt_test
call :run_custom_output_test

echo.
echo === Summary: !PASS! passed, !FAIL! failed ===
if !FAIL! gtr 0 exit /b 1
exit /b 0

:run_case
set "SRC=%~1"
set "EXPECT=%~2"
set "PATH_EX=examples\!SRC!"

if not exist "!PATH_EX!" (
    echo [FAIL] !SRC!  file not found
    set /a FAIL+=1
    exit /b 0
)

if exist "output\output.ir" del /q "output\output.ir"
if exist "output\output_raw.ir" del /q "output\output_raw.ir"

call :run_compiler "!PATH_EX!"
set "ACTUAL=!ERRORLEVEL!"

if not "!ACTUAL!"=="!EXPECT!" (
    echo [FAIL] !SRC!  expected exit !EXPECT!, got !ACTUAL!
    set /a FAIL+=1
    exit /b 0
)

if "!EXPECT!"=="0" (
    if not exist "output\output.ir" (
        echo [FAIL] !SRC!  exit 0 but output\output.ir missing
        set /a FAIL+=1
        exit /b 0
    )
    if /i "!SRC!"=="test.c" (
        findstr /C:"(func, factorial" "output\output.ir" >nul 2>&1
        if errorlevel 1 (
            echo [FAIL] test.c  output.ir missing expected factorial IR
            set /a FAIL+=1
            exit /b 0
        )
    )
)

if "!EXPECT!"=="2" (
    if exist "output\output.ir" (
        echo [FAIL] !SRC!  exit 2 but output\output.ir was generated
        set /a FAIL+=1
        exit /b 0
    )
)

echo [PASS] !SRC!  exit=!EXPECT!
set /a PASS+=1
exit /b 0

:run_no_opt_test
if exist "output\output.ir" del /q "output\output.ir"
if exist "output\output_raw.ir" del /q "output\output_raw.ir"
call :run_compiler examples\test.c --no-opt
set "ACTUAL=!ERRORLEVEL!"
if not "!ACTUAL!"=="0" (
    echo [FAIL] --no-opt  expected exit 0, got !ACTUAL!
    set /a FAIL+=1
    exit /b 0
)
if not exist "output\output.ir" (
    echo [FAIL] --no-opt  output\output.ir missing
    set /a FAIL+=1
    exit /b 0
)
if not exist "output\output_raw.ir" (
    echo [FAIL] --no-opt  output\output_raw.ir missing
    set /a FAIL+=1
    exit /b 0
)
fc /b "output\output.ir" "output\output_raw.ir" >nul 2>&1
if errorlevel 1 (
    echo [FAIL] --no-opt  output.ir differs from output_raw.ir
    set /a FAIL+=1
    exit /b 0
)
echo [PASS] --no-opt  output.ir matches output_raw.ir
set /a PASS+=1
exit /b 0

:run_custom_output_test
if exist "output\custom_report.txt" del /q "output\custom_report.txt"
call :run_compiler examples\test.c -o output\custom_report.txt
set "ACTUAL=!ERRORLEVEL!"
if not "!ACTUAL!"=="0" (
    echo [FAIL] -o custom  expected exit 0, got !ACTUAL!
    set /a FAIL+=1
    exit /b 0
)
if not exist "output\custom_report.txt" (
    echo [FAIL] -o custom  custom report missing
    set /a FAIL+=1
    exit /b 0
)
echo [PASS] -o custom  report written
set /a PASS+=1
exit /b 0

:run_compiler
"%COMPILER%" %* >nul 2>&1
exit /b !ERRORLEVEL!
