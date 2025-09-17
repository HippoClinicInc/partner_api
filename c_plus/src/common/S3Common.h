#ifndef S3COMMON_H
#define S3COMMON_H

// C++11 standard library headers (must be included before AWS SDK)
// For std::once_flag, std::call_once
#include <mutex>
 // For std::atomic operations
#include <atomic>
// For std::thread support
#include <thread>
// For std::shared_ptr, etc.
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <unordered_map>
// For strlen
#include <cstring>
// For std::quoted
#include <iomanip>
// For Windows API types
#include <windows.h>
#include <regex>
// Type aliases for cleaner code
using String = std::string;

// MinGW compatibility fix: define missing byte order conversion functions
#ifdef __MINGW32__
// Define MinGW missing byte order conversion functions
// Use GCC built-in functions, no need to include intrin.h
inline unsigned long _byteswap_ulong(unsigned long x) {
    return __builtin_bswap32(x);
}

inline unsigned short _byteswap_ushort(unsigned short x) {
    return __builtin_bswap16(x);
}

inline unsigned __int64 _byteswap_uint64(unsigned __int64 x) {
    return __builtin_bswap64(x);
}
#endif

// AWS SDK headers
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>

// DLL export macro definition
#ifdef S3UPLOAD_EXPORTS
#define S3UPLOAD_API __declspec(dllexport)
#else
#define S3UPLOAD_API __declspec(dllimport)
#endif

// Async upload retry configuration
// Maximum number of retry attempts for failed uploads
static const int MAX_UPLOAD_RETRIES = 3;

// Error message constants
namespace ErrorMessage {
    const String INVALID_PARAMETERS = "Invalid parameters: one or more required parameters are null";
    const String SDK_NOT_INITIALIZED = "AWS SDK not initialized. Call InitializeAwsSDK() first";
    const String LOCAL_FILE_NOT_EXIST = "Local file does not exist";
    const String CANNOT_READ_FILE_SIZE = "Cannot read file size";
    const String CANNOT_OPEN_FILE = "Cannot open file for reading";
    const String UPLOAD_EXCEPTION = "Upload failed with exception";
    const String UNKNOWN_ERROR = "Unknown error";
}

// Format error message helper function
String formatErrorMessage(const String& baseMessage, const String& detail = "");

// Upload status enumeration - defines possible states of an async upload
enum UploadStatus {
    // Upload is waiting to start
    UPLOAD_PENDING = 0,      
    // Upload is currently in progress
    UPLOAD_UPLOADING = 1,    
    // Upload completed successfully
    UPLOAD_SUCCESS = 2,
    // Upload failed with error
    UPLOAD_FAILED = 3,
    // Upload was cancelled by user
    UPLOAD_CANCELLED = 4,
    // SDK resources were successfully initialized
    SDK_INIT_SUCCESS = 5,
    // SDK resources were successfully cleaned up
    SDK_CLEAN_SUCCESS = 6
};

// Async upload progress information structure
// Contains all tracking data for a single upload operation
struct AsyncUploadProgress {
    // Unique identifier for this upload
    String uploadId;
    // Current status of the upload
    UploadStatus status;
    // Total size of file being uploaded (in bytes)
    long long totalSize;
    // Error message if upload failed
    String errorMessage;
    // Local file path
    String s3ObjectKey;
    // Local file path
    String localFilePath;
    // When upload started
    std::chrono::steady_clock::time_point startTime;
    // When upload completed
    std::chrono::steady_clock::time_point endTime;
     // Atomic flag for cancellation requests
    std::atomic<bool> shouldCancel;

    // Constructor - initialize with default values
    AsyncUploadProgress() : status(UPLOAD_PENDING), totalSize(0), shouldCancel(false) {}
};

// Async upload manager class - thread-safe singleton for managing multiple uploads
// Provides centralized tracking and status management for concurrent file uploads
class AsyncUploadManager {
private:
    std::mutex mutex_;  // Mutex for thread-safe operations
    std::unordered_map<String, std::shared_ptr<AsyncUploadProgress>> uploads_;  // Map of upload ID to progress info

public:
    // Get singleton instance of the manager
    static AsyncUploadManager& getInstance() {
        static AsyncUploadManager instance;
        return instance;
    }

    // Add a new upload to tracking system
    // Returns the upload ID for reference
    String addUpload(const String& uploadId, const String& localFilePath, const String& s3ObjectKey) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto progress = std::make_shared<AsyncUploadProgress>();
        progress->uploadId = uploadId;
        progress->localFilePath = localFilePath;
        progress->s3ObjectKey = s3ObjectKey;
        uploads_[uploadId] = progress;
        return uploadId;
    }

    // Get upload progress information by ID
    // Returns shared_ptr to progress info or nullptr if not found
    std::shared_ptr<AsyncUploadProgress> getUpload(const String& uploadId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = uploads_.find(uploadId);
        return it != uploads_.end() ? it->second : nullptr;
    }

    // Remove upload from tracking system (cleanup)
    void removeUpload(const String& uploadId) {
        std::lock_guard<std::mutex> lock(mutex_);
        uploads_.erase(uploadId);
    }

    // Update upload status and error message
    // Thread-safe status updates for progress tracking
    void updateProgress(const String& uploadId, UploadStatus status,
                       const String& error = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = uploads_.find(uploadId);
        if (it != uploads_.end()) {
            it->second->status = status;
            if (!error.empty()) {
                it->second->errorMessage = error;
            }
        }
    }
};

// Global variables (extern declarations)
extern bool g_isInitialized;
extern Aws::SDKOptions g_options;

// Common utility functions
String create_response(int code, const String& message);

// AWS SDK management functions (extern "C" declarations)
extern "C" {
    S3UPLOAD_API int __stdcall FileExists(const char* filePath);
    S3UPLOAD_API long __stdcall GetS3FileSize(const char* filePath);
    S3UPLOAD_API const char* __stdcall InitializeAwsSDK();
    S3UPLOAD_API const char* __stdcall CleanupAwsSDK();
}

// S3 client creation helper
Aws::S3::S3Client createS3Client(const String& accessKey,
                                const String& secretKey,
                                const String& sessionToken,
                                const String& region);

// S3COMMON_H
#endif
