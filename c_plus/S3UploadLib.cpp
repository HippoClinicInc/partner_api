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
        static std::string response = create_response(-2, "AWS SDK already initialized");
        return response.c_str();
    }

    try {
        // Set log level (can be adjusted as needed)
        g_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;

        // Initialize AWS SDK
        Aws::InitAPI(g_options);
        g_isInitialized = true;

        static std::string response = create_response(0, "AWS SDK initialized successfully");
        return response.c_str();
    }
    catch (const std::exception& e) {
        static std::string response = create_response(-1, "Failed to initialize AWS SDK: " + std::string(e.what()));
        return response.c_str();
    }
    catch (...) {
        static std::string response = create_response(-1, "Failed to initialize AWS SDK: Unknown error");
        return response.c_str();
    }
}

// Cleanup AWS SDK
extern "C" S3UPLOAD_API const char* __stdcall CleanupAwsSDK() {
    if (g_isInitialized) {
        try {
            Aws::ShutdownAPI(g_options);
            g_isInitialized = false;
            static std::string successResponse = create_response(0, "AWS SDK cleaned up successfully");
            return successResponse.c_str();
        }
        catch (...) {
            static std::string unknownError = create_response(-1, "Error during AWS SDK cleanup");
            return unknownError.c_str();
        }
    } else {
        static std::string notInitialized = create_response(S3_ERROR_NOT_INITIALIZED, "AWS SDK was not initialized");
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
extern "C" S3UPLOAD_API const char* __stdcall UploadFile(
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
        response = create_response(S3_ERROR_INVALID_PARAMS, "Invalid parameters: one or more required parameters are null");
        return response.c_str();
    }

    if (!g_isInitialized) {
        response = create_response(S3_ERROR_NOT_INITIALIZED, "AWS SDK not initialized. Call InitializeAwsSDK() first");
        return response.c_str();
    }

    // Check if file exists
    if (!FileExists(localFilePath)) {
        response = create_response(S3_ERROR_FILE_NOT_EXISTS, "Local file does not exist: " + std::string(localFilePath));
        return response.c_str();
    }

    // Get file size
    long fileSize = GetS3FileSize(localFilePath);
    if (fileSize < 0) {
        response = create_response(S3_ERROR_FILE_READ_ERROR, "Cannot read file size: " + std::string(localFilePath));
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
        clientConfig.requestTimeoutMs = 30000;  // 30 seconds timeout
        clientConfig.connectTimeoutMs = 10000;  // 10 seconds connect timeout

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
            response = create_response(S3_ERROR_FILE_OPEN_ERROR, "Cannot open file for reading: " + std::string(localFilePath));
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
            response = create_response(S3_SUCCESS, oss.str());
            return response.c_str();
        } else {
            auto error = outcome.GetError();
            std::ostringstream oss;
            oss << "S3 upload failed: " << error.GetMessage().c_str()
                << " (Error Code: " << static_cast<int>(error.GetErrorType()) << ")";

            AWS_LOGSTREAM_ERROR("S3Upload", "Upload FAILED: " << oss.str());
            AWS_LOGSTREAM_ERROR("S3Upload", "Error type: " << error.GetExceptionName());
            response = create_response(S3_ERROR_S3_UPLOAD_FAILED, oss.str());
            return response.c_str();
        }

    } catch (const std::exception& e) {
        AWS_LOGSTREAM_ERROR("S3Upload", "Exception caught in UploadFileToS3WithToken: " << e.what());
      response = create_response(S3_ERROR_EXCEPTION, "Upload failed with exception: " + std::string(e.what()));
        return response.c_str();
    } catch (...) {
        AWS_LOGSTREAM_ERROR("S3Upload", "Unknown exception caught in UploadFileToS3WithToken");
        response = create_response(S3_ERROR_UNKNOWN, "Upload failed: Unknown error");
        return response.c_str();
    }
}

// simple upload status enum
enum SimpleUploadStatus {
    SIMPLE_UPLOAD_PENDING = 0,      // waiting to start
    SIMPLE_UPLOAD_UPLOADING = 1,    // uploading
    SIMPLE_UPLOAD_SUCCESS = 2,      // success
    SIMPLE_UPLOAD_FAILED = 3,       // failed
    SIMPLE_UPLOAD_CANCELLED = 4     // cancelled
};

// simple upload progress info structure
struct SimpleUploadProgress {
    std::string uploadId;           // upload ID (used as unique identifier)
    SimpleUploadStatus status;      // current status
    long long totalSize;            // total file size
    std::string errorMessage;       // error message
    std::chrono::steady_clock::time_point startTime; // start time
    std::chrono::steady_clock::time_point endTime;   // end time
    std::atomic<bool> shouldCancel; // cancel flag

    SimpleUploadProgress() : status(SIMPLE_UPLOAD_PENDING), totalSize(0), shouldCancel(false) {}
};

// simple upload manager
class SimpleUploadManager {
private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<SimpleUploadProgress>> uploads_;

public:
    static SimpleUploadManager& getInstance() {
        static SimpleUploadManager instance;
        return instance;
    }

