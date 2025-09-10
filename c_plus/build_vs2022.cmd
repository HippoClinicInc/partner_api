@echo off
echo Building S3UploadLib.dll with Visual Studio 2022
echo.

REM Set VS 2022 environment
call "F:\visualStudio\VC\Auxiliary\Build\vcvars32.bat"

echo Build environment setup complete
echo.

REM Compile object file
echo Step 1: Compiling source file
cl /std:c++14 /EHsc /MD /c /DS3UPLOAD_EXPORTS /I"aws-sdk-cpp\include" S3UploadLib.cpp

if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo.
echo Step 2: Linking to create DLL...
link /DLL /OUT:S3UploadLib.dll S3UploadLib.obj /LIBPATH:"aws-sdk-cpp\lib" aws-cpp-sdk-core.lib aws-cpp-sdk-s3.lib aws-c-common.lib aws-c-auth.lib aws-c-cal.lib aws-c-compression.lib aws-c-event-stream.lib aws-c-http.lib aws-c-io.lib aws-c-mqtt.lib aws-c-s3.lib aws-c-sdkutils.lib aws-checksums.lib aws-crt-cpp.lib zlib.lib kernel32.lib user32.lib advapi32.lib ws2_32.lib /DEF:S3UploadLib.def

if %ERRORLEVEL% neq 0 (
    echo Linking failed!
    pause
    exit /b 1
)

echo.
echo Build successful! Generated files:
if exist S3UploadLib.dll echo S3UploadLib.dll - Build successful!
if exist S3UploadLib.lib echo S3UploadLib.lib - Import library generated!

echo.
echo Next steps:
echo 1. Copy S3UploadLib.dll to your VB6 program directory
echo 2. Copy all DLL files from aws-sdk-cpp\bin to VB6 program directory
echo 3. Use S3UploadLib.bas declarations in your VB6 project
echo.
pause
