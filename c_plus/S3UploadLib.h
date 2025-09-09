#ifndef S3UPLOADLIB_H
#define S3UPLOADLIB_H

// C++11 standard library headers (must be included before AWS SDK)
#include <mutex>        // For std::once_flag, std::call_once
#include <atomic>       // For std::atomic operations
#include <thread>       // For std::thread support
#include <memory>       // For std::shared_ptr, etc.

#include <windows.h>  // For Windows API types

// DLL export macro definition
#ifdef S3UPLOAD_EXPORTS
#define S3UPLOAD_API __declspec(dllexport)
#else
#define S3UPLOAD_API __declspec(dllimport)
#endif

// VB6 compatible function declarations - using __stdcall calling convention
extern "C" {
    
    // Initialize AWS SDK
    // Return value: 0=success, -1=failure
    S3UPLOAD_API int __stdcall InitializeAwsSDK();
    
    // Cleanup AWS SDK resources
    S3UPLOAD_API void __stdcall CleanupAwsSDK();
    
    // Get last error message
    // Return value: error message string pointer
    S3UPLOAD_API const char* __stdcall GetS3LastError();
    
    // Upload file to S3 (using permanent credentials)
    // Parameters:
    //   accessKey: AWS Access Key ID
    //   secretKey: AWS Secret Access Key  
    //   region: AWS region (e.g. "us-east-1")
    //   bucketName: S3 bucket name
    //   objectKey: S3 object key (filename)
    //   localFilePath: local file path
    // Return value: 0=success, negative=various error codes
    S3UPLOAD_API int __stdcall UploadFileToS3(
        const char* accessKey,
        const char* secretKey,
        const char* region,
        const char* bucketName,
        const char* objectKey,
        const char* localFilePath
    );
    
    // Upload file to S3 (using temporary credentials/STS Token)
    // Parameters:
    //   accessKey: AWS Access Key ID
    //   secretKey: AWS Secret Access Key
    //   sessionToken: AWS Session Token (STS temporary credentials)
    //   region: AWS region
    //   bucketName: S3 bucket name
    //   objectKey: S3 object key (filename)
    //   localFilePath: local file path
    // Return value: 0=success, negative=various error codes
    S3UPLOAD_API int __stdcall UploadFileToS3WithToken(
        const char* accessKey,
        const char* secretKey,
        const char* sessionToken,
        const char* region,
        const char* bucketName,
        const char* objectKey,
        const char* localFilePath
    );
    
    // Simplified version: upload file using JSON configuration
    // Parameters:
    //   configJson: JSON format configuration string
    //   localFilePath: local file path
    // JSON format example:
    // {
    //   "accessKey": "your_access_key",
    //   "secretKey": "your_secret_key", 
    //   "sessionToken": "optional_session_token",
    //   "region": "us-east-1",
    //   "bucket": "your-bucket-name",
    //   "objectKey": "optional/path/filename.txt"
    // }
    // Return value: 0=success, negative=various error codes
    S3UPLOAD_API int __stdcall UploadFileToS3Simple(
        const char* configJson,
        const char* localFilePath
    );
    
    // Helper function: check if file exists
    // Return value: 1=exists, 0=does not exist
    S3UPLOAD_API int __stdcall FileExists(const char* filePath);
    
    // Helper function: get file size
    // Return value: file size (bytes), -1=error
    S3UPLOAD_API long __stdcall GetS3FileSize(const char* filePath);
    
    // Test function: verify DLL is working properly
    // Return value: 100=normal
    S3UPLOAD_API int __stdcall TestS3Library();
}

// Error code definitions (for reference)
#define S3_SUCCESS                  0   // Success
#define S3_ERROR_INVALID_PARAMS    -1   // Invalid parameters
#define S3_ERROR_NOT_INITIALIZED   -2   // SDK not initialized
#define S3_ERROR_FILE_NOT_EXISTS   -3   // File does not exist
#define S3_ERROR_FILE_READ_ERROR   -4   // File read error
#define S3_ERROR_FILE_OPEN_ERROR   -5   // File open error
#define S3_ERROR_S3_UPLOAD_FAILED  -6   // S3 upload failed
#define S3_ERROR_EXCEPTION         -7   // Exception error
#define S3_ERROR_UNKNOWN           -8   // Unknown error

#endif // S3UPLOADLIB_H
