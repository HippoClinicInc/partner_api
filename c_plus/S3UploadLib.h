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
    S3UPLOAD_API const char* __stdcall UploadFileSync(
        const char* accessKey,
        const char* secretKey,
        const char* sessionToken,
        const char* region,
        const char* bucketName,
        const char* objectKey,
        const char* localFilePath
    );
    
    // Start asynchronous upload to S3 with automatic retry mechanism
    // Features: Automatic retry up to 3 times on failure with exponential backoff
    // Parameters:
    //   accessKey: AWS Access Key ID
    //   secretKey: AWS Secret Access Key
    //   sessionToken: AWS Session Token (STS temporary credentials)
    //   region: AWS region
    //   bucketName: S3 bucket name
    //   objectKey: S3 object key (filename)
    //   localFilePath: local file path
    // Return value: JSON string with upload ID on success, error on failure
    S3UPLOAD_API const char* __stdcall UploadFileAsync(
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
    S3UPLOAD_API const char* __stdcall GetAsyncUploadStatus(
        const char* uploadId
    );
}

#endif // S3UPLOADLIB_H
