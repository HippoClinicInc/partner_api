#include "S3UploadLib.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

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
static std::string g_lastError = "";
static bool g_isInitialized = false;
static Aws::SDKOptions g_options;

// Set error message (internal function)
void SetLastError(const std::string& error) {
    g_lastError = error;
}

// Initialize AWS SDK
extern "C" S3UPLOAD_API int __stdcall InitializeAwsSDK() {
    if (g_isInitialized) {
        SetLastError("AWS SDK already initialized");
        return 0; // Already initialized, return success
    }

    try {
        // Set log level (can be adjusted as needed)
        g_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;

        // Initialize AWS SDK
        Aws::InitAPI(g_options);
        g_isInitialized = true;

        SetLastError("AWS SDK initialized successfully");
        return 0; // Success
    }
    catch (const std::exception& e) {
        SetLastError("Failed to initialize AWS SDK: " + std::string(e.what()));
        return -1; // Failure
    }
    catch (...) {
        SetLastError("Failed to initialize AWS SDK: Unknown error");
        return -1; // Failure
    }
}

// Cleanup AWS SDK
extern "C" S3UPLOAD_API void __stdcall CleanupAwsSDK() {
    if (g_isInitialized) {
        try {
            Aws::ShutdownAPI(g_options);
            g_isInitialized = false;
            SetLastError("AWS SDK cleaned up successfully");
        }
        catch (...) {
            SetLastError("Error during AWS SDK cleanup");
        }
    } else {
        SetLastError("AWS SDK was not initialized");
    }
}

// Get last error message
extern "C" S3UPLOAD_API const char* __stdcall GetS3LastError() {
    return g_lastError.c_str();
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

// Test function
extern "C" S3UPLOAD_API int __stdcall TestS3Library() {
    SetLastError("S3 Library test function called successfully");
    return 100; // Return fixed value to indicate DLL is working properly
}

// Upload file to S3 (without Session Token)
extern "C" S3UPLOAD_API int __stdcall UploadFileToS3(
    const char* accessKey,
    const char* secretKey,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath
) {
    // Parameter validation
    if (!accessKey || !secretKey || !region || !bucketName || !objectKey || !localFilePath) {
        SetLastError("Invalid parameters: one or more parameters are null");
        return S3_ERROR_INVALID_PARAMS;
    }

    if (!g_isInitialized) {
        SetLastError("AWS SDK not initialized. Call InitializeAwsSDK() first");
        return S3_ERROR_NOT_INITIALIZED;
    }

    // Check if file exists
    if (!FileExists(localFilePath)) {
        SetLastError("Local file does not exist: " + std::string(localFilePath));
        return S3_ERROR_FILE_NOT_EXISTS;
    }

    // Get file size
    long fileSize = GetS3FileSize(localFilePath);
    if (fileSize < 0) {
        SetLastError("Cannot read file size: " + std::string(localFilePath));
        return S3_ERROR_FILE_READ_ERROR;
    }

    try {
        AWS_LOGSTREAM_INFO("S3Upload", "=== Starting UploadFileToS3 (No Token) ===");
        AWS_LOGSTREAM_INFO("S3Upload", "AccessKey (FULL): " << std::string(accessKey));
        AWS_LOGSTREAM_INFO("S3Upload", "SecretKey (FULL): " << std::string(secretKey));
        AWS_LOGSTREAM_INFO("S3Upload", "Region: " << region);
        AWS_LOGSTREAM_INFO("S3Upload", "Bucket: " << bucketName);
        AWS_LOGSTREAM_INFO("S3Upload", "Object: " << objectKey);
        AWS_LOGSTREAM_INFO("S3Upload", "File: " << localFilePath);
        
        // Configure client
        AWS_LOGSTREAM_INFO("S3Upload", "Creating S3 client configuration...");
        Aws::S3::S3ClientConfiguration clientConfig;
        clientConfig.region = region;
        clientConfig.requestTimeoutMs = 30000;  // 30 seconds timeout
        clientConfig.connectTimeoutMs = 10000;  // 10 seconds connect timeout

        // Create AWS credentials (without Session Token)
        AWS_LOGSTREAM_INFO("S3Upload", "Creating permanent AWS credentials...");
        Aws::Auth::AWSCredentials credentials(accessKey, secretKey);
        
        // Create credentials provider
        auto credentialsProvider = Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>("S3Upload", credentials);

        // Create S3 client - using credentials provider constructor
        Aws::S3::S3Client s3Client(credentialsProvider, nullptr, clientConfig);

        // Create upload request
        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bucketName);
        request.SetKey(objectKey);

        // Open file stream
        auto inputData = Aws::MakeShared<Aws::FStream>("PutObjectInputStream",
                                                       localFilePath,
                                                       std::ios_base::in | std::ios_base::binary);

        if (!inputData->is_open()) {
            SetLastError("Cannot open file for reading: " + std::string(localFilePath));
            return S3_ERROR_FILE_OPEN_ERROR;
        }

        request.SetBody(inputData);
        request.SetContentType("application/octet-stream");

        // Execute upload
        auto outcome = s3Client.PutObject(request);

        if (outcome.IsSuccess()) {
            std::ostringstream oss;
            oss << "Successfully uploaded " << localFilePath
                << " (" << fileSize << " bytes) to s3://"
                << bucketName << "/" << objectKey
                << " in region " << region;

            SetLastError(oss.str());
            return S3_SUCCESS;
        } else {
            auto error = outcome.GetError();
            std::ostringstream oss;
            oss << "S3 upload failed: " << error.GetMessage().c_str()
                << " (Error Code: " << static_cast<int>(error.GetErrorType()) << ")";

            SetLastError(oss.str());
            return S3_ERROR_S3_UPLOAD_FAILED;
        }

    } catch (const std::exception& e) {
        SetLastError("Upload failed with exception: " + std::string(e.what()));
        return S3_ERROR_EXCEPTION;
    } catch (...) {
        SetLastError("Upload failed: Unknown error");
        return S3_ERROR_UNKNOWN;
    }
}

