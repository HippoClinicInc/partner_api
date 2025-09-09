@echo off
echo AWS C++ SDK Installation Script
echo ===============================

REM Set variables
set VCPKG_DIR=%cd%\vcpkg
set AWS_SDK_DIR=%cd%\aws-sdk-cpp

REM Step 1: Check and install vcpkg
echo Step 1: Checking for vcpkg...
if exist "%VCPKG_DIR%" (
    echo vcpkg already exists at %VCPKG_DIR%
) else (
    echo Installing vcpkg...

    REM Download vcpkg
    echo Downloading vcpkg...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip' -OutFile 'vcpkg.zip'"

    if not exist "vcpkg.zip" (
        echo Failed to download vcpkg.
        echo Please download manually from: https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip
        pause
        exit /b 1
    )

    REM Extract vcpkg
    echo Extracting vcpkg...
    powershell -Command "Expand-Archive -Path 'vcpkg.zip' -DestinationPath 'vcpkg-temp'"

    if not exist "vcpkg-temp" (
        echo Failed to extract vcpkg.
        pause
        exit /b 1
    )

    REM Rename directory
    move "vcpkg-temp\vcpkg-master" "vcpkg" >nul
    rmdir /s /q "vcpkg-temp" >nul
    del "vcpkg.zip" >nul

    if not exist "vcpkg" (
        echo Failed to create vcpkg directory.
        pause
        exit /b 1
    )
)

REM Step 2: Bootstrap vcpkg
echo Step 2: Bootstrapping vcpkg...
cd "%VCPKG_DIR%"
call bootstrap-vcpkg.bat
if %errorlevel% neq 0 (
    echo Failed to bootstrap vcpkg.
    cd ..
    pause
    exit /b 1
)
cd ..

REM Step 3: Install AWS SDK (32-bit)
echo Step 3: Installing AWS SDK (32-bit)...
echo This may take a while...
"%VCPKG_DIR%\vcpkg.exe" install aws-sdk-cpp[s3]:x86-windows
if %errorlevel% neq 0 (
    echo Failed to install AWS SDK.
    pause
    exit /b 1
)

REM Step 4: Create directory structure
echo Step 4: Creating directory structure...
if not exist "%AWS_SDK_DIR%" mkdir "%AWS_SDK_DIR%"

REM Step 5: Copy files
echo Step 5: Copying files...
xcopy /s /e /y "%VCPKG_DIR%\installed\x86-windows\*" "%AWS_SDK_DIR%\" >nul

echo.
echo Installation completed successfully!
echo AWS SDK files are available in: %AWS_SDK_DIR%
echo.
pause