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
    S3UPLOAD_API const char* __stdcall InitializeAwsSDK();
    
    // Cleanup AWS SDK resources
    S3UPLOAD_API const char* __stdcall CleanupAwsSDK();
    
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
    S3UPLOAD_API const char* __stdcall UploadFile(
        const char* accessKey,
        const char* secretKey,
        const char* sessionToken,
        const char* region,
        const char* bucketName,
        const char* objectKey,
        const char* localFilePath
    );
    
    // Start asynchronous upload to S3
    // Parameters:
    //   accessKey: AWS Access Key ID
    //   secretKey: AWS Secret Access Key
    //   sessionToken: AWS Session Token (STS temporary credentials)
    //   region: AWS region
    //   bucketName: S3 bucket name
    //   objectKey: S3 object key (filename)
    //   localFilePath: local file path
    // Return value: Upload ID string on success, NULL on failure
    S3UPLOAD_API const char* __stdcall StartAsyncUpload(
        const char* accessKey,
        const char* secretKey,
        const char* sessionToken,
        const char* region,
        const char* bucketName,
        const char* objectKey,
        const char* localFilePath
    );
    
    // Get simple upload status
    // Parameters:
    //   uploadId: Upload ID returned by StartAsyncUpload
    // Return value: JSON string with upload status information
    S3UPLOAD_API const char* __stdcall GetSimpleUploadStatus(
        const char* uploadId
    );
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
