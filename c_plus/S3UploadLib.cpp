#include "S3UploadLib.h"
#include <intrin.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <chrono>

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

// Global variables
static bool g_isInitialized = false;
static Aws::SDKOptions g_options;

// Async upload retry configuration
// Maximum number of retry attempts for failed uploads
static const int MAX_UPLOAD_RETRIES = 3;


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
    UPLOAD_CANCELLED = 4     
};

// Error message constants
namespace ErrorMessage {
    const std::string INVALID_PARAMETERS = "Invalid parameters: one or more required parameters are null";
    const std::string SDK_NOT_INITIALIZED = "AWS SDK not initialized. Call InitializeAwsSDK() first";
    const std::string LOCAL_FILE_NOT_EXIST = "Local file does not exist";
    const std::string CANNOT_READ_FILE_SIZE = "Cannot read file size";
    const std::string CANNOT_OPEN_FILE = "Cannot open file for reading";
    const std::string UPLOAD_EXCEPTION = "Upload failed with exception";
    const std::string UNKNOWN_ERROR = "Unknown error";
}

// Format error message
std::string formatErrorMessage(const std::string& baseMessage, const std::string& detail = "") {
    if (detail.empty()) {
        return baseMessage;
    }
    return baseMessage + ": " + detail;
}

std::string create_response(int code, const std::string& message) {
    std::ostringstream oss;
    oss << "{"
        << "\"code\":" << code << ","
        << "\"message\":\"" << message << "\""
        << "}";
    return oss.str();
}

// Initialize AWS SDK
extern "C" S3UPLOAD_API const char* __stdcall InitializeAwsSDK() {
    if (g_isInitialized) {
        static std::string response = create_response(UPLOAD_FAILED, "AWS SDK already initialized");
        return response.c_str();
    }

    try {
        // Set log level (can be adjusted as needed)
        g_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;

        // Initialize AWS SDK
        Aws::InitAPI(g_options);
        g_isInitialized = true;

        static std::string response = create_response(UPLOAD_SUCCESS, "AWS SDK initialized successfully");
        return response.c_str();
    }
    catch (const std::exception& e) {
        static std::string response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to initialize AWS SDK", e.what()));
        return response.c_str();
    }
    catch (...) {
        static std::string response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to initialize AWS SDK", "Unknown error"));
        return response.c_str();
    }
}

// Cleanup AWS SDK
extern "C" S3UPLOAD_API const char* __stdcall CleanupAwsSDK() {
    if (g_isInitialized) {
        try {
            Aws::ShutdownAPI(g_options);
            g_isInitialized = false;
            static std::string successResponse = create_response(UPLOAD_SUCCESS, "AWS SDK cleaned up successfully");
            return successResponse.c_str();
        }
        catch (...) {
            static std::string unknownError = create_response(UPLOAD_FAILED, formatErrorMessage("Error during AWS SDK cleanup"));
            return unknownError.c_str();
        }
    } else {
        static std::string notInitialized = create_response(UPLOAD_FAILED, "AWS SDK was not initialized");
        return notInitialized.c_str();
    }
}

// Check if file exists
extern "C" S3UPLOAD_API int __stdcall FileExists(const char* filePath) {
    if (!filePath) return 0;
    
    std::ifstream file(filePath);
    return file.good() ? 1 : 0;
}

// Get file size
extern "C" S3UPLOAD_API long __stdcall GetS3FileSize(const char* filePath) {
    if (!filePath) return -1;
    
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return -1;
    }
    return static_cast<long>(file.tellg());
}

