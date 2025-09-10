# HippoClinic Data Integration Demo - Upload Module

This folder contains a VB6-based file upload system that integrates with the HippoClinic API and AWS S3 for medical data uploads. The system supports both single file and batch folder uploads with comprehensive error handling and logging.

## 📁 Folder Contents

### Core VB6 Modules

#### `Main.bas`
**Main application module** - Contains the complete file upload workflow:
- **Main()** - Primary workflow function with 11-step process

**Key Functions:**
- `Main()` - Primary workflow entry point
- `UploadSingleFile()` - Upload individual files to S3
- `UploadFolderContents()` - Batch upload entire folders

#### `HippoBackend.bas`
**HippoClinic API Integration** - Backend API communication module:
- **Authentication** - Login and JWT token management
- **Patient Management** - Patient record creation and management
- **API Communication** - HTTP requests to HippoClinic API endpoints
- **Credential Management** - S3 credentials retrieval

**Key Functions:**
- `LoginAndGetToken()` - Authenticate with HippoClinic API
- `CreatePatient()` - Create patient records
- `GetS3Credentials()` - Retrieve AWS S3 temporary credentials
- `GenerateDataId()` - Generate unique data identifiers
- `ConfirmUploadRawFile()` - Confirm uploads with API

#### `FileLib.bas`
**File System Operations** - File and folder management utilities:
- **File Validation** - File and folder existence checking
- **Path Processing** - File path validation and processing
- **System Integration** - Windows file system operations

**Key Functions:**
- `FileOrFolderExists()` - Check if file or folder exists
- `GetLocalFileSize()` - Get file size in bytes
- `IsPathFolder()` - Check if path is a directory

#### `S3UploadLib.bas`
**AWS S3 Library Interface** - VB6 declarations for S3 operations:
- **API Declarations** - Function prototypes for S3UploadLib.dll
- **Error Constants** - Standardized error codes and messages
- **Helper Functions** - Error message translation utilities

**Key Features:**
- Support for both permanent and temporary AWS credentials
- File existence and size validation
- Comprehensive error code system
- Simple and advanced upload methods

#### `JsonConverter.bas`
**JSON Processing Library** - VBA-JSON v2.3.1 implementation:
- **JSON Parsing** - Convert JSON strings to VBA objects
- **JSON Generation** - Convert VBA objects to JSON strings
- **Error Handling** - Robust JSON parsing error management
- **Cross-Platform** - Support for both Windows and Mac VBA

### AWS SDK Libraries

#### Core AWS C++ Libraries
- `aws-cpp-sdk-core.dll` (1.4MB) - AWS SDK core functionality
- `aws-cpp-sdk-s3.dll` (2.3MB) - S3-specific operations
- `aws-crt-cpp.dll` (424KB) - AWS Common Runtime C++

#### AWS C Libraries
- `aws-c-s3.dll` (170KB) - S3 C library
- `aws-c-auth.dll` (169KB) - Authentication library
- `aws-c-http.dll` (284KB) - HTTP client library
- `aws-c-io.dll` (248KB) - I/O operations library
- `aws-c-mqtt.dll` (272KB) - MQTT client library
- `aws-c-common.dll` (175KB) - Common utilities
- `aws-c-sdkutils.dll` (76KB) - SDK utilities
- `aws-c-event-stream.dll` (70KB) - Event streaming
- `aws-c-cal.dll` (48KB) - Cryptographic library
- `aws-checksums.dll` (77KB) - Checksum utilities
- `aws-c-compression.dll` (13KB) - Compression library

#### System Dependencies
- `zlib1.dll` (77KB) - Compression library
- `S3UploadLib.dll` (87KB) - Custom S3 wrapper library

## 🚀 Quick Start

