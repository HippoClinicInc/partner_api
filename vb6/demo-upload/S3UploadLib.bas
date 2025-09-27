' VB6 Declaration File - S3UploadLib.bas
' Usage: Add this module to your VB6 project

Option Explicit

' API Declarations - Note: DLL file must be in the same directory as the program or in system PATH
' Return type: JSON string
' The code is corresponds to the error code constants above
' { "code": 0, "message": "success" }
Declare Function InitializeAwsSDK Lib "S3UploadLib.dll" () As String

Declare Sub CleanupAwsSDK Lib "S3UploadLib.dll" ()

' Return type: JSON string
' The code is corresponds to the error code constants above
' { "code": 0, "message": "success" }
Declare Function UploadFileSync Lib "S3UploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal localFilePath As String _
) As String

' Start asynchronous upload to S3
' Return value: JSON string with upload ID on success, error on failure
Declare Function UploadFileAsync Lib "S3UploadLib.dll" ( _
    ByVal accessKey As String, _
    ByVal secretKey As String, _
    ByVal sessionToken As String, _
    ByVal region As String, _
    ByVal bucketName As String, _
    ByVal objectKey As String, _
    ByVal localFilePath As String, _
    ByVal dataId As String _
) As String

' Get upload status as byte array (safer for large responses)
' Parameters:
'   dataId: Data ID used to identify the upload
'   buffer: Byte array to receive the JSON data
'   bufferSize: Size of the buffer
' Return value: Number of bytes copied to buffer, 0 on error
Declare Function GetAsyncUploadStatusBytes Lib "S3UploadLib.dll" ( _
    ByVal dataId As String, _
    ByRef buffer As Byte, _
    ByVal bufferSize As Long _
) As Long

' Clean up uploads by dataId - removes all uploads that match the dataId prefix
' Parameters:
'   dataId: Data ID used to identify the uploads to clean up
' Return value: JSON string indicating success or failure
' { "code": 2, "message": "Successfully cleaned up X upload(s) for dataId: xxx" }
Declare Function CleanupUploadsByDataId Lib "S3UploadLib.dll" ( _
    ByVal dataId As String _
) As String