// S3 upload implementation with Session Token support
extern "C" S3UPLOAD_API const char* __stdcall UploadFileSync(
    const char* accessKey,
    const char* secretKey,
    const char* sessionToken,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath
) {
     static std::string response;

    // Parameter validation
     if (!accessKey || !secretKey || !region || !bucketName || !objectKey || !localFilePath) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::INVALID_PARAMETERS));
        return response.c_str();
    }

    if (!g_isInitialized) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::SDK_NOT_INITIALIZED));
        return response.c_str();
    }

    // Check if file exists
    if (!FileExists(localFilePath)) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::LOCAL_FILE_NOT_EXIST, localFilePath));
        return response.c_str();
    }

    // Get file size
    long fileSize = GetS3FileSize(localFilePath);
    if (fileSize < 0) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::CANNOT_READ_FILE_SIZE, localFilePath));
        return response.c_str();
    }

    try {
        AWS_LOGSTREAM_INFO("S3Upload", "=== Starting UploadFileToS3WithToken ===");
        AWS_LOGSTREAM_INFO("S3Upload", "AccessKey (FULL): " << std::string(accessKey));
        AWS_LOGSTREAM_INFO("S3Upload", "SecretKey (FULL): " << std::string(secretKey));
        AWS_LOGSTREAM_INFO("S3Upload", "SessionToken (FULL): " << (sessionToken ? std::string(sessionToken) : "NULL"));
        AWS_LOGSTREAM_INFO("S3Upload", "Region: " << region);
        AWS_LOGSTREAM_INFO("S3Upload", "Bucket: " << bucketName);
        AWS_LOGSTREAM_INFO("S3Upload", "Object: " << objectKey);
        AWS_LOGSTREAM_INFO("S3Upload", "File: " << localFilePath);
        AWS_LOGSTREAM_INFO("S3Upload", "SessionToken length: " << (sessionToken ? strlen(sessionToken) : 0));
        
        // Configure client
        AWS_LOGSTREAM_INFO("S3Upload", "Creating S3 client configuration...");
        Aws::S3::S3ClientConfiguration clientConfig;
        clientConfig.region = region;
        // 30 seconds timeout
        clientConfig.requestTimeoutMs = 30000;  
        // 10 seconds connect timeout
        clientConfig.connectTimeoutMs = 10000;  

        // Create AWS credentials (with Session Token)
        AWS_LOGSTREAM_INFO("S3Upload", "Creating AWS credentials...");
        Aws::Auth::AWSCredentials credentials;
        if (sessionToken && strlen(sessionToken) > 0) {
            // Use temporary credentials (STS)
            AWS_LOGSTREAM_INFO("S3Upload", "Using temporary credentials with session token");
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey, sessionToken);
        } else {
            // Use permanent credentials
            AWS_LOGSTREAM_INFO("S3Upload", "Using permanent credentials");
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey);
        }

        // Create credentials provider
        AWS_LOGSTREAM_INFO("S3Upload", "Creating credentials provider...");
        auto credentialsProvider = Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>("S3Upload", credentials);
        
        // Create S3 client - using credentials provider constructor
        AWS_LOGSTREAM_INFO("S3Upload", "Creating S3 client...");
        Aws::S3::S3Client s3Client(credentialsProvider, nullptr, clientConfig);

        // Create upload request
        AWS_LOGSTREAM_INFO("S3Upload", "Creating PutObject request...");
        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bucketName);
        request.SetKey(objectKey);

        // Open file stream
        AWS_LOGSTREAM_INFO("S3Upload", "Opening file stream for: " << localFilePath);
        auto inputData = Aws::MakeShared<Aws::FStream>("PutObjectInputStream",
                                                       localFilePath,
                                                       std::ios_base::in | std::ios_base::binary);

        if (!inputData->is_open()) {
            AWS_LOGSTREAM_ERROR("S3Upload", "Failed to open file: " << localFilePath);
            response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::CANNOT_OPEN_FILE, localFilePath));
            return response.c_str();
        }

        AWS_LOGSTREAM_INFO("S3Upload", "File opened successfully, setting request body...");
        request.SetBody(inputData);
        request.SetContentType("application/octet-stream");

        // Execute upload
        AWS_LOGSTREAM_INFO("S3Upload", "Starting S3 PutObject operation...");
        AWS_LOGSTREAM_INFO("S3Upload", "File size: " << fileSize << " bytes");
        AWS_LOGSTREAM_INFO("S3Upload", "This may take a while depending on file size and network...");
        
        auto outcome = s3Client.PutObject(request);
        
        AWS_LOGSTREAM_INFO("S3Upload", "PutObject operation completed");

        if (outcome.IsSuccess()) {
            std::ostringstream oss;
            oss << "Successfully uploaded " << localFilePath
                << " (" << fileSize << " bytes) to s3://"
                << bucketName << "/" << objectKey
                << " in region " << region;

            if (sessionToken && strlen(sessionToken) > 0) {
                oss << " using STS credentials";
            }

            AWS_LOGSTREAM_INFO("S3Upload", "Upload SUCCESS: " << oss.str());
            response = create_response(UPLOAD_SUCCESS, oss.str());
            return response.c_str();
        } else {
            auto error = outcome.GetError();
            std::ostringstream oss;
            oss << "S3 upload failed: " << error.GetMessage().c_str()
                << " (Error Code: " << static_cast<int>(error.GetErrorType()) << ")";

            AWS_LOGSTREAM_ERROR("S3Upload", "Upload FAILED: " << oss.str());
            AWS_LOGSTREAM_ERROR("S3Upload", "Error type: " << error.GetExceptionName());
            response = create_response(UPLOAD_FAILED, oss.str());
            return response.c_str();
        }

    } catch (const std::exception& e) {
        AWS_LOGSTREAM_ERROR("S3Upload", "Exception caught in UploadFileToS3WithToken: " << e.what());
      response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::UPLOAD_EXCEPTION, e.what()));
        return response.c_str();
    } catch (...) {
        AWS_LOGSTREAM_ERROR("S3Upload", "Unknown exception caught in UploadFileToS3WithToken");
        response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::UNKNOWN_ERROR));
        return response.c_str();
    }
}

