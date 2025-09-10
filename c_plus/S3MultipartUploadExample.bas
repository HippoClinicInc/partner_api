' VB6示例：使用S3MultipartUploadLib进行断点续传上传
' 
' 这个示例展示了如何在VB6中使用新的S3 Multipart Upload库
' 支持断点续传、进度回调和并发上传

Option Explicit

' 声明DLL函数
' 注意：需要将S3MultipartUploadLib.dll放在合适的位置

' 基础函数
Private Declare Function InitializeAwsSDKMultipart Lib "S3MultipartUploadLib.dll" () As Long
Private Declare Sub CleanupAwsSDKMultipart Lib "S3MultipartUploadLib.dll" ()
Private Declare Function GetMultipartLastError Lib "S3MultipartUploadLib.dll" () As String
Private Declare Function TestMultipartLibrary Lib "S3MultipartUploadLib.dll" () As Long

' 文件操作函数
Private Declare Function FileExistsMultipart Lib "S3MultipartUploadLib.dll" (ByVal filePath As String) As Long
Private Declare Function GetFileSizeMultipart Lib "S3MultipartUploadLib.dll" (ByVal filePath As String) As Currency

' 主要上传函数
Private Declare Function MultipartUploadToS3Simple Lib "S3MultipartUploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal localFilePath As String, _
    ByVal partSizeMB As Long, _
    ByVal resumeFilePath As String _
) As Long

' 进度回调版本 - 注意：VB6中函数指针比较复杂，这里提供基础版本
Private Declare Function MultipartUploadToS3WithProgress Lib "S3MultipartUploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal localFilePath As String, _
    ByVal partSizeMB As Long, _
    ByVal resumeFilePath As String, _
    ByVal progressCallback As Long, _
    ByVal userData As Long _
) As Long

' 管理函数
Private Declare Function CancelMultipartUpload Lib "S3MultipartUploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal uploadId As String _
) As Long

Private Declare Function ListMultipartUploads Lib "S3MultipartUploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal outputBuffer As String, _
    ByVal bufferSize As Long _
) As Long

Private Declare Function CleanupMultipartUploads Lib "S3MultipartUploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKeyPrefix As String _
) As Long

' 错误码常量
Private Const S3MP_SUCCESS As Long = 0
Private Const S3MP_ERROR_INVALID_PARAMS As Long = -1
Private Const S3MP_ERROR_NOT_INITIALIZED As Long = -2
Private Const S3MP_ERROR_FILE_NOT_EXISTS As Long = -3
Private Const S3MP_ERROR_FILE_READ_ERROR As Long = -4
Private Const S3MP_ERROR_FILE_OPEN_ERROR As Long = -5
Private Const S3MP_ERROR_S3_UPLOAD_FAILED As Long = -6
Private Const S3MP_ERROR_EXCEPTION As Long = -7
Private Const S3MP_ERROR_UNKNOWN As Long = -8
Private Const S3MP_ERROR_PART_TOO_SMALL As Long = -9
Private Const S3MP_ERROR_UPLOAD_NOT_FOUND As Long = -10
Private Const S3MP_ERROR_RESUME_FAILED As Long = -11
Private Const S3MP_ERROR_BUFFER_TOO_SMALL As Long = -12

' 全局变量用于存储AWS配置
Private g_AccessKey As String
Private g_SecretKey As String
Private g_SessionToken As String
Private g_Region As String
Private g_BucketName As String

' 初始化AWS配置
Public Sub InitializeAWSConfig(accessKey As String, secretKey As String, region As String, bucketName As String, Optional sessionToken As String = "")
    g_AccessKey = accessKey
    g_SecretKey = secretKey
    g_SessionToken = sessionToken
    g_Region = region
    g_BucketName = bucketName
End Sub

' 测试库是否正常工作
Public Function TestLibrary() As Boolean
    Dim result As Long
    result = TestMultipartLibrary()
    If result = 200 Then
        Debug.Print "S3 Multipart Library test successful: " & GetMultipartLastError()
        TestLibrary = True
    Else
        Debug.Print "S3 Multipart Library test failed: " & GetMultipartLastError()
        TestLibrary = False
    End If
End Function

' 初始化AWS SDK
Public Function InitializeSDK() As Boolean
    Dim result As Long
    result = InitializeAwsSDKMultipart()
    If result = S3MP_SUCCESS Then
        Debug.Print "AWS SDK initialized successfully: " & GetMultipartLastError()
        InitializeSDK = True
    Else
        Debug.Print "Failed to initialize AWS SDK: " & GetMultipartLastError()
        InitializeSDK = False
    End If
End Function

' 清理AWS SDK
Public Sub CleanupSDK()
    CleanupAwsSDKMultipart
    Debug.Print "AWS SDK cleanup: " & GetMultipartLastError()
End Sub

' 检查文件信息
Public Function CheckFileInfo(filePath As String) As String
    Dim fileSize As Currency
    Dim exists As Long
    
    exists = FileExistsMultipart(filePath)
    If exists = 1 Then
        fileSize = GetFileSizeMultipart(filePath)
        CheckFileInfo = "File exists, size: " & FormatNumber(fileSize, 0) & " bytes"
    Else
        CheckFileInfo = "File does not exist"
    End If
