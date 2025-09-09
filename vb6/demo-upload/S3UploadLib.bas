' VB6 Declaration File - S3UploadLib.bas
' Usage: Add this module to your VB6 project

Option Explicit

' Error code constants
Public Const S3_SUCCESS As Long = 0
Public Const S3_ERROR_INVALID_PARAMS As Long = -1
Public Const S3_ERROR_NOT_INITIALIZED As Long = -2
Public Const S3_ERROR_FILE_NOT_EXISTS As Long = -3
Public Const S3_ERROR_FILE_READ_ERROR As Long = -4
Public Const S3_ERROR_FILE_OPEN_ERROR As Long = -5
Public Const S3_ERROR_S3_UPLOAD_FAILED As Long = -6
Public Const S3_ERROR_EXCEPTION As Long = -7
Public Const S3_ERROR_UNKNOWN As Long = -8

' API Declarations - Note: DLL file must be in the same directory as the program or in system PATH
Declare Function InitializeAwsSDK Lib "S3UploadLib.dll" () As Long
Declare Sub CleanupAwsSDK Lib "S3UploadLib.dll" ()
Declare Function GetS3LastError Lib "S3UploadLib.dll" () As String

Declare Function UploadFileToS3 Lib "S3UploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal localFilePath As String _
) As Long

Declare Function UploadFileToS3WithToken Lib "S3UploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal localFilePath As String _
) As Long

Declare Function UploadFileToS3Simple Lib "S3UploadLib.dll" ( _
    ByVal jsonConfig As String _
) As Long

Declare Function FileExists Lib "S3UploadLib.dll" ( _
    ByVal filePath As String _
) As Long

Declare Function GetS3FileSize Lib "S3UploadLib.dll" ( _
    ByVal filePath As String _
) As Long

Declare Function TestS3Library Lib "S3UploadLib.dll" () As Long

' Helper function: Get error message
Public Function GetErrorMessage(ByVal errorCode As Long) As String
    Select Case errorCode
        Case S3_SUCCESS
            GetErrorMessage = "Operation successful"
        Case S3_ERROR_INVALID_PARAMS
            GetErrorMessage = "Invalid parameters"
        Case S3_ERROR_NOT_INITIALIZED
            GetErrorMessage = "AWS SDK not initialized"
        Case S3_ERROR_FILE_NOT_EXISTS
            GetErrorMessage = "File does not exist"
        Case S3_ERROR_FILE_READ_ERROR
            GetErrorMessage = "File read error"
        Case S3_ERROR_FILE_OPEN_ERROR
            GetErrorMessage = "File open error"
        Case S3_ERROR_S3_UPLOAD_FAILED
            GetErrorMessage = "S3 upload failed"
        Case S3_ERROR_EXCEPTION
            GetErrorMessage = "Exception occurred"
        Case S3_ERROR_UNKNOWN
            GetErrorMessage = "Unknown error"
        Case Else
            GetErrorMessage = "Undefined error code: " & errorCode
    End Select
End Function