    std::string addUpload(const std::string& uploadId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto progress = std::make_shared<SimpleUploadProgress>();
        progress->uploadId = uploadId;
        uploads_[uploadId] = progress;
        return uploadId;
    }

    std::shared_ptr<SimpleUploadProgress> getUpload(const std::string& uploadId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = uploads_.find(uploadId);
        return it != uploads_.end() ? it->second : nullptr;
    }

    void removeUpload(const std::string& uploadId) {
        std::lock_guard<std::mutex> lock(mutex_);
        uploads_.erase(uploadId);
    }

    void updateProgress(const std::string& uploadId, SimpleUploadStatus status,
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

// Async upload worker
void asyncUploadWorker(const std::string& uploadId,
                      const std::string& accessKey,
                      const std::string& secretKey,
                      const std::string& sessionToken,
                      const std::string& region,
                      const std::string& bucketName,
                      const std::string& objectKey,
                      const std::string& localFilePath) {

    auto& manager = SimpleUploadManager::getInstance();
    auto progress = manager.getUpload(uploadId);

    if (!progress) return;

    try {
        progress->startTime = std::chrono::steady_clock::now();
        manager.updateProgress(uploadId, SIMPLE_UPLOAD_UPLOADING);

        AWS_LOGSTREAM_INFO("S3Upload", "=== Starting Async Upload ===");
        AWS_LOGSTREAM_INFO("S3Upload", "Upload ID: " << uploadId);
        AWS_LOGSTREAM_INFO("S3Upload", "File: " << localFilePath);

        // 
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_CANCELLED);
            return;
        }

        // 
        if (accessKey.empty() || secretKey.empty() || region.empty() ||
            bucketName.empty() || objectKey.empty() || localFilePath.empty()) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, "Invalid parameters");
            return;
        }

        if (!g_isInitialized) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, "AWS SDK not initialized");
            return;
        }

        // check if file exists
        if (!FileExists(localFilePath.c_str())) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, "Local file does not exist");
            return;
        }

        // get file size
        long fileSize = GetS3FileSize(localFilePath.c_str());
        if (fileSize < 0) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, "Cannot read file size");
            return;
        }

        progress->totalSize = fileSize;
        AWS_LOGSTREAM_INFO("S3Upload", "File size: " << fileSize << " bytes");

        // check cancel flag
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_CANCELLED);
            return;
        }

        // configure client
        Aws::S3::S3ClientConfiguration clientConfig;
        clientConfig.region = region;
        clientConfig.requestTimeoutMs = 30000;  // 30 seconds timeout
        clientConfig.connectTimeoutMs = 10000;  // 10 seconds connect timeout

        // create AWS credentials
        Aws::Auth::AWSCredentials credentials;
        if (!sessionToken.empty()) {
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey, sessionToken);
            AWS_LOGSTREAM_INFO("S3Upload", "Using temporary credentials with session token");
        } else {
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey);
            AWS_LOGSTREAM_INFO("S3Upload", "Using permanent credentials");
        }

        // create credentials provider and S3 client
        auto credentialsProvider = Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>("S3Upload", credentials);
        Aws::S3::S3Client s3Client(credentialsProvider, nullptr, clientConfig);

        // create upload request
        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bucketName);
        request.SetKey(objectKey);

        // check cancel flag
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_CANCELLED);
            return;
        }

        // open file stream
        auto inputData = Aws::MakeShared<Aws::FStream>("PutObjectInputStream",
                                                       localFilePath.c_str(),
                                                       std::ios_base::in | std::ios_base::binary);

        if (!inputData->is_open()) {
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, "Cannot open file for reading");
            return;
        }

        request.SetBody(inputData);
        request.SetContentType("application/octet-stream");

        AWS_LOGSTREAM_INFO("S3Upload", "Starting S3 PutObject operation...");

        // execute upload
        auto outcome = s3Client.PutObject(request);

        AWS_LOGSTREAM_INFO("S3Upload", "PutObject operation completed");

        if (outcome.IsSuccess()) {
            progress->endTime = std::chrono::steady_clock::now();
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_SUCCESS);
            AWS_LOGSTREAM_INFO("S3Upload", "Async upload SUCCESS for ID: " << uploadId);
        } else {
            auto error = outcome.GetError();
            std::string errorMsg = "S3 upload failed: " + std::string(error.GetMessage().c_str());
            manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, errorMsg);
            AWS_LOGSTREAM_ERROR("S3Upload", "Async upload FAILED for ID: " << uploadId << " - " << errorMsg);
        }

    } catch (const std::exception& e) {
        std::string errorMsg = "Upload failed with exception: " + std::string(e.what());
        manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, errorMsg);
        AWS_LOGSTREAM_ERROR("S3Upload", "Exception in async upload: " << e.what());
    } catch (...) {
        manager.updateProgress(uploadId, SIMPLE_UPLOAD_FAILED, "Unknown error");
        AWS_LOGSTREAM_ERROR("S3Upload", "Unknown exception in async upload");
    }
}