// Async upload progress information structure
// Contains all tracking data for a single upload operation
struct AsyncUploadProgress {
    // Unique identifier for this upload
    std::string uploadId;           
    // Current status of the upload
    UploadStatus status;           
    // Total size of file being uploaded (in bytes)
    long long totalSize;           
    // Error message if upload failed
    std::string errorMessage;      
    // Local file path
    std::string localFilePath;   
    // s3 object key
    std::string s3ObjectKey;   
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
    // Mutex for thread-safe operations
    std::mutex mutex_; 
    // Map of upload ID to progress info
    std::unordered_map<std::string, std::shared_ptr<AsyncUploadProgress>> uploads_;  // Map of upload ID to progress info

public:
    // Get singleton instance of the manager
    static AsyncUploadManager& getInstance() {
        static AsyncUploadManager instance;
        return instance;
    }

    // Add a new upload to tracking system
    // Returns the upload ID for reference
    std::string addUpload(const std::string& uploadId, const std::string& localFilePath, const std::string& s3ObjectKey) {
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
    std::shared_ptr<AsyncUploadProgress> getUpload(const std::string& uploadId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = uploads_.find(uploadId);
        return it != uploads_.end() ? it->second : nullptr;
    }

    // Remove upload from tracking system (cleanup)
    void removeUpload(const std::string& uploadId) {
        std::lock_guard<std::mutex> lock(mutex_);
        uploads_.erase(uploadId);
    }

    // Update upload status and error message
    // Thread-safe status updates for progress tracking
    void updateProgress(const std::string& uploadId, UploadStatus status,
                       const std::string& error = "") {
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

// Async upload worker thread function
// This function runs in a separate thread to handle file upload to S3
void asyncUploadWorker(const std::string& uploadId,
                      const std::string& accessKey,
                      const std::string& secretKey,
                      const std::string& sessionToken,
                      const std::string& region,
                      const std::string& bucketName,
                      const std::string& objectKey,
                      const std::string& localFilePath) {

    // Step 1: Get upload progress tracker from manager
    auto& manager = AsyncUploadManager::getInstance();
    auto progress = manager.getUpload(uploadId);

    if (!progress) return;

    try {
        // Step 2: Initialize upload progress and set status to uploading
        progress->startTime = std::chrono::steady_clock::now();
        manager.updateProgress(uploadId, UPLOAD_UPLOADING);

        AWS_LOGSTREAM_INFO("S3Upload", "=== Starting Async Upload ===");
        AWS_LOGSTREAM_INFO("S3Upload", "Upload ID: " << uploadId);
        AWS_LOGSTREAM_INFO("S3Upload", "File: " << localFilePath);

        // Step 3: Check for cancellation before starting
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, UPLOAD_CANCELLED);
            return;
        }

        // Step 4: Validate input parameters
        if (accessKey.empty() || secretKey.empty() || region.empty() ||
            bucketName.empty() || objectKey.empty() || localFilePath.empty()) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, "Invalid parameters");
            return;
        }

