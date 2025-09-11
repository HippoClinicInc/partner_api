#ifndef S3UPLOADLIB_H
#define S3UPLOADLIB_H

// C++11 standard library headers (must be included before AWS SDK)
#include <mutex>        // For std::once_flag, std::call_once
#include <atomic>       // For std::atomic operations
#include <thread>       // For std::thread support
#include <memory>       // For std::shared_ptr, etc.
#include <vector>       // For std::vector
#include <string>       // For std::string

#include <windows.h>  // For Windows API types

#ifdef S3UPLOAD_EXPORTS
#define S3UPLOAD_API __declspec(dllexport)
#else
#define S3UPLOAD_API __declspec(dllimport)
#endif

// Multipart Upload
struct MultipartConfig {
    const char* accessKey;
    const char* secretKey;
    const char* sessionToken;
    const char* region;
    const char* bucketName;
    const char* objectKey;
    const char* localFilePath;

    // Multipart configuration
    long partSize;              // part size in bytes, minimum 5MB, default 20MB
    int maxRetries;             // maximum retry count per part, default 3

    void* userData;

    // resume configuration (deprecated, kept for compatibility)
    const char* resumeFilePath; // resume file path, NULL to disable resume
};

// VB6 compatible function declarations - using __stdcall calling convention
extern "C" {
    
    // Initialize AWS SDK
    // Return value: JSON response string
    S3UPLOAD_API const char* __stdcall InitializeAwsSDK();
    
    // Cleanup AWS SDK resources
    // Return value: JSON response string
    S3UPLOAD_API const char* __stdcall CleanupAwsSDK();
    
    // Simplified version: Multipart upload file to S3 (basic parameters)
    // Parameters:
    //   accessKey: AWS Access Key ID
    //   secretKey: AWS Secret Access Key
    //   sessionToken: AWS Session Token (optional, NULL for permanent credentials)
    //   region: AWS region
    //   bucketName: S3 bucket name
    //   objectKey: S3 object key (filename)
    //   localFilePath: local file path
    //   retryCount: maximum retry count per part
    // Return value: JSON response string
    S3UPLOAD_API const char* __stdcall UploadFile(
        const char* accessKey,
        const char* secretKey,
        const char* sessionToken,
        const char* region,
        const char* bucketName,
        const char* objectKey,
        const char* localFilePath,
        int retryCount
    );
}

#define AWS_INITIALIZE_SUCCESS        0   // success
#define AWS_INITIALIZE_FAILED        -1   // success

#define AWS_CLEANUP_SUCCESS        0   // success
#define AWS_CLEANUP_FAILED        -1   // success

// error code definitions
#define S3_SUCCESS                    0   // success
#define S3_ERROR_INVALID_PARAMS      -1   // invalid parameters
#define S3_ERROR_NOT_INITIALIZED     -2   // SDK not initialized
#define S3_ERROR_FILE_NOT_EXISTS     -3   // file not exists
#define S3_ERROR_FILE_READ_ERROR     -4   // file read error
#define S3_ERROR_FILE_OPEN_ERROR     -5   // file open error
#define S3_ERROR_S3_UPLOAD_FAILED    -6   // S3 upload failed
#define S3_ERROR_EXCEPTION           -7   // exception error
#define S3_ERROR_UNKNOWN             -8   // unknown error
#define S3_ERROR_PART_TOO_SMALL      -9   // part too small (minimum 5MB)
#define S3_ERROR_UPLOAD_NOT_FOUND    -10  // upload not found
#define S3_ERROR_RESUME_FAILED       -11  // resume failed
#define S3_ERROR_BUFFER_TOO_SMALL    -12  // buffer too small
#define S3_ERROR_CONCURRENT_LIMIT    -13  // concurrent limit error
#define S3_ERROR_RETRY_LIMIT_EXCEEDED -14 // retry limit exceeded


// constant definitions
#define S3_DEFAULT_PART_SIZE         (20 * 1024 * 1024)    // 20MB - S3 default part size
#define S3_MAX_PARTS                 10000                // S3 maximum parts
#define S3_DEFAULT_MAX_RETRIES       3                    // S3 default maximum retries

#endif
