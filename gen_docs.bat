@echo off
REM Generate API documentation with Doxygen
REM Requires: doxygen in PATH
REM Optional: graphviz (dot) for class diagrams

where doxygen >nul 2>&1
if errorlevel 1 (
    echo [ERROR] doxygen not found. Install from https://www.doxygen.nl/download.html
    echo         and add its bin folder to PATH.
    exit /b 1
)

echo === Running Doxygen ===
doxygen config\Doxyfile
if errorlevel 1 (
    echo [ERROR] Doxygen failed.
    exit /b 1
)

echo.
echo === Done ===
echo Open: docs\html\index.html
echo.
if exist docs\html\index.html start docs\html\index.html
