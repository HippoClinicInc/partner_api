#include "../common/S3Common.h"

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
        
        // Create S3 client using helper function
        Aws::S3::S3Client s3Client = createS3Client(
            String(accessKey),
            String(secretKey),
            sessionToken ? String(sessionToken) : "",
            String(region)
        );

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