// S3 upload implementation with Session Token support
extern "C" S3UPLOAD_API int __stdcall UploadFileToS3WithToken(
    const char* accessKey,
    const char* secretKey,
    const char* sessionToken,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath
) {
    // Parameter validation
    if (!accessKey || !secretKey || !region || !bucketName || !objectKey || !localFilePath) {
        SetLastError("Invalid parameters: one or more required parameters are null");
        return S3_ERROR_INVALID_PARAMS;
    }

    if (!g_isInitialized) {
        SetLastError("AWS SDK not initialized. Call InitializeAwsSDK() first");
        return S3_ERROR_NOT_INITIALIZED;
    }

    // Check if file exists
    if (!FileExists(localFilePath)) {
        SetLastError("Local file does not exist: " + std::string(localFilePath));
        return S3_ERROR_FILE_NOT_EXISTS;
    }

    // Get file size
    long fileSize = GetS3FileSize(localFilePath);
    if (fileSize < 0) {
        SetLastError("Cannot read file size: " + std::string(localFilePath));
        return S3_ERROR_FILE_READ_ERROR;
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
            SetLastError("Cannot open file for reading: " + std::string(localFilePath));
            return S3_ERROR_FILE_OPEN_ERROR;
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
            SetLastError(oss.str());
            return S3_SUCCESS;
        } else {
            auto error = outcome.GetError();
            std::ostringstream oss;
            oss << "S3 upload failed: " << error.GetMessage().c_str()
                << " (Error Code: " << static_cast<int>(error.GetErrorType()) << ")";

            AWS_LOGSTREAM_ERROR("S3Upload", "Upload FAILED: " << oss.str());
            AWS_LOGSTREAM_ERROR("S3Upload", "Error type: " << error.GetExceptionName());
            SetLastError(oss.str());
            return S3_ERROR_S3_UPLOAD_FAILED;
        }

    } catch (const std::exception& e) {
        AWS_LOGSTREAM_ERROR("S3Upload", "Exception caught in UploadFileToS3WithToken: " << e.what());
        SetLastError("Upload failed with exception: " + std::string(e.what()));
        return S3_ERROR_EXCEPTION;
    } catch (...) {
        AWS_LOGSTREAM_ERROR("S3Upload", "Unknown exception caught in UploadFileToS3WithToken");
        SetLastError("Upload failed: Unknown error");
        return S3_ERROR_UNKNOWN;
    }
}