End Function

' 基础Multipart上传函数
Public Function UploadFileBasic(localFilePath As String, objectKey As String, Optional partSizeMB As Long = 5) As Boolean
    Dim result As Long
    
    ' 验证参数
    If g_AccessKey = "" Or g_SecretKey = "" Or g_Region = "" Or g_BucketName = "" Then
        Debug.Print "Error: AWS configuration not initialized. Call InitializeAWSConfig first."
        UploadFileBasic = False
        Exit Function
    End If
    
    ' 检查文件是否存在
    If FileExistsMultipart(localFilePath) = 0 Then
        Debug.Print "Error: File does not exist: " & localFilePath
        UploadFileBasic = False
        Exit Function
    End If
    
    Debug.Print "Starting basic multipart upload..."
    Debug.Print "File: " & localFilePath
    Debug.Print "Object Key: " & objectKey
    Debug.Print "Part Size: " & partSizeMB & " MB"
    
    ' 执行上传
    result = MultipartUploadToS3Simple( _
        g_AccessKey, _
        g_SecretKey, _
        g_SessionToken, _
        g_Region, _
        g_BucketName, _
        objectKey, _
        localFilePath, _
        partSizeMB, _
        vbNullString _
    )
    
    If result = S3MP_SUCCESS Then
        Debug.Print "✓ Upload successful: " & GetMultipartLastError()
        UploadFileBasic = True
    Else
        Debug.Print "✗ Upload failed (code " & result & "): " & GetMultipartLastError()
        UploadFileBasic = False
    End If
End Function

' 带断点续传的上传函数
Public Function UploadFileWithResume(localFilePath As String, objectKey As String, resumeFilePath As String, Optional partSizeMB As Long = 10) As Boolean
    Dim result As Long
    
    ' 验证参数
    If g_AccessKey = "" Or g_SecretKey = "" Or g_Region = "" Or g_BucketName = "" Then
        Debug.Print "Error: AWS configuration not initialized. Call InitializeAWSConfig first."
        UploadFileWithResume = False
        Exit Function
    End If
    
    ' 检查文件是否存在
    If FileExistsMultipart(localFilePath) = 0 Then
        Debug.Print "Error: File does not exist: " & localFilePath
        UploadFileWithResume = False
        Exit Function
    End If
    
    Debug.Print "Starting multipart upload with resume capability..."
    Debug.Print "File: " & localFilePath
    Debug.Print "Object Key: " & objectKey
    Debug.Print "Resume File: " & resumeFilePath
    Debug.Print "Part Size: " & partSizeMB & " MB"
    
    ' 检查是否有续传状态文件
    If FileExistsMultipart(resumeFilePath) = 1 Then
        Debug.Print "Found resume file, will attempt to resume upload..."
    End If
    
    ' 执行上传
    result = MultipartUploadToS3Simple( _
        g_AccessKey, _
        g_SecretKey, _
        g_SessionToken, _
        g_Region, _
        g_BucketName, _
        objectKey, _
        localFilePath, _
        partSizeMB, _
        resumeFilePath _
    )
    
    If result = S3MP_SUCCESS Then
        Debug.Print "✓ Upload successful: " & GetMultipartLastError()
        UploadFileWithResume = True
    Else
        Debug.Print "✗ Upload failed (code " & result & "): " & GetMultipartLastError()
        UploadFileWithResume = False
    End If
End Function

' 列出未完成的上传
Public Function ListIncompleteUploads() As String
    Dim result As Long
    Dim buffer As String
    Dim bufferSize As Long
    
    ' 验证参数
    If g_AccessKey = "" Or g_SecretKey = "" Or g_Region = "" Or g_BucketName = "" Then
        ListIncompleteUploads = "Error: AWS configuration not initialized"
        Exit Function
    End If
    
    ' 准备缓冲区
    bufferSize = 10240  ' 10KB buffer
    buffer = Space(bufferSize)
    
    result = ListMultipartUploads( _
        g_AccessKey, _
        g_SecretKey, _
        g_SessionToken, _
        g_Region, _
        g_BucketName, _
        buffer, _
        bufferSize _
    )
    
    If result = S3MP_SUCCESS Then
        ListIncompleteUploads = Trim(buffer)
        Debug.Print "Successfully listed multipart uploads"
    Else
        ListIncompleteUploads = "Error listing uploads (code " & result & "): " & GetMultipartLastError()
    End If
End Function

' 清理所有未完成的上传
Public Function CleanupIncompleteUploads(Optional objectKeyPrefix As String = "") As Long
    Dim result As Long
    
    ' 验证参数
    If g_AccessKey = "" Or g_SecretKey = "" Or g_Region = "" Or g_BucketName = "" Then
        Debug.Print "Error: AWS configuration not initialized"
        CleanupIncompleteUploads = -1
        Exit Function
    End If
    
    Debug.Print "Cleaning up incomplete multipart uploads..."
    If objectKeyPrefix <> "" Then
        Debug.Print "Prefix filter: " & objectKeyPrefix
    End If
    
    result = CleanupMultipartUploads( _
        g_AccessKey, _
        g_SecretKey, _
        g_SessionToken, _
        g_Region, _
        g_BucketName, _
        IIf(objectKeyPrefix = "", vbNullString, objectKeyPrefix) _
    )
    
    If result >= 0 Then
        Debug.Print "✓ Cleaned up " & result & " multipart uploads"
        CleanupIncompleteUploads = result
    Else
        Debug.Print "✗ Cleanup failed (code " & result & "): " & GetMultipartLastError()
        CleanupIncompleteUploads = result
    End If
