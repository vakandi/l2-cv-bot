@echo off
echo Building L2 CV Bot for modern Windows 10/11...
echo.

REM Check if OpenCV_DIR is set
if "%OpenCV_DIR%"=="" (
    echo ERROR: OpenCV_DIR environment variable is not set!
    echo Please set it to your OpenCV installation directory.
    echo Example: set OpenCV_DIR=C:\opencv\opencv\build
    echo.
    pause
    exit /b 1
)

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake is not installed or not in PATH!
    echo Please install CMake from https://cmake.org/download/
    echo.
    pause
    exit /b 1
)

echo Using OpenCV at: %OpenCV_DIR%
echo.

REM Create build directory and configure
echo Configuring project...
cmake -H. -Bbuild-modern -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    echo Please check your OpenCV installation and CMake setup.
    echo.
    pause
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build build-modern --config Release
if errorlevel 1 (
    echo ERROR: Build failed!
    echo Please check the error messages above.
    echo.
    pause
    exit /b 1
)

REM Copy OpenCV DLL
echo Copying OpenCV DLL...
for %%i in ("%OpenCV_DIR%\x64\vc16\bin\opencv_world*.dll") do (
    copy "%%i" "build-modern\Release\" >nul
    if errorlevel 1 (
        echo WARNING: Could not copy OpenCV DLL. You may need to copy it manually.
    ) else (
        echo Copied: %%~nxi
    )
)

REM Copy run script
copy "install\run.bat" "build-modern\Release\" >nul

echo.
echo Build completed successfully!
echo Executable location: build-modern\Release\l2-cv-bot.exe
echo Run script location: build-modern\Release\run.bat
echo.
echo Next steps:
echo 1. Install Interception driver (run as Administrator): install-interception.exe /install
echo 2. Reboot your computer
echo 3. Run Lineage II and start the bot with: run.bat "Lineage II"
echo.
pause