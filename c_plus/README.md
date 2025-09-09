# S3UploadLib - AWS S3 Upload Library for Windows

A C++ wrapper library that provides AWS S3 upload functionality with VB6 compatibility. This library encapsulates the AWS C++ SDK to enable file uploads to Amazon S3 from legacy Windows applications.

## 📋 Table of Contents

- [Prerequisites](#prerequisites)
- [Project Structure](#project-structure)
- [Build Environment](#build-environment)
- [Build Instructions](#build-instructions)
- [Command Line Notes](#command-line-notes)
- [Configuration Requirements](#configuration-requirements)
- [Troubleshooting](#troubleshooting)
- [API Reference](#api-reference)
- [Integration](#integration)

## 🛠️ Prerequisites

### Required Software
1. **Visual Studio 2022** with C++ development tools
   - MSVC v143 compiler toolset
   - Windows 10/11 SDK
   - CMake tools (optional)

2. **Windows OS**
   - Windows 10 or later (x86/x64)
   - Administrator privileges for installation

### AWS SDK Dependencies
- AWS C++ SDK (included in `aws-sdk-cpp/` directory)
- All required DLL files are pre-compiled and included

## 📁 Project Structure

```
c_plus/
├── README.md                    # This file
├── build_vs2022.cmd            # Build script for Visual Studio 2022
├── S3UploadLib.h               # Header file with API declarations
├── S3UploadLib.cpp             # Main implementation file
├── S3UploadLib.def             # DLL export definitions
├── S3UploadLib.dll             # Generated DLL (after build)
├── S3UploadLib.lib             # Generated import library (after build)
└── aws-sdk-cpp/                # AWS C++ SDK distribution
    ├── bin/                    # Release DLL files
    │   ├── aws-cpp-sdk-core.dll
    │   ├── aws-cpp-sdk-s3.dll
    │   ├── aws-c-*.dll         # AWS C libraries
    │   ├── aws-crt-cpp.dll
    │   └── zlib1.dll
    ├── debug/                  # Debug DLL files
    │   ├── bin/
    │   └── lib/
    ├── include/                # Header files
    │   ├── aws/
    │   ├── smithy/
    │   ├── zconf.h
    │   └── zlib.h
    ├── lib/                    # Static libraries (Release)
    └── share/                  # CMake configuration files
```

## 🔧 Build Environment

### Visual Studio Path Configuration

The build script expects Visual Studio 2022 to be installed at:
```
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat
```

**⚠️ IMPORTANT:** You may need to modify this path in `build_vs2022.cmd` line 6:

```batch
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
```

**Common Visual Studio 2022 Installation Paths:**
- `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat`
- `C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat`
- `C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat`
- `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat`

## 🚀 Build Instructions

### Method 1: Using Batch Script (Recommended)

1. **Open Command Prompt** (cmd.exe, not PowerShell)
   ```cmd
   cd /d E:\partner_integration\c_plus
   ```

2. **Execute build script**
   ```cmd
   cmd /c build_vs2022.cmd
   ```

### Method 2: Manual Build

1. **Setup Visual Studio Environment**
   ```cmd
   call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
   ```

2. **Compile Source Code**
   ```cmd
   cl /std:c++14 /EHsc /MD /c /DS3UPLOAD_EXPORTS /I"aws-sdk-cpp\include" S3UploadLib.cpp
   ```

3. **Link DLL**
   ```cmd
   link /DLL /OUT:S3UploadLib.dll S3UploadLib.obj /LIBPATH:"aws-sdk-cpp\lib" ^
        aws-cpp-sdk-core.lib aws-cpp-sdk-s3.lib aws-c-common.lib aws-c-auth.lib ^
        aws-c-cal.lib aws-c-compression.lib aws-c-event-stream.lib aws-c-http.lib ^
        aws-c-io.lib aws-c-mqtt.lib aws-c-s3.lib aws-c-sdkutils.lib aws-checksums.lib ^
        aws-crt-cpp.lib zlib.lib kernel32.lib user32.lib advapi32.lib ws2_32.lib ^
        /DEF:S3UploadLib.def
   ```

## 💻 Command Line Notes

### PowerShell vs Command Prompt

**⚠️ CRITICAL:** Always use **Command Prompt (cmd.exe)** for building, NOT PowerShell.

**Why?**
- PowerShell may have encoding issues with the batch file
- Visual Studio environment setup scripts are designed for cmd.exe
- Character encoding problems can cause compilation errors

### Encoding Issues

If you encounter compilation errors like:
```
error C2062: unexpected type 'char'
error C2059: syntax error ')'
```

**Solution:**
1. Ensure source files are saved in UTF-8 encoding
2. Use cmd.exe instead of PowerShell
3. Set code page if necessary: `chcp 65001`

### Build Output

**Successful build will produce:**
```
S3UploadLib.dll - Main library file
S3UploadLib.lib - Import library for linking
S3UploadLib.exp - Export file (generated automatically)
```

## ⚙️ Configuration Requirements

### File Paths to Modify

1. **Visual Studio Path** (in `build_vs2022.cmd`):
   ```batch
   call "YOUR_VS_PATH\VC\Auxiliary\Build\vcvars32.bat"
   ```

2. **Library Dependencies** (if using different AWS SDK version):
   - Update `/LIBPATH:` in link command
   - Update `/I"aws-sdk-cpp\include"` in compile command

### Environment Variables

The build process may require these environment variables:
- `INCLUDE` - Additional include directories
- `LIB` - Additional library directories
- `PATH` - Must include Visual Studio tools

## 🔍 Troubleshooting

### Common Build Errors

1. **"vcvars32.bat not found"**
   ```
   Solution: Update Visual Studio path in build_vs2022.cmd
   ```

2. **"Cannot open include file 'aws/core/Aws.h'"**
   ```
   Solution: Verify aws-sdk-cpp/include directory exists and is accessible
   ```

3. **"LNK1104: cannot open file 'aws-cpp-sdk-core.lib'"**
   ```
   Solution: Check that aws-sdk-cpp/lib directory contains all required .lib files
   ```

4. **Character encoding errors**
   ```
   Solution: Use cmd.exe instead of PowerShell, ensure UTF-8 encoding
   ```

### Verification Steps

1. **Check Visual Studio Installation**
   ```cmd
   where cl.exe
   ```

2. **Verify AWS SDK Files**
   ```cmd
   dir aws-sdk-cpp\lib\*.lib
   dir aws-sdk-cpp\include\aws\core\
   ```

3. **Test Generated DLL**
   ```cmd
   dumpbin /exports S3UploadLib.dll
   ```

## 📚 API Reference

### Core Functions

```cpp
// Initialize AWS SDK (call first)
int InitializeAwsSDK();

// Upload file with permanent credentials
int UploadFileToS3(
    const char* accessKey,
    const char* secretKey,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath
);

// Upload file with temporary credentials (STS)
int UploadFileToS3WithToken(
    const char* accessKey,
    const char* secretKey,
    const char* sessionToken,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath
);

// Get last error message
const char* GetS3LastError();

// Cleanup AWS SDK (call on exit)
void CleanupAwsSDK();
```

### Error Codes

```cpp
#define S3_SUCCESS                  0   // Success
#define S3_ERROR_INVALID_PARAMS    -1   // Invalid parameters
#define S3_ERROR_NOT_INITIALIZED   -2   // SDK not initialized
#define S3_ERROR_FILE_NOT_EXISTS   -3   // File does not exist
#define S3_ERROR_FILE_READ_ERROR   -4   // File read error
#define S3_ERROR_FILE_OPEN_ERROR   -5   // File open error
#define S3_ERROR_S3_UPLOAD_FAILED  -6   // S3 upload failed
#define S3_ERROR_EXCEPTION         -7   // Exception error
#define S3_ERROR_UNKNOWN           -8   // Unknown error
```

## 🔗 Integration

### VB6 Integration

1. **Copy DLL files to VB6 project directory:**
   ```
   S3UploadLib.dll
   aws-sdk-cpp\bin\*.dll (all AWS DLL files)
   ```

2. **Use VB6 declarations:**
   ```vb
   ' Include S3UploadLib.bas in your VB6 project
   ' Call functions using provided declarations
   ```

### C++ Integration

1. **Include header file:**
   ```cpp
   #include "S3UploadLib.h"
   ```

2. **Link import library:**
   ```cpp
   #pragma comment(lib, "S3UploadLib.lib")
   ```

## 📄 License

- **S3UploadLib**: Custom implementation for internal use
- **AWS C++ SDK**: Apache License 2.0
- **Dependencies**: Various open source licenses (see aws-sdk-cpp/share/)

## 🆘 Support

For build issues or integration questions:

1. Check this README first
2. Verify all prerequisites are installed
3. Ensure Visual Studio path is correct
4. Use cmd.exe for building, not PowerShell
5. Check encoding of source files if compilation fails

---

**Version:** 1.0  
**Compatibility:** Windows 10+, Visual Studio 2022, VB6+
