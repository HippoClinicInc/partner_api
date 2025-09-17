#include "../common/S3Common.h"

// Async upload worker thread function
// This function runs in a separate thread to handle file upload to S3
void asyncUploadWorker(const String& uploadId,
                      const String& accessKey,
                      const String& secretKey,
                      const String& sessionToken,
                      const String& region,
                      const String& bucketName,
                      const String& objectKey,
                      const String& localFilePath) {

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
            manager.updateProgress(uploadId, UPLOAD_FAILED, "AWS SDK not initialized");
            return;
        }

        // Step 6: Check if local file exists
        if (!FileExists(localFilePath.c_str())) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, "Local file does not exist");
            return;
        }

        // Step 7: Get file size and validate
        long fileSize = GetS3FileSize(localFilePath.c_str());
        if (fileSize < 0) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, "Cannot read file size");
            return;
        }

        progress->totalSize = fileSize;
        AWS_LOGSTREAM_INFO("S3Upload", "File size: " << fileSize << " bytes");

        // Step 8: Check for cancellation again before heavy operations
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, UPLOAD_CANCELLED);
            return;
        }

        // Step 9: Create S3 client using helper function
        Aws::S3::S3Client s3Client = createS3Client(accessKey, secretKey, sessionToken, region);

        // Step 10: Create S3 PutObject request
        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bucketName);
        request.SetKey(objectKey);

        // Step 11: Final cancellation check before upload
        if (progress->shouldCancel.load()) {
            manager.updateProgress(uploadId, UPLOAD_CANCELLED);
            return;
        }

        // Step 12: Open file stream for reading
        auto inputData = Aws::MakeShared<Aws::FStream>("PutObjectInputStream",
                                                       localFilePath.c_str(),
                                                       std::ios_base::in | std::ios_base::binary);

        if (!inputData->is_open()) {
            manager.updateProgress(uploadId, UPLOAD_FAILED, "Cannot open file for reading");
            return;
        }

        // Step 13: Set request body and content type
        request.SetBody(inputData);
        request.SetContentType("application/octet-stream");

        AWS_LOGSTREAM_INFO("S3Upload", "Starting S3 PutObject operation...");

        // Step 14: Execute S3 upload with retry mechanism (up to 3 retries on failure)
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

        // Step 15: Handle final upload result
        if (uploadSuccess) {
            progress->endTime = std::chrono::steady_clock::now();
            manager.updateProgress(uploadId, UPLOAD_SUCCESS);
            AWS_LOGSTREAM_INFO("S3Upload", "Async upload SUCCESS for ID: " << uploadId);
        } else {
            manager.updateProgress(uploadId, UPLOAD_FAILED, finalErrorMsg);
            AWS_LOGSTREAM_ERROR("S3Upload", "Async upload FAILED for ID: " << uploadId << " after " << (MAX_UPLOAD_RETRIES + 1) << " attempts - " << finalErrorMsg);
        }

    } catch (const std::exception& e) {
        // Step 16: Handle exceptions during upload
        std::string errorMsg = "Upload failed with exception: " + std::string(e.what());
        manager.updateProgress(uploadId, UPLOAD_FAILED, errorMsg);
        AWS_LOGSTREAM_ERROR("S3Upload", "Exception in async upload: " << e.what());
    } catch (...) {
        // Step 17: Handle unknown exceptions
        manager.updateProgress(uploadId, UPLOAD_FAILED, "Unknown error");
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
        String uploadId = "async_upload_" + std::to_string(timestamp);

        // Step 4: Register upload with manager for progress tracking
        AsyncUploadManager::getInstance().addUpload(uploadId, localFilePath, objectKey);

        // Step 5: Convert C-style parameters to C++ strings (avoid pointer lifetime issues)
        String strAccessKey = accessKey;
        String strSecretKey = secretKey;
        String strSessionToken = sessionToken ? sessionToken : "";
        String strRegion = region;
        String strBucketName = bucketName;
        String strObjectKey = objectKey;
        String strLocalFilePath = localFilePath;

        // Step 6: Start background thread for async upload
        std::thread uploadThread(asyncUploadWorker, uploadId,
                               strAccessKey, strSecretKey, strSessionToken,
                               strRegion, strBucketName, strObjectKey, strLocalFilePath);
        uploadThread.detach(); // Detach thread so it runs independently

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
