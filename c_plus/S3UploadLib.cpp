#include "S3UploadLib.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <chrono>
#include <thread>

// MinGW compatibility fix: define missing byte order conversion functions
#ifdef __MINGW32__
// define MinGW missing byte order conversion functions
// use GCC built-in functions, no need to include intrin.h
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
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/UploadPartRequest.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/AbortMultipartUploadRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
#include <aws/core/utils/HashingUtils.h>
// JSON processing (simple implementation)
#include <regex>

// global variables
static std::string g_multipartLastError = "";
static bool g_isInitialized = false;
static Aws::SDKOptions g_options;

const char* create_response(int code, const std::string& message) {
    static std::string response;
    std::ostringstream oss;
    oss << "{"
        << "\"code\":" << code << ","
        << "\"message\":\"" << message << "\""
        << "}";
    response = oss.str();
    return response.c_str();
}

// Initialize AWS SDK
extern "C" S3UPLOAD_API const char* __stdcall InitializeAwsSDK() {
    if (g_isInitialized) {
        return create_response(-2, "AWS SDK already initialized");
    }

    try {
        // Set log level (can be adjusted as needed)
        g_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;

        // Initialize AWS SDK
        Aws::InitAPI(g_options);
        g_isInitialized = true;

        return create_response(S3_SUCCESS, "AWS SDK initialized successfully");
    }
    catch (const std::exception& e) {
        return create_response(S3_ERROR_INVALID_PARAMS, std::string("Failed to initialize AWS SDK: ") + e.what());
    }
    catch (...) {
        return create_response(S3_ERROR_INVALID_PARAMS, "Failed to initialize AWS SDK: Unknown error");
    }
}

// Cleanup AWS SDK
extern "C" S3UPLOAD_API const char* __stdcall CleanupAwsSDK() {
    if (g_isInitialized) {
        try {
            Aws::ShutdownAPI(g_options);
            g_isInitialized = false;
            return create_response(S3_SUCCESS, "AWS SDK cleaned up successfully");
        }
        catch (...) {
            return create_response(S3_ERROR_INVALID_PARAMS, "Error during AWS SDK cleanup");
        }
    } else {
        return create_response(S3_ERROR_NOT_INITIALIZED, "AWS SDK was not initialized");
    }
}

// Check if file exists
extern "C" S3UPLOAD_API int __stdcall FileExistsMultipart(const char* filePath) {
    if (!filePath) return 0;
    
    std::ifstream file(filePath);
    return file.good() ? 1 : 0;
}

// Get file size
extern "C" S3UPLOAD_API long long __stdcall GetFileSizeMultipart(const char* filePath) {
    if (!filePath) return -1;
    
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return -1;
    }
    return static_cast<long long>(file.tellg());
}


// initialize MultipartConfig structure
extern "C" S3UPLOAD_API void __stdcall InitializeMultipartConfig(MultipartConfig* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(MultipartConfig));
    config->partSize = S3_DEFAULT_PART_SIZE;
    config->maxRetries = S3_DEFAULT_MAX_RETRIES;
    config->userData = nullptr;
}

// cancel ongoing multipart upload
extern "C" S3UPLOAD_API const char* __stdcall cancelMultipartUpload(
    const char* accessKey,
    const char* secretKey,
    const char* sessionToken,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* uploadId
) {
    if (!accessKey || !secretKey || !region || !bucketName || !objectKey || !uploadId) {
        return create_response(S3_ERROR_INVALID_PARAMS, "Invalid parameters for cancel operation");
    }

    if (!g_isInitialized) {
        return create_response(S3_ERROR_NOT_INITIALIZED, "AWS SDK not initialized");
    }

    try {
        Aws::S3::S3ClientConfiguration clientConfig;
        clientConfig.region = region;

        Aws::Auth::AWSCredentials credentials;
        if (sessionToken && strlen(sessionToken) > 0) {
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey, sessionToken);
        } else {
            credentials = Aws::Auth::AWSCredentials(accessKey, secretKey);
        }

        auto credentialsProvider = Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>("S3MultipartCancel", credentials);
        Aws::S3::S3Client s3Client(credentialsProvider, nullptr, clientConfig);

        Aws::S3::Model::AbortMultipartUploadRequest request;
        request.SetBucket(bucketName);
        request.SetKey(objectKey);
        request.SetUploadId(uploadId);

        auto outcome = s3Client.AbortMultipartUpload(request);
        if (outcome.IsSuccess()) {
            return create_response(S3_SUCCESS, "Multipart upload cancelled successfully");
        } else {
            return create_response(S3_ERROR_S3_UPLOAD_FAILED, std::string("Failed to cancel multipart upload: ") + outcome.GetError().GetMessage());
        }
    } catch (const std::exception& e) {
        return create_response(S3_ERROR_EXCEPTION, std::string("Cancel operation failed: ") + e.what());
    }
}

