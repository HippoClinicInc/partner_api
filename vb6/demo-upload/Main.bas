Option Explicit

' Required references:
' Project -> References -> Add the following libraries
' - Microsoft WinHTTP Services 5.1
' - Microsoft Scripting Runtime (for Dictionary object)

' Required files:
' Project -> Add file -> Add the following files
' - JsonConverter.bas
' - S3UploadLib.bas
' - HippoBackend.bas
' - FileLib.bas

' S3 configuration constants
Private Const S3_BUCKET As String = "hippoclinic-staging"
Private Const S3_REGION As String = "us-west-1"

' Main function to handle file upload workflow with HippoClinic API
Sub Main()
    Dim jwtToken As String
    Dim hospitalId As String
    Dim patientId As String
    Dim s3Credentials As String
    Dim s3ExpirationTimestamp As String
    Dim dataId As String
    Dim uploadFilePath As String
    Dim isFolder As Boolean
    Dim uploadSuccess As Boolean
    Dim uploadDataName As String
    Dim totalFileSize As Long
    Dim sdkInitResult As String
    
    ' 1. Get file path from user input and validate existence
    uploadFilePath = InputBox("Please enter the file path to upload:", "File Upload", "")
    uploadFilePath = Trim(uploadFilePath)
    If Len(uploadFilePath) > 1 Then
        If (Left(uploadFilePath, 1) = """" And Right(uploadFilePath, 1) = """") Or _
           (Left(uploadFilePath, 1) = "'" And Right(uploadFilePath, 1) = "'") Then
            uploadFilePath = Mid(uploadFilePath, 2, Len(uploadFilePath) - 2)
        End If
        ' 1.1. Validate file/folder exists
        If Not FileOrFolderExists(uploadFilePath) Then
            Debug.Print "ERROR: Path does not exist: " & uploadFilePath
            Exit Sub
        End If
    End If

    If uploadFilePath = "" Then
        Debug.Print "ERROR: No file path provided"
        Exit Sub
    End If
    
    ' 2. Login and get authentication token
    If Not LoginAndGetToken(jwtToken, hospitalId) Then
        Debug.Print "ERROR: Login failed"
        Exit Sub
    End If
    
    ' 3. Create patient record
    If Not CreatePatient(jwtToken, hospitalId, patientId) Then
        Debug.Print "ERROR: Failed to create patient"
        Exit Sub
    End If
    
    ' 4. Get S3 credentials for file upload
    If Not GetS3Credentials(jwtToken, patientId, s3Credentials, s3ExpirationTimestamp) Then
        Debug.Print "ERROR: Failed to get S3 credentials"
        Exit Sub
    End If
    
    ' 5. Generate unique data ID
    If Not GenerateDataId(jwtToken, dataId) Then
        Debug.Print "ERROR: Failed to generate data ID"
        Exit Sub
    End If
    
    ' 6. Use the AWS SDK to update file or folder to S3.
    sdkInitResult = InitializeAwsSDK()
    Dim jsonResponse As Object
    Set jsonResponse = JsonConverter.ParseJson(sdkInitResult)

    If jsonResponse("code") <> 0 Then
        Debug.Print "ERROR: AWS SDK initialization failed - code: " & sdkInitResult
        Exit Sub
    End If

    ' Determine upload type and execute upload
    isFolder = IsPathFolder(uploadFilePath)
    If isFolder Then
        ' 6.1. Upload folder contents
        uploadSuccess = UploadFolderContents(uploadFilePath, jwtToken, hospitalId, patientId, s3Credentials, s3ExpirationTimestamp, dataId, uploadDataName, totalFileSize)
    Else
        ' 6.2. Upload single file
        uploadSuccess = UploadSingleFile(uploadFilePath, jwtToken, patientId, s3Credentials, dataId)
        If uploadSuccess Then
            uploadDataName = GetFileName(uploadFilePath)
            totalFileSize = GetLocalFileSize(uploadFilePath)
        End If
    End If
    
    ' 7. Call the backend to insert a record for the uploaded file.
    If uploadSuccess Then
        If ConfirmUploadRawFile(jwtToken, dataId, uploadDataName, patientId, totalFileSize) Then
            Debug.Print "SUCCESS: Upload completed"
        Else
            Debug.Print "ERROR: Upload confirmation failed"
        End If
    Else
        Debug.Print "ERROR: Upload process failed"
    End If
    
    ' 8. Cleanup resources
    CleanupAwsSDK
End Sub

' Upload all files in a folder to S3 and confirm with API
Private Function UploadFolderContents(ByVal folderPath As String, ByVal jwtToken As String, ByVal hospitalId As String, ByVal patientId As String, ByVal s3Credentials As String, ByVal s3ExpirationTimestamp As String, ByVal dataId As String, ByRef uploadDataName As String, ByRef totalFileSize As Long) As Boolean
    Dim fso As Object
    Dim folder As Object
    Dim file As Object
    Dim fileCount As Integer
    Dim uploadedCount As Integer
    Dim failedCount As Integer
    Dim currentFile As String
    Dim currentFileSize As Long
    
    ' 1. Initialize file system object for folder operations
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 2. Validate that the specified folder path exists
    If Not fso.FolderExists(folderPath) Then
        Debug.Print "ERROR: Folder does not exist: " & folderPath
        UploadFolderContents = False
        Exit Function
    End If
    
    ' 3. Get folder object and count files
    Set folder = fso.GetFolder(folderPath)
    fileCount = folder.Files.Count
    
    If fileCount = 0 Then
        Debug.Print "ERROR: No files found in folder"
        UploadFolderContents = False
        Exit Function
    End If
    
    ' 4. Initialize counters and size tracking
    uploadedCount = 0
    failedCount = 0
    totalFileSize = 0
    uploadDataName = fso.GetFolder(folderPath).Name
    
    ' 5. Loop through all files in the folder
    For Each file In folder.Files
        currentFile = file.Path
        
        ' 6. Get file size before upload
        currentFileSize = GetLocalFileSize(currentFile)
        
        ' 7. Upload the current file
        If UploadSingleFile(currentFile, jwtToken, patientId, s3Credentials, dataId) Then
            uploadedCount = uploadedCount + 1
            totalFileSize = totalFileSize + currentFileSize
        Else
            failedCount = failedCount + 1
            Debug.Print "ERROR: Failed to upload " & currentFile
        End If
    Next file
    
    ' 9. Determine if upload process was successful
    ' If any files failed to upload, the entire process is failed
    If failedCount > 0 Then
        Debug.Print "ERROR: " & failedCount & " files failed to upload"
        UploadFolderContents = False
    Else
        Debug.Print "SUCCESS: All " & uploadedCount & " files uploaded"
        UploadFolderContents = True
    End If
    
    ' 10. Cleanup file system objects to free memory
    Set file = Nothing
    Set folder = Nothing
    Set fso = Nothing
End Function

' Upload a single file to S3 using AWS SDK
Private Function UploadSingleFile(ByVal filePath As String, ByVal jwtToken As String, ByVal patientId As String, ByVal s3Credentials As String, ByVal dataId As String) As Boolean
    Dim result As Long
    Dim fileSize As Long
    Dim accessKey As String
    Dim secretKey As String
    Dim sessionToken As String
    Dim objectKey As String
    Dim credentialsObj As Object
    Dim jsonResponse As String
    
    On Error GoTo ErrorHandler
    
    ' 1. Check if file exists
    result = FileExists(filePath)
    If result <> 1 Then
        Debug.Print "ERROR: Local file does not exist: " & filePath
        UploadSingleFile = False
        Exit Function
    End If
    
    ' 2. Get file size
    fileSize = GetLocalFileSize(filePath)
    
    ' 3. Parse S3 credentials
    Set credentialsObj = JsonConverter.ParseJson(s3Credentials)
    accessKey = credentialsObj("accessKeyId")
    secretKey = credentialsObj("secretAccessKey")
    sessionToken = credentialsObj("sessionToken")
    objectKey = "patient/" & patientId & "/source_data/" & dataId & "/" & GetFileName(filePath)

    ' 4. Upload file to S3
    jsonResponse = UploadFile(accessKey, secretKey, sessionToken, S3_REGION, S3_BUCKET, objectKey, filePath)

    Dim jsonObject As Object
    Set jsonObject = JsonConverter.ParseJson(jsonResponse)
    Dim code As Long
    Dim message As String
    code = jsonObject("code")
    message = jsonObject("message")
    
    ' 5. Check upload result
    If code = 0 Then
        UploadSingleFile = True
    Else
        Debug.Print "ERROR: Upload failed - " & message
        UploadSingleFile = False
    End If
    
    Exit Function
    
ErrorHandler:
    ' 6. Handle errors
    Debug.Print "ERROR: Upload failed - " & Err.Description
    UploadSingleFile = False
End Function
