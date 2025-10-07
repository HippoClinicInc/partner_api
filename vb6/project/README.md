# HippoClinic File Upload VB6 Project

## Project Structure

```
test1/
├── src/                    # Source code directory
│   ├── forms/             # Form files
│   │   └── Form1.frm     # Main form
│   └── modules/           # Module files
│       ├── Main.bas      # Main program logic
│       ├── JsonConverter.bas  # JSON processing
│       ├── S3UploadLib.bas    # S3 upload library declarations
│       ├── HippoBackend.bas   # Backend API calls
│       └── FileLib.bas        # File operation utilities
├── lib/                   # Library files directory
│   ├── *.dll             # AWS SDK DLL files
│   └── S3UploadLib.dll   # Custom S3 upload library
├── Project1.vbp         # VB6 project file
└── README.md            # Project documentation
```

## Module Description

### 1. Form Module (src/forms/)
- **Form1.frm**: Main upload button

### 2. Core Modules (src/modules/)
- **Main.bas**: Main program logic, handles file upload workflow
- **HippoBackend.bas**: HippoClinic backend API calls
  - User login authentication
  - Patient record creation
  - S3 credentials retrieval
  - Data ID generation
  - Upload confirmation
- **S3UploadLib.bas**: AWS S3 upload library VB6 declarations
- **DllPathManager.bas**: DLL path management module
  - Dynamic DLL path resolution
  - DLL loading and unloading management
  - Path validation and error handling
- **FileLib.bas**: File system operation utilities
- **JsonConverter.bas**: JSON data processing

### 3. Library Files (lib/)
- AWS SDK related DLL files
- Custom S3UploadLib.dll

## Usage Instructions

1. Open VB6 IDE
2. Load Project1.vbp project file
3. Ensure all reference libraries are properly configured:
   - Microsoft WinHTTP Services 5.1
   - Microsoft Scripting Runtime
4. Compile and run the project
5. Enter the file or folder path to upload in the interface
6. Click "Start Upload" button to begin upload

## Configuration

### API Configuration (HippoBackend.bas)
```vb
Private Const ENV_URL As String = "https://hippoclinic.com"
Private Const LOGIN_ACCOUNT As String = "your-email@example.com"
Private Const LOGIN_ACCOUNT_PASSWORD As String = "your-password"
```

### S3 Configuration (Main.bas)
```vb
Private Const S3_BUCKET As String = "hippoclinic-staging"
Private Const S3_REGION As String = "us-west-1"
```

## Dependencies

- VB6 Runtime
- Microsoft WinHTTP Services 5.1
- Microsoft Scripting Runtime
- AWS SDK DLL files (in lib directory)

## DLL Path Management

The project uses the `DllPathManager.bas` module to manage DLL file path issues:

### Problem Solution
- **Problem**: VB6 may not be able to correctly find DLL files in subdirectory structures
- **Solution**: Use Windows API to set DLL search path
- **Features**: 
  - Automatic project root directory detection
  - Use SetDllDirectory API to set search path
  - Validate DLL file existence
  - Provide detailed error information
  - DLL files remain in lib directory, no copying required

### DLL File Management
1. **DLL File Location**: All DLL files are stored in the `lib/` directory
2. **Search Path Setting**: Automatically set DLL search path when program starts
3. **No Copying Required**: DLL files remain in lib directory, maintaining clean project structure

```
lib/                    # DLL file directory (only location)
├── S3UploadLib.dll      # Custom S3 upload library
├── aws-c-*.dll         # AWS C SDK library
├── aws-cpp-sdk-*.dll   # AWS C++ SDK library
└── zlib1.dll           # Compression library
```

### Usage Instructions
1. Ensure all DLL files are in the `lib/` directory
2. Open and run the project in VB6
3. The program will automatically set DLL search path and validate file existence

## Important Notes

1. **DLL Files**: Ensure all DLL files are in lib directory and accessible
2. **API Configuration**: Modify API configuration information to match your environment
3. **Upload Support**: Project supports single file and folder uploads
4. **Error Handling**: Upload process includes progress monitoring and error handling
5. **Path Issues**: If encountering DLL loading problems, check if lib directory exists and contains all required DLL files
