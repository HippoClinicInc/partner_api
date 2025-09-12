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
' Return type: JSON string
' The code is corresponds to the error code constants above
' { "code": 0, "message": "success" }
Declare Function InitializeAwsSDK Lib "S3UploadLib.dll" () As String

Declare Sub CleanupAwsSDK Lib "S3UploadLib.dll" ()

' Return type: JSON string
' The code is corresponds to the error code constants above
' { "code": 0, "message": "success" }
Declare Function UploadFile Lib "S3UploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal localFilePath As String, _
    ByVal retryCount As Long _
) As String
