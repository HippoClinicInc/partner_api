#include "S3Common.h"

// Global variables
bool g_isInitialized = false;
Aws::SDKOptions g_options;

String create_response(int code, const String& message) {
    std::ostringstream oss;
    oss << "{"
        << "\"code\":" << code << ","
        << "\"message\":\"" << message << "\""
        << "}";
    return oss.str();
}

// Format error message helper function
String formatErrorMessage(const String& baseMessage, const String& detail) {
    if (detail.empty()) {
        return baseMessage;
    }
    return baseMessage + ": " + detail;
}

// Upload ID helper functions
String getUploadId(const String& dataId, long long timestamp) {
    return dataId + UPLOAD_ID_SEPARATOR + std::to_string(timestamp);
}

// Initialize AWS SDK
extern "C" S3UPLOAD_API const char* __stdcall InitializeAwsSDK() {
    if (g_isInitialized) {
        static std::string response = create_response(UPLOAD_FAILED, formatErrorMessage("AWS SDK already initialized"));
        return response.c_str();
    }

    try {
        // Set log level (can be adjusted as needed)
        g_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;

        // Initialize AWS SDK
        Aws::InitAPI(g_options);
        g_isInitialized = true;

        static std::string response = create_response(SDK_INIT_SUCCESS, "AWS SDK initialized successfully");
        return response.c_str();
    }
    catch (const std::exception& e) {
        static std::string response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to initialize AWS SDK", e.what()));
        return response.c_str();
    }
    catch (...) {
        static std::string response = create_response(UPLOAD_FAILED, formatErrorMessage("Failed to initialize AWS SDK", ErrorMessage::UNKNOWN_ERROR));
        return response.c_str();
    }
}

// Cleanup AWS SDK
extern "C" S3UPLOAD_API const char* __stdcall CleanupAwsSDK() {
    if (g_isInitialized) {
        try {
            Aws::ShutdownAPI(g_options);
            g_isInitialized = false;
            static std::string successResponse = create_response(SDK_CLEAN_SUCCESS, "AWS SDK cleaned up successfully");
            return successResponse.c_str();
        }
        catch (...) {
            static std::string unknownError = create_response(UPLOAD_FAILED, formatErrorMessage("Error during AWS SDK cleanup"));
            return unknownError.c_str();
        }
    } else {
        static std::string notInitialized = create_response(UPLOAD_FAILED, formatErrorMessage(ErrorMessage::SDK_NOT_INITIALIZED));
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

// S3 client creation helper
Aws::S3::S3Client createS3Client(const String& accessKey, 
                                const String& secretKey, 
                                const String& sessionToken, 
                                const String& region) {
    // Configure client
    AWS_LOGSTREAM_INFO("S3Upload", "Creating S3 client configuration...");
    Aws::S3::S3ClientConfiguration clientConfig;
    clientConfig.region = region;
    // 30 seconds timeout
    clientConfig.requestTimeoutMs = 30000;
    clientConfig.connectTimeoutMs = 10000;
    // 10 seconds connect timeout

    // Create AWS credentials (with Session Token)
    AWS_LOGSTREAM_INFO("S3Upload", "Creating AWS credentials...");
    Aws::Auth::AWSCredentials credentials;
    if (!sessionToken.empty()) {
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
    return Aws::S3::S3Client(credentialsProvider, nullptr, clientConfig);
}