### Prerequisites
**VB6 Environment** - Visual Basic 6.0 or compatible IDE
> **Where to get Visual Basic 6.0?**  
> Visual Basic 6.0 is legacy software and is no longer officially distributed by Microsoft. For educational or legacy maintenance purposes, you can find installation media and instructions at [WinWorldPC - Visual Basic 6.0](https://winworldpc.com/product/microsoft-visual-bas/60).

**Visual C++ Redistributable Runtimes 32-bit**
If you having issue with the DLL files, install the Visual C++ Redistributable Runtimes 32-bit maybe helpful.
- [Visual C++ Redistributable Runtimes](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)

You can find the installer in the `cpp_runtime` folder. click the `install_all.bat` to install all the runtimes.

#### Visual Basic 6.0 Installation Steps

1. **Download the Installer**  
   Go to [WinWorldPC - Visual Basic 6.0](https://winworldpc.com/product/microsoft-visual-bas/60) and download the ISO file for the desired edition (e.g., Enterprise Edition).

2. **Mount or Extract the ISO**  
   - Use a tool like 7-Zip/WinRAR to extract the files.
   - On Windows 10/11, right-click the ISO and select "Mount".
3. **Run the Setup**  
   - Open the mounted/extracted folder and run `SETUP.EXE` as Administrator.
   - If you encounter compatibility issues, right-click `SETUP.EXE` → Properties → Compatibility, and set to "Windows XP (Service Pack 3)" or "Windows 98/Me".

4. **Follow the Installation Wizard**  
   - Enter the product key
   > **Note:**  
   > After extracting the ISO, you will find a file named `Visual Basic 6.0 Enterprise Edition - serial.txt` in the extracted folder.  
   > This file contains the product key (serial number) required during installation.  
   > Open this file with a text editor and copy the product key when prompted by the setup wizard.
   - Choose "Typical" or "Custom" install (default options are fine).
   - Complete the installation.

5. **First Launch**  
   - Start Visual Basic 6.0 from the Start Menu.
   - If prompted, allow any additional setup or registration steps.


### Setup Instructions

1. **Open demo.bas with Visual Basic 6.0**
   ```
   Open Main.bas with Visual Basic 6.0
   ```

2. **Add Required Modules and References**
   ```
   Project → Add File → Add the following .bas files:
   - HippoBackend.bas
   - FileLib.bas
   - S3UploadLib.bas  
   - JsonConverter.bas
   ```

3. **Add Required References**
   ```
   Project → References → Add:
   - Microsoft WinHTTP Services 5.1
   - Microsoft Scripting Runtime
   ```

4. **Copy DLL Files**
   ```
   Copy all .dll files to:
   - Project directory, OR
   - System PATH directory
   ```

5. **Configure Constants**
   ```
   Edit Main.bas and HippoBackend.bas constants sections:
   - Main.bas: S3_BUCKET, S3_REGION
   - HippoBackend.bas: ENV_URL, LOGIN_ACCOUNT, LOGIN_ACCOUNT_PASSWORD
   ```

6. **Run Application**
   ```
   Press F5 or Run → Start
   ```

## 🔧 Configuration

### API Configuration (HippoBackend.bas)
```vb
Private Const ENV_URL As String = "https://dev.hippoclinic.com"
Private Const LOGIN_ACCOUNT As String = "your-email@example.com"
Private Const LOGIN_ACCOUNT_PASSWORD As String = "your-password"
```

### S3 Configuration (Main.bas)
```vb
Private Const S3_BUCKET As String = "hippoclinic"
Private Const S3_REGION As String = "us-west-1"
```

## 📋 Usage

### Single File Upload
1. Run the application
2. Enter file path when prompted
3. System will:
   - Validate file existence
   - Authenticate with API
   - Create patient record
   - Get S3 credentials
   - Upload file to S3
   - Confirm upload with API

### Folder Upload
1. Run the application
2. Enter folder path when prompted
3. System will:
   - Process all files in folder
   - Upload each file individually
   - Provide batch upload summary
   - Confirm entire batch with API

## 🐛 Debugging

### Debug Output
All operations are logged to the VB6 Immediate Window:
- `ERROR:` - Error conditions
- `SUCCESS:` - Successful operations
- `WARNING:` - Warning conditions
- `SUMMARY:` - Batch operation summaries

### Error Codes
S3UploadLib provides standardized error codes:
- `0` - Success
- `-1` - Invalid parameters
- `-2` - AWS SDK not initialized
- `-3` - File does not exist
- `-4` - File read error
- `-5` - File open error
- `-6` - S3 upload failed
- `-7` - Exception occurred
- `-8` - Unknown error

## 🔒 Security Features

- **Temporary Credentials** - Uses AWS STS for secure S3 access
- **JWT Authentication** - Secure API authentication
- **Error Handling** - No sensitive data in error messages
- **Input Validation** - Comprehensive file and path validation

## 📊 Supported File Types

- **Any file type** - No restrictions on file extensions
- **Large files** - Supports files of any size
- **Batch uploads** - Multiple files in single operation
- **Mixed content** - Files and folders can be mixed

## 🛠️ Troubleshooting

### Common Issues

1. **DLL Not Found**
   - Ensure all .dll files are in the same directory as the .bas files
   - Ensure C++ runtimes are installed
   - Do not create a new project in VB6, open Main.bas directly
   - Check Windows PATH environment variable as fallback

   **How to Add DLL Directory to Windows PATH(Not required if you install the runtimes)**
   
   **Method 1: Using System Properties (GUI)**
   1. Press `Windows + R` to open Run dialog
   2. Type `sysdm.cpl` and press Enter
   3. Click "Advanced" → "Environment Variables..." button
   4. In "System variables" section, find and select "Path"
   5. Click "Edit..." → "New" → Add your DLL directory path
   6. Click "OK" to save changes
   
   **Method 2: Using Command Prompt**
   ```cmd
   # Check current PATH
   echo %PATH%
   
   # Add directory temporarily
   set PATH=%PATH%;C:\path\to\your\dll\files
   
   # Add directory permanently
   setx PATH "%PATH%;C:\path\to\your\dll\files"
   ```
   
   **Method 3: Using PowerShell**
   ```powershell
   # Check current PATH
   $env:PATH
   
   # Add directory temporarily
   $env:PATH += ";C:\path\to\your\dll\files"
   
   # Add directory permanently
   [Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\path\to\your\dll\files", "Machine")
   ```
   
   **Verification:**
   - Open new Command Prompt
   - Run `echo %PATH%` to confirm your DLL directory is listed
   - Test your VB6 application

2. **Authentication Failed**
   - Verify login credentials in HippoBackend.bas
   - Check network connectivity
   - Confirm API endpoint URL

3. **Upload Failed**
   - Check AWS S3 credentials
   - Verify S3 bucket permissions
   - Ensure file is not locked by another process

4. **JSON Parsing Errors**
   - Verify JsonConverter.bas is properly added
   - Check API response format

### Debug Steps
1. Enable VB6 Immediate Window (View → Immediate Window)
2. Run application and monitor debug output
3. Check error codes and messages
4. Verify file paths and permissions

## 📝 API Endpoints

The system integrates with these HippoClinic API endpoints:
- `POST /hippo/thirdParty/user/login` - User authentication
- `POST /hippo/thirdParty/queryOrCreatePatient` - Patient management
- `POST /hippo/thirdParty/file/getS3Credentials` - S3 credentials
- `GET /hippo/thirdParty/file/generateUniqueKey` - Data ID generation
- `POST /hippo/thirdParty/file/confirmUploadRawFile` - Upload confirmation

## 📄 License

- **JsonConverter.bas** - MIT License (VBA-JSON v2.3.1)
- **S3UploadLib** - Custom implementation
- **AWS SDK** - Apache License 2.0
- **Demo Application** - Internal use only

## 🤝 Support

For technical support or questions:
1. Check debug output in VB6 Immediate Window
2. Review error codes and messages
3. Verify configuration settings
4. Contact development team with specific error details

---

**Version:** 1.0  
**Last Updated:** December 2024  
**Compatibility:** VB6, Windows 7+  
**Dependencies:** AWS SDK, WinHTTP, Scripting Runtime