// Multipart upload core implementation
extern "C" S3UPLOAD_API const char* __stdcall MultipartUploadToS3(const MultipartConfig* config) {
    // parameter validation
    if (!config || !config->accessKey || !config->secretKey || !config->region || 
        !config->bucketName || !config->objectKey || !config->localFilePath) {
        return create_response(S3_ERROR_INVALID_PARAMS, "Invalid parameters: one or more required parameters are null");
    }

    // check if AWS SDK is initialized
    if (!g_isInitialized) {
        return create_response(S3_ERROR_NOT_INITIALIZED, "AWS SDK not initialized. Call InitializeAwsSDK() first");
    }

    // check if local file exists
    if (!FileExistsMultipart(config->localFilePath)) {
        return create_response(S3_ERROR_FILE_NOT_EXISTS, std::string("Local file does not exist: ") + config->localFilePath);
    }

    // get local file size
    long long fileSize = GetFileSizeMultipart(config->localFilePath);
    if (fileSize < 0) {
        return create_response(S3_ERROR_FILE_READ_ERROR, std::string("Cannot read file size: ") + config->localFilePath);
    }

    // part size is default part size
    long partSize = S3_DEFAULT_PART_SIZE;

    // calculate part number, if part number is greater than maximum parts, return error
    int totalParts = static_cast<int>((fileSize + partSize - 1) / partSize);
    if (totalParts > S3_MAX_PARTS) {
        return create_response(S3_ERROR_INVALID_PARAMS, "File too large for multipart upload. Max parts: " + std::to_string(S3_MAX_PARTS));
    }

    try {
        AWS_LOGSTREAM_INFO("S3MultipartUpload", "=== Starting Multipart Upload ===");
        AWS_LOGSTREAM_INFO("S3MultipartUpload", "File: " << config->localFilePath);
        AWS_LOGSTREAM_INFO("S3MultipartUpload", "Size: " << fileSize << " bytes");
        AWS_LOGSTREAM_INFO("S3MultipartUpload", "Part size: " << partSize << " bytes");
        AWS_LOGSTREAM_INFO("S3MultipartUpload", "Total parts: " << totalParts);

        // 1.configure client
        Aws::S3::S3ClientConfiguration clientConfig;
        clientConfig.region = config->region;
        clientConfig.requestTimeoutMs = 30000;   // 30 seconds timeout (same as successful case)
        clientConfig.connectTimeoutMs = 10000;   // 10 seconds connect timeout (same as successful case)
        clientConfig.maxConnections = 25;        // 25 connections (same as successful case)

        clientConfig.enableTcpKeepAlive = true;  // Enable TCP keep-alive
        clientConfig.tcpKeepAliveIntervalMs = 30000; // 30 seconds keep-alive interval

        clientConfig.httpLibOverride = Aws::Http::TransferLibType::WIN_HTTP_CLIENT;

        // 2.create AWS credentials
        Aws::Auth::AWSCredentials credentials;
        if (config->sessionToken && strlen(config->sessionToken) > 0) {
            credentials = Aws::Auth::AWSCredentials(config->accessKey, config->secretKey, config->sessionToken);
        } else {
            credentials = Aws::Auth::AWSCredentials(config->accessKey, config->secretKey);
        }

        // 3.create credentials provider and S3 client
        auto credentialsProvider = Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>("S3MultipartUpload", credentials);
        Aws::S3::S3Client s3Client(credentialsProvider, nullptr, clientConfig);

        // 4.create new multipart upload
        Aws::S3::Model::CreateMultipartUploadRequest createRequest;
        createRequest.SetBucket(config->bucketName);
        createRequest.SetKey(config->objectKey);
        createRequest.SetContentType("application/octet-stream");

        auto createOutcome = s3Client.CreateMultipartUpload(createRequest);
        if (!createOutcome.IsSuccess()) {
            return create_response(S3_ERROR_S3_UPLOAD_FAILED, std::string("Failed to create multipart upload: ") + createOutcome.GetError().GetMessage());
        }

        std::string uploadId = createOutcome.GetResult().GetUploadId();
        AWS_LOGSTREAM_INFO("S3MultipartUpload", "Created new multipart upload with ID: " << uploadId);
  
        std::vector<Aws::S3::Model::CompletedPart> completedParts;
        long long uploadedBytes = 0;

        // 5.create file stream once for all parts
        std::ifstream file(config->localFilePath, std::ios::binary);
        if (!file.is_open()) {
            return create_response(S3_ERROR_FILE_OPEN_ERROR, std::string("Cannot open file for reading: ") + config->localFilePath);
        }

        // 6.upload all parts sequentially
        for (int partNumber = 1; partNumber <= totalParts; ++partNumber) {
            long long offset = (partNumber - 1) * partSize;
            long long currentPartSize = (static_cast<long long>(partSize) < (fileSize - offset)) ?
                                       static_cast<long long>(partSize) : (fileSize - offset);

            AWS_LOGSTREAM_INFO("S3MultipartUpload", "Uploading part " << partNumber << "/" << totalParts
                              << " (size: " << (long long)currentPartSize << " bytes)");

            bool partSuccess = false;
            std::string etag;
            int retryCount = 0;
            int maxRetries = config->maxRetries > 0 ? config->maxRetries : 3;

            // 6.1. retry upload part if failed, max retries is 3
            while (retryCount <= maxRetries && !partSuccess) {
                try {
                    // every time retry, read data again, avoid stream status problem
                    file.seekg(offset);

                    // read data to memory buffer
                    std::vector<char> buffer(currentPartSize);
                    file.read(buffer.data(), currentPartSize);

                    // check if file read error
                    if (!file.good() && !file.eof()) {
                        AWS_LOGSTREAM_ERROR("S3MultipartUpload", "File read error at offset " << offset);
                        return create_response(S3_ERROR_FILE_READ_ERROR, "File read error");
                    }

                    // create memory stream
                    auto memoryStream = Aws::MakeShared<Aws::StringStream>("UploadPartStream");
                    memoryStream->write(buffer.data(), currentPartSize);
                    memoryStream->seekg(0);

                    Aws::S3::Model::UploadPartRequest uploadRequest;
                    uploadRequest.SetBucket(config->bucketName);
                    uploadRequest.SetKey(config->objectKey);
                    uploadRequest.SetUploadId(uploadId);
                    uploadRequest.SetPartNumber(partNumber);
                    uploadRequest.SetBody(memoryStream);
                    uploadRequest.SetContentLength(currentPartSize);

                    // record upload start time
                    auto startTime = std::chrono::steady_clock::now();

                    // execute upload
                    auto uploadOutcome = s3Client.UploadPart(uploadRequest);

                    auto endTime = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

                    if (uploadOutcome.IsSuccess()) {
                        etag = uploadOutcome.GetResult().GetETag();
                        partSuccess = true;
                        AWS_LOGSTREAM_INFO("S3MultipartUpload", "Part " << partNumber << " uploaded successfully");
                    } else {
                        retryCount++;
                        if (retryCount <= maxRetries) {
                            AWS_LOGSTREAM_WARN("S3MultipartUpload", "Part " << partNumber << " upload failed (attempt " << retryCount << "/" << (maxRetries + 1) << "): " << uploadOutcome.GetError().GetMessage());
                            std::this_thread::sleep_for(std::chrono::seconds(1)); // wait 1 second before retrying
                        } else {
                            AWS_LOGSTREAM_ERROR("S3MultipartUpload", "Part " << partNumber << " upload failed after " << (maxRetries + 1) << " attempts: " << uploadOutcome.GetError().GetMessage());
                            return create_response(S3_ERROR_S3_UPLOAD_FAILED, std::string("Part ") + std::to_string(partNumber) + " upload failed: " + uploadOutcome.GetError().GetMessage());
                        }
                    }
                } catch (const std::exception& e) {
                    retryCount++;
                    AWS_LOGSTREAM_WARN("S3MultipartUpload", "Part " << partNumber << " upload exception (attempt "
                                      << retryCount << "/" << (maxRetries + 1) << "): " << e.what());
                    if (retryCount <= maxRetries) {
                        AWS_LOGSTREAM_WARN("S3MultipartUpload", "Part " << partNumber << " upload exception (attempt " << retryCount << "/" << (maxRetries + 1) << "): " << e.what());
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    } else {
                        AWS_LOGSTREAM_ERROR("S3MultipartUpload", "Part " << partNumber << " upload exception after " << (maxRetries + 1) << " attempts: " << e.what());
                        return create_response(S3_ERROR_EXCEPTION, std::string("Part ") + std::to_string(partNumber) + " upload exception: " + e.what());
                    }
                }
            }

            // 6.2. if part upload failed after all retries, return error
            if (!partSuccess) {
                // cancel multipart upload
                cancelMultipartUpload(config->accessKey, config->secretKey, config->sessionToken, config->region, config->bucketName, config->objectKey, uploadId.c_str());
                return create_response(S3_ERROR_S3_UPLOAD_FAILED, "Part " + std::to_string(partNumber) + " upload failed after all retries");
            }

            // 6.3. add to completed parts list
            Aws::S3::Model::CompletedPart completedPart;
            completedPart.SetPartNumber(partNumber);
            completedPart.SetETag(etag);
            completedParts.push_back(completedPart);
            uploadedBytes += currentPartSize;
        }

        // 7.close file stream
        file.close();

        // 8.sort by part number
        std::sort(completedParts.begin(), completedParts.end(),
                 [](const Aws::S3::Model::CompletedPart& a, const Aws::S3::Model::CompletedPart& b) {
                     return a.GetPartNumber() < b.GetPartNumber();
                 });

        // 9.complete multipart upload
        Aws::S3::Model::CompleteMultipartUploadRequest completeRequest;
        completeRequest.SetBucket(config->bucketName);
        completeRequest.SetKey(config->objectKey);
        completeRequest.SetUploadId(uploadId);

        Aws::S3::Model::CompletedMultipartUpload completedUpload;
        completedUpload.SetParts(completedParts);
        completeRequest.SetMultipartUpload(completedUpload);

        auto completeOutcome = s3Client.CompleteMultipartUpload(completeRequest);
        if (!completeOutcome.IsSuccess()) {
            return create_response(S3_ERROR_S3_UPLOAD_FAILED, std::string("Failed to complete multipart upload: ") + completeOutcome.GetError().GetMessage());
        }

        // 10.return success
        std::ostringstream oss;
        oss << "Successfully uploaded " << config->localFilePath
            << " (" << fileSize << " bytes) to s3://"
            << config->bucketName << "/" << config->objectKey
            << " in " << totalParts << " parts";

        AWS_LOGSTREAM_INFO("S3MultipartUpload", "Multipart upload completed successfully");
        return create_response(S3_SUCCESS, oss.str());

    } catch (const std::exception& e) {
        return create_response(S3_ERROR_EXCEPTION, std::string("Multipart upload failed with exception: ") + e.what());
    } catch (...) {
        return create_response(S3_ERROR_UNKNOWN, "Multipart upload failed: Unknown error");
    }
}

// upload file to S3
extern "C" S3UPLOAD_API const char* __stdcall UploadFile(
    const char* accessKey,
    const char* secretKey,
    const char* sessionToken,
    const char* region,
    const char* bucketName,
    const char* objectKey,
    const char* localFilePath,
    int retryCount
) {
    MultipartConfig config;
    InitializeMultipartConfig(&config);

    config.accessKey = accessKey;
    config.secretKey = secretKey;
    config.sessionToken = sessionToken;
    config.region = region;
    config.bucketName = bucketName;
    config.objectKey = objectKey;
    config.localFilePath = localFilePath;
    config.maxRetries = retryCount;

    return MultipartUploadToS3(&config);
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
        if (g_isInitialized) {
            CleanupAwsSDK();
        }
        break;
    }
    return TRUE;
}