        // Step 5: Verify AWS SDK is initialized
        if (!g_isInitialized) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, formatErrorMessage(ErrorMessage::SDK_NOT_INITIALIZED));
            return;
        }

        // Step 6: Check if local file exists
        if (!FileExists(localFilePath.c_str())) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, formatErrorMessage(ErrorMessage::LOCAL_FILE_NOT_EXIST, localFilePath));
            return;
        }

        // Step 7: Get file size and validate
        long fileSize = GetS3FileSize(localFilePath.c_str());
        if (fileSize < 0) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, formatErrorMessage(ErrorMessage::CANNOT_READ_FILE_SIZE, localFilePath));
            return;
        }

        progress->totalSize = fileSize;
        AWS_LOGSTREAM_INFO("S3Upload", "File size: " << fileSize << " bytes");

        // Step 8: Check for cancellation again before heavy operations
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, UPLOAD_CANCELLED);
            return;
        }

        // Step 9: Configure S3 client settings
        Aws::S3::S3ClientConfiguration clientConfig;
        clientConfig.region = region;
        // 30 seconds timeout
        clientConfig.requestTimeoutMs = 30000;  
        // 10 seconds connect timeout
        clientConfig.connectTimeoutMs = 10000;

        // Step 10: Create AWS credentials (support both permanent and temporary STS tokens)
        Aws::Auth::AWSCredentials credentials;
        if (!sessionToken.empty()) {
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey, sessionToken);
            AWS_LOGSTREAM_INFO("S3Upload", "Using temporary credentials with session token");
        } else {
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey);
            AWS_LOGSTREAM_INFO("S3Upload", "Using permanent credentials");
        }

        // Step 11: Create credentials provider and S3 client
        auto credentialsProvider = Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>("S3Upload", credentials);
        Aws::S3::S3Client s3Client(credentialsProvider, nullptr, clientConfig);

        // Step 12: Create S3 PutObject request
        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bucketName);
        request.SetKey(objectKey);

        // Step 13: Final cancellation check before upload
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, UPLOAD_CANCELLED);
            return;
        }

        // Step 14: Open file stream for reading
        auto inputData = Aws::MakeShared<Aws::FStream>("PutObjectInputStream",
                                                       localFilePath.c_str(),
                                                       std::ios_base::in | std::ios_base::binary);

        if (!inputData->is_open()) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, formatErrorMessage(ErrorMessage::CANNOT_OPEN_FILE, localFilePath));
            return;
        }

        // Step 15: Set request body and content type
        request.SetBody(inputData);
        request.SetContentType("application/octet-stream");

        AWS_LOGSTREAM_INFO("S3Upload", "Starting S3 PutObject operation...");

        // Step 16: Execute S3 upload with retry mechanism (up to 3 retries on failure)
        bool uploadSuccess = false;
        std::string finalErrorMsg = "";
        
        // Retry loop: attempt upload up to MAX_UPLOAD_RETRIES + 1 times (initial + 3 retries)
        for (int retryCount = 0; retryCount <= MAX_UPLOAD_RETRIES; retryCount++) {
            // Check for cancellation before each retry attempt
            if (progress->shouldCancel.load()) {
                manager.updateProgress(uploadId, UPLOAD_CANCELLED);
                return;
            }
            
            // Apply exponential backoff delay for retry attempts (2, 4, 6 seconds)
            if (retryCount > 0) {
                AWS_LOGSTREAM_INFO("S3Upload", "Retry attempt " << retryCount << " for upload ID: " << uploadId);
                std::this_thread::sleep_for(std::chrono::seconds(retryCount * 2));
            }
            
            // Execute the actual S3 upload operation
            auto outcome = s3Client.PutObject(request);
            
            if (outcome.IsSuccess()) {
                // Upload succeeded - exit retry loop
                uploadSuccess = true;
                AWS_LOGSTREAM_INFO("S3Upload", "Async upload SUCCESS for ID: " << uploadId << " (attempt " << (retryCount + 1) << ")");
                break;
            } else {
                // Upload failed - log error and prepare for potential retry
                auto error = outcome.GetError();
                finalErrorMsg = "S3 upload failed (attempt " + std::to_string(retryCount + 1) + "): " + std::string(error.GetMessage().c_str());
                AWS_LOGSTREAM_WARN("S3Upload", "Upload attempt " << (retryCount + 1) << " failed for ID: " << uploadId << " - " << finalErrorMsg);
                
                // If this is the last attempt, exit retry loop
                if (retryCount == MAX_UPLOAD_RETRIES) {
                    break;
                }
            }
        }

        AWS_LOGSTREAM_INFO("S3Upload", "PutObject operation completed");

        // Step 17: Handle final upload result
        if (uploadSuccess) {
            progress->endTime = std::chrono::steady_clock::now();
            manager.updateProgress(uploadId, UPLOAD_SUCCESS);
            AWS_LOGSTREAM_INFO("S3Upload", "Async upload SUCCESS for ID: " << uploadId);
        } else {
            manager.updateProgress(uploadId, UPLOAD_FAILED, finalErrorMsg);
            AWS_LOGSTREAM_ERROR("S3Upload", "Async upload FAILED for ID: " << uploadId << " after " << (MAX_UPLOAD_RETRIES + 1) << " attempts - " << finalErrorMsg);
        }

    } catch (const std::exception& e) {
        // Step 18: Handle exceptions during upload
        std::string errorMsg = formatErrorMessage(ErrorMessage::UPLOAD_EXCEPTION, e.what());
        manager.updateProgress(uploadId, UPLOAD_FAILED, errorMsg);
        AWS_LOGSTREAM_ERROR("S3Upload", "Exception in async upload: " << e.what());
    } catch (...) {
        // Step 19: Handle unknown exceptions
        manager.updateProgress(uploadId, UPLOAD_FAILED, formatErrorMessage(ErrorMessage::UNKNOWN_ERROR));
        AWS_LOGSTREAM_ERROR("S3Upload", "Unknown exception in async upload");
    }
}