// exported async upload function
extern "C" S3UPLOAD_API const char* __stdcall StartAsyncUpload(
    const char* accessKey,
    const char* secretKey,
    const char* sessionToken,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath
) {
    static std::string response;

    // parameter validation
    if (!accessKey || !secretKey || !region || !bucketName || !objectKey || !localFilePath) {
        response = create_response(S3_ERROR_INVALID_PARAMS, "Invalid parameters: one or more required parameters are null");
        return response.c_str();
    }

    if (!g_isInitialized) {
        response = create_response(S3_ERROR_NOT_INITIALIZED, "AWS SDK not initialized. Call InitializeAwsSDK() first");
        return response.c_str();
    }

    try {
        // generate unique upload ID
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        std::string uploadId = "simple_upload_" + std::to_string(timestamp);

        // add to manager
        SimpleUploadManager::getInstance().addUpload(uploadId);

        // convert parameters to string (avoid pointer lifetime issues)
        std::string strAccessKey = accessKey;
        std::string strSecretKey = secretKey;
        std::string strSessionToken = sessionToken ? sessionToken : "";
        std::string strRegion = region;
        std::string strBucketName = bucketName;
        std::string strObjectKey = objectKey;
        std::string strLocalFilePath = localFilePath;

        // start async thread
        std::thread uploadThread(asyncUploadWorker, uploadId,
                               strAccessKey, strSecretKey, strSessionToken,
                               strRegion, strBucketName, strObjectKey, strLocalFilePath);
        uploadThread.detach();

        response = create_response(S3_SUCCESS, uploadId);
        return response.c_str();

    } catch (const std::exception& e) {
        response = create_response(S3_ERROR_EXCEPTION, "Failed to start async upload: " + std::string(e.what()));
        return response.c_str();
    } catch (...) {
        response = create_response(S3_ERROR_UNKNOWN, "Failed to start async upload: Unknown error");
        return response.c_str();
    }
}

// get simple upload status
extern "C" S3UPLOAD_API const char* __stdcall GetSimpleUploadStatus(const char* uploadId) {
    static std::string response;

    if (!uploadId) {
        response = create_response(S3_ERROR_INVALID_PARAMS, "Upload ID is null");
        return response.c_str();
    }

    auto progress = SimpleUploadManager::getInstance().getUpload(uploadId);
    if (!progress) {
        response = create_response(S3_ERROR_INVALID_PARAMS, "Upload ID not found");
        return response.c_str();
    }

    try {
        std::ostringstream oss;
        oss << "{"
            << "\"code\":" << S3_SUCCESS << ","
            << "\"uploadId\":\"" << progress->uploadId << "\","
            << "\"status\":" << progress->status << ","
            << "\"totalSize\":" << progress->totalSize << ","
            << "\"errorMessage\":\"" << progress->errorMessage << "\"";

        // calculate duration (if completed)
        if (progress->status == SIMPLE_UPLOAD_SUCCESS || progress->status == SIMPLE_UPLOAD_FAILED) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                progress->endTime - progress->startTime);
            oss << ",\"durationSeconds\":" << duration.count();
        }

        oss << "}";

        return oss.str().c_str();

    } catch (const std::exception& e) {
        response = create_response(S3_ERROR_EXCEPTION, "Failed to get upload status: " + std::string(e.what()));
        return response.c_str();
    } catch (...) {
        response = create_response(S3_ERROR_UNKNOWN, "Failed to get upload status: Unknown error");
        return response.c_str();
    }
}

// cancel simple upload
extern "C" S3UPLOAD_API const char* __stdcall CancelSimpleUpload(const char* uploadId) {
    static std::string response;

    if (!uploadId) {
        response = create_response(S3_ERROR_INVALID_PARAMS, "Upload ID is null");
        return response.c_str();
    }

    auto progress = SimpleUploadManager::getInstance().getUpload(uploadId);
    if (!progress) {
        response = create_response(S3_ERROR_INVALID_PARAMS, "Upload ID not found");
        return response.c_str();
    }

    try {
        // set cancel flag
        progress->shouldCancel.store(true);

        response = create_response(S3_SUCCESS, "Cancel request sent");
        return response.c_str();

    } catch (const std::exception& e) {
        response = create_response(S3_ERROR_EXCEPTION, "Failed to cancel upload: " + std::string(e.what()));
        return response.c_str();
    } catch (...) {
        response = create_response(S3_ERROR_UNKNOWN, "Failed to cancel upload: Unknown error");
        return response.c_str();
    }
}

// cleanup completed simple upload records
extern "C" S3UPLOAD_API const char* __stdcall CleanupSimpleUpload(const char* uploadId) {
    static std::string response;

    if (!uploadId) {
        response = create_response(S3_ERROR_INVALID_PARAMS, "Upload ID is null");
        return response.c_str();
    }

    try {
        SimpleUploadManager::getInstance().removeUpload(uploadId);
        response = create_response(S3_SUCCESS, "Upload record cleaned up");
        return response.c_str();

    } catch (const std::exception& e) {
        response = create_response(S3_ERROR_EXCEPTION, "Failed to cleanup upload: " + std::string(e.what()));
        return response.c_str();
    } catch (...) {
        response = create_response(S3_ERROR_UNKNOWN, "Failed to cleanup upload: Unknown error");
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