// Simplified version: using JSON configuration
extern "C" S3UPLOAD_API int __stdcall UploadFileToS3Simple(
    const char* configJson,
    const char* localFilePath
) {
    if (!configJson || !localFilePath) {
        SetLastError("Invalid parameters: configJson or localFilePath is null");
        return S3_ERROR_INVALID_PARAMS;
    }

    // Simple JSON parsing (should use proper JSON library in production)
    std::string config(configJson);

    // Extract configuration information
    std::string accessKey, secretKey, region, bucketName, objectKey, sessionToken;

    // Simple string search parsing (for demonstration only)
    size_t pos;

    // Parse accessKey
    pos = config.find("\"accessKey\"");
    if (pos != std::string::npos) {
        size_t start = config.find("\"", pos + 12) + 1;
        size_t end = config.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            accessKey = config.substr(start, end - start);
        }
    }

    // Parse secretKey
    pos = config.find("\"secretKey\"");
    if (pos != std::string::npos) {
        size_t start = config.find("\"", pos + 12) + 1;
        size_t end = config.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            secretKey = config.substr(start, end - start);
        }
    }

    // Parse sessionToken
    pos = config.find("\"sessionToken\"");
    if (pos != std::string::npos) {
        size_t start = config.find("\"", pos + 15) + 1;
        size_t end = config.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            sessionToken = config.substr(start, end - start);
        }
    }

    // Parse region
    pos = config.find("\"region\"");
    if (pos != std::string::npos) {
        size_t start = config.find("\"", pos + 9) + 1;
        size_t end = config.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            region = config.substr(start, end - start);
        }
    }

    // Parse bucket
    pos = config.find("\"bucket\"");
    if (pos != std::string::npos) {
        size_t start = config.find("\"", pos + 9) + 1;
        size_t end = config.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            bucketName = config.substr(start, end - start);
        }
    }

    // Parse objectKey
    pos = config.find("\"objectKey\"");
    if (pos != std::string::npos) {
        size_t start = config.find("\"", pos + 12) + 1;
        size_t end = config.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            objectKey = config.substr(start, end - start);
        }
    }

    // Validate required parameters
    if (accessKey.empty() || secretKey.empty() || region.empty() || bucketName.empty()) {
        SetLastError("Missing required configuration parameters in JSON");
        return S3_ERROR_INVALID_PARAMS;
    }

    // If objectKey not specified, use filename
    if (objectKey.empty()) {
        std::string filePath(localFilePath);
        size_t lastSlash = filePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            objectKey = filePath.substr(lastSlash + 1);
        } else {
            objectKey = filePath;
        }
    }

    // Call corresponding upload function
    if (!sessionToken.empty()) {
        return UploadFileToS3WithToken(
            accessKey.c_str(),
            secretKey.c_str(),
            sessionToken.c_str(),
            region.c_str(),
            bucketName.c_str(),
            objectKey.c_str(),
            localFilePath
        );
    } else {
        return UploadFileToS3(
            accessKey.c_str(),
            secretKey.c_str(),
            region.c_str(),
            bucketName.c_str(),
            objectKey.c_str(),
            localFilePath
        );
    }
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // When DLL is loaded
        SetLastError("S3UploadLib DLL loaded successfully");
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