// Exported async upload function - starts file upload in background thread
// Returns JSON with upload ID on success, error message on failure
extern "C" S3UPLOAD_API const char* __stdcall UploadFileAsync(
    const char* accessKey,
    const char* secretKey,
    const char* sessionToken,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath
) {
    static std::string response;

    // Step 1: Validate input parameters
    if (!accessKey || !secretKey || !region || !bucketName || !objectKey || !localFilePath) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::INVALID_PARAMETERS));
        return response.c_str();
    }

    // Step 2: Check if AWS SDK is initialized
    if (!g_isInitialized) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::SDK_NOT_INITIALIZED));
        return response.c_str();
    }

    try {
        // Step 3: Generate unique upload ID using current timestamp
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        std::string uploadId = "async_upload_" + std::to_string(timestamp);

        // Step 4: Register upload with manager for progress tracking
        AsyncUploadManager::getInstance().addUpload(uploadId, localFilePath, objectKey);

        // Step 5: Convert C-style parameters to C++ strings (avoid pointer lifetime issues)
        std::string strAccessKey = accessKey;
        std::string strSecretKey = secretKey;
        std::string strSessionToken = sessionToken ? sessionToken : "";
        std::string strRegion = region;
        std::string strBucketName = bucketName;
        std::string strObjectKey = objectKey;
        std::string strLocalFilePath = localFilePath;

        // Step 6: Start background thread for async upload
        std::thread uploadThread(asyncUploadWorker, uploadId,
                               strAccessKey, strSecretKey, strSessionToken,
                               strRegion, strBucketName, strObjectKey, strLocalFilePath);
        // Detach thread so it runs independently
        uploadThread.detach(); 

        // Step 7: Return success response with upload ID
        response = create_response(UPLOAD_SUCCESS, uploadId);
        return response.c_str();

    } catch (const std::exception& e) {
        // Step 8: Handle exceptions during thread creation
        response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to start async upload", e.what()));
        return response.c_str();
    } catch (...) {
        // Step 9: Handle unknown exceptions
        response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to start async upload", ErrorMessage::UNKNOWN_ERROR));
        return response.c_str();
    }
}

// Get async upload status function - queries current status of an upload
// Returns JSON with upload status, progress info, and error messages
extern "C" S3UPLOAD_API const char* __stdcall GetAsyncUploadStatus(const char* uploadId) {
    static std::string response;

    // Step 1: Validate upload ID parameter
    if (!uploadId) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage("Upload ID is null"));
        return response.c_str();
    }

    // Step 2: Look up upload progress in manager
    auto progress = AsyncUploadManager::getInstance().getUpload(uploadId);
    if (!progress) {
        response = create_response(UPLOAD_FAILED, formatErrorMessage("Upload ID not found"));
        return response.c_str();
    }

    try {
        // Step 3: Build JSON response with upload information
        std::ostringstream oss;
        oss << "{"
            << "\"code\":" << UPLOAD_SUCCESS << ","
            << "\"uploadId\":\"" << progress->uploadId << "\","
            << "\"status\":" << progress->status << ","
            << "\"totalSize\":" << progress->totalSize << ","
            << "\"errorMessage\":\"" << progress->errorMessage << "\"";

        // Step 4: Calculate duration if upload is completed (success or failed)
        if (progress->status == UPLOAD_SUCCESS || progress->status == UPLOAD_FAILED) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                progress->endTime - progress->startTime);
            oss << ",\"durationSeconds\":" << duration.count();
        }

        oss << "}";

        // Step 5: Return status information as JSON string
        return oss.str().c_str();

    } catch (const std::exception& e) {
        // Step 6: Handle exceptions during status query
        response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to get upload status", e.what()));
        return response.c_str();
    } catch (...) {
        // Step 7: Handle unknown exceptions
        response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to get upload status", ErrorMessage::UNKNOWN_ERROR));
        return response.c_str();
    }
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        // When DLL is unloaded, auto cleanup
        if (g_isInitialized) {
            CleanupAwsSDK();
        }
        break;
    }
    return TRUE;
}