End Function

' 取消特定的上传
Public Function CancelSpecificUpload(objectKey As String, uploadId As String) As Boolean
    Dim result As Long
    
    ' 验证参数
    If g_AccessKey = "" Or g_SecretKey = "" Or g_Region = "" Or g_BucketName = "" Then
        Debug.Print "Error: AWS configuration not initialized"
        CancelSpecificUpload = False
        Exit Function
    End If
    
    Debug.Print "Cancelling multipart upload..."
    Debug.Print "Object Key: " & objectKey
    Debug.Print "Upload ID: " & uploadId
    
    result = CancelMultipartUpload( _
        g_AccessKey, _
        g_SecretKey, _
        g_SessionToken, _
        g_Region, _
        g_BucketName, _
        objectKey, _
        uploadId _
    )
    
    If result = S3MP_SUCCESS Then
        Debug.Print "✓ Upload cancelled: " & GetMultipartLastError()
        CancelSpecificUpload = True
    Else
        Debug.Print "✗ Cancel failed (code " & result & "): " & GetMultipartLastError()
        CancelSpecificUpload = False
    End If
End Function

' 错误码解释函数
Public Function GetErrorDescription(errorCode As Long) As String
    Select Case errorCode
        Case S3MP_SUCCESS
            GetErrorDescription = "Success"
        Case S3MP_ERROR_INVALID_PARAMS
            GetErrorDescription = "Invalid parameters"
        Case S3MP_ERROR_NOT_INITIALIZED
            GetErrorDescription = "AWS SDK not initialized"
        Case S3MP_ERROR_FILE_NOT_EXISTS
            GetErrorDescription = "File does not exist"
        Case S3MP_ERROR_FILE_READ_ERROR
            GetErrorDescription = "File read error"
        Case S3MP_ERROR_FILE_OPEN_ERROR
            GetErrorDescription = "File open error"
        Case S3MP_ERROR_S3_UPLOAD_FAILED
            GetErrorDescription = "S3 upload failed"
        Case S3MP_ERROR_EXCEPTION
            GetErrorDescription = "Exception occurred"
        Case S3MP_ERROR_UNKNOWN
            GetErrorDescription = "Unknown error"
        Case S3MP_ERROR_PART_TOO_SMALL
            GetErrorDescription = "Part size too small (minimum 5MB)"
        Case S3MP_ERROR_UPLOAD_NOT_FOUND
            GetErrorDescription = "Upload not found"
        Case S3MP_ERROR_RESUME_FAILED
            GetErrorDescription = "Resume failed"
        Case S3MP_ERROR_BUFFER_TOO_SMALL
            GetErrorDescription = "Buffer too small"
        Case Else
            GetErrorDescription = "Unknown error code: " & errorCode
    End Select
End Function

' 示例使用函数 - 演示完整的上传流程
Public Sub ExampleUploadFlow()
    Dim testFile As String
    Dim objectKey As String
    Dim resumeFile As String
    
    ' 配置示例（请替换为您的实际值）
    Call InitializeAWSConfig( _
        "YOUR_ACCESS_KEY", _
        "YOUR_SECRET_KEY", _
        "us-east-1", _
        "your-bucket-name", _
        "" _
    )
    
    ' 设置文件路径（请替换为实际文件路径）
    testFile = "C:\temp\large_file.zip"
    objectKey = "uploads/large_file.zip"
    resumeFile = "C:\temp\upload_resume.json"
    
    Debug.Print "=== S3 Multipart Upload Example ==="
    
    ' 1. 测试库
    If Not TestLibrary() Then
        Debug.Print "Library test failed, aborting."
        Exit Sub
    End If
    
    ' 2. 初始化SDK
    If Not InitializeSDK() Then
        Debug.Print "SDK initialization failed, aborting."
        Exit Sub
    End If
    
    ' 3. 检查文件信息
    Debug.Print "File info: " & CheckFileInfo(testFile)
    
    ' 4. 上传文件（带断点续传）
    Debug.Print ""
    Debug.Print "Starting upload with resume capability..."
    If UploadFileWithResume(testFile, objectKey, resumeFile, 10) Then
        Debug.Print "Upload completed successfully!"
    Else
        Debug.Print "Upload failed. Check error messages above."
    End If
    
    ' 5. 列出未完成的上传（可选）
    Debug.Print ""
    Debug.Print "Listing incomplete uploads:"
    Debug.Print ListIncompleteUploads()
    
    ' 6. 清理SDK
    CleanupSDK
    
    Debug.Print ""
    Debug.Print "=== Example completed ==="
End Sub
