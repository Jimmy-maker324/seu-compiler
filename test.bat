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

REM  format: source.c expected_exit
call :run_case test.c          0
call :run_case block_scope.c   0
call :run_case syntax_err.c    1
call :run_case redef.c         2
call :run_case bad_break.c     2
call :run_case bad_continue.c  2
call :run_case dup_case.c         2
call :run_case bad_void_return.c  2
call :run_case bad_empty_return.c 2
call :run_case bad_return_type.c  2
call :run_case string_test.c      0
call :run_case bad_string_int.c   2

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
    goto :eof
)

call :run_compiler "!PATH_EX!"
set "ACTUAL=%ERRORLEVEL%"

if not "!ACTUAL!"=="!EXPECT!" (
    echo [FAIL] !SRC!  expected exit !EXPECT!, got !ACTUAL!
    set /a FAIL+=1
    goto :eof
)

if "!EXPECT!"=="0" (
    if not exist "output\output.ir" (
        echo [FAIL] !SRC!  exit 0 but output\output.ir missing
        set /a FAIL+=1
        goto :eof
    )
)

if "!EXPECT!"=="2" (
    REM Semantic failure must not produce optimized IR (main returns before IR dump)
    REM Allow stale output.ir from a prior test; only exit code is authoritative here.
)

echo [PASS] !SRC!  exit=!EXPECT!
set /a PASS+=1
goto :eof

:run_compiler
"%COMPILER%" %~1 >nul 2>&1
exit /b %ERRORLEVEL%
