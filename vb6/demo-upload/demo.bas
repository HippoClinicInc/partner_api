Option Explicit

' Required references:
' Project -> References -> Add the following libraries
' - Microsoft WinHTTP Services 5.1
' - Microsoft Scripting Runtime (for Dictionary object)

' Required files:
' Project -> Add file -> Add the following files
' - JsonConverter.bas
' - S3UploadLib.bas

Private Const ENV_URL As String = "https://hippoclinic.com"
' You need to change to your account when testing this.
Private Const LOGIN_ACCOUNT As String = "2546566177@qq.com"
Private Const LOGIN_ACCOUNT_PASSWORD As String = "u3LJ2lXv"
Private Const S3_BUCKET As String = "hippoclinic-staging"
Private Const S3_REGION As String = "us-west-1"
' Module-level constant for default Medical Record Number (MRN)
' This hardcoded value is used for demonstration purposes only
Private Const DEFAULT_MRN As String = "123"
Private Const DEFAULT_PATIENT_NAME As String = "Test api"
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
    Dim sdkInitResult As Long
    
    ' 1. Get file path from user input and validate existence
    uploadFilePath = InputBox("Please enter the file path to upload:", "File Upload", "")
    uploadFilePath = Trim(uploadFilePath)
    If Len(uploadFilePath) > 1 Then
        If (Left(uploadFilePath, 1) = """" And Right(uploadFilePath, 1) = """") Or _
           (Left(uploadFilePath, 1) = "'" And Right(uploadFilePath, 1) = "'") Then
            uploadFilePath = Mid(uploadFilePath, 2, Len(uploadFilePath) - 2)
        End If
        ' 1.1. Validate file/folder exists
        Debug.Print "Validating path: " & uploadFilePath
        If Not FileOrFolderExists(uploadFilePath) Then
            Debug.Print "ERROR: Path does not exist: " & uploadFilePath
            Exit Sub
        End If
        Debug.Print "Path validation successful"
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
    Debug.Print "Login successful - Hospital ID: " & hospitalId
    
    ' 3. Create patient record
    If Not CreatePatient(jwtToken, hospitalId, patientId) Then
        Debug.Print "ERROR: Failed to create patient"
        Exit Sub
    End If
    Debug.Print "Patient created - ID: " & patientId
    
    ' 4. Get S3 credentials for file upload
    If Not GetS3Credentials(jwtToken, patientId, s3Credentials, s3ExpirationTimestamp) Then
        Debug.Print "ERROR: Failed to get S3 credentials"
        Exit Sub
    End If
    Debug.Print "S3 credentials obtained"
    
    ' 5. Generate unique data ID
    If Not GenerateDataId(jwtToken, dataId) Then
        Debug.Print "ERROR: Failed to generate data ID"
        Exit Sub
    End If
    Debug.Print "Data ID generated: " & dataId
    
    ' 6. Use the AWS SDK to update file or folder to S3.
    sdkInitResult = InitializeAwsSDK()
    If sdkInitResult <> 0 Then
        Debug.Print "ERROR: AWS SDK initialization failed - code: " & sdkInitResult
        Exit Sub
    End If
    Debug.Print "AWS SDK initialized"

    ' Determine upload type and execute upload
    isFolder = IsPathFolder(uploadFilePath)
    If isFolder Then
        ' 6.1. Upload folder contents
        Debug.Print "Processing folder upload"
        uploadSuccess = UploadFolderContents(uploadFilePath, jwtToken, hospitalId, patientId, s3Credentials, s3ExpirationTimestamp, dataId, uploadDataName, totalFileSize)
    Else
        ' 6.2. Upload single file
        Debug.Print "Processing single file upload"
        uploadSuccess = UploadSingleFile(uploadFilePath, jwtToken, patientId, s3Credentials, dataId)
        If uploadSuccess Then
            uploadDataName = GetFileName(uploadFilePath)
            totalFileSize = GetS3FileSize(uploadFilePath)
        End If
    End If
    
    ' 7. Call the backend to insert a record for the uploaded file.
    If uploadSuccess Then
        Debug.Print "Upload successful, confirming with API"
        If ConfirmUploadRawFile(jwtToken, dataId, uploadDataName, patientId, totalFileSize) Then
            Debug.Print "Upload confirmation successful"
            Debug.Print "SUCCESS: Upload completed - Patient: " & patientId & ", Data: " & dataId & ", Size: " & totalFileSize & " bytes"
        Else
            Debug.Print "ERROR: Upload confirmation failed"
        End If
    Else
        Debug.Print "ERROR: Upload process failed"
    End If
    
    ' 8. Cleanup resources
    CleanupAwsSDK
    Debug.Print "AWS SDK cleanup completed"
    Debug.Print "Workflow completed"
End Sub

' Check if file or folder exists using file attributes
Private Function FileOrFolderExists(ByVal path As String) As Boolean
    On Error GoTo ErrorHandler
    
    ' 1. Get file/folder attributes
    Dim attr As Long
    attr = GetAttr(path)
    
    ' 2. Check if it's a directory
    If (attr And vbDirectory) = vbDirectory Then
        Debug.Print "Directory found: " & path
        FileOrFolderExists = True
        Exit Function
    End If
    
    ' 3. If attributes retrieved successfully, it's a file
    Debug.Print "File found: " & path
    FileOrFolderExists = True
    Exit Function
    
ErrorHandler:
    ' 4. Error getting attributes means path doesn't exist
    Debug.Print "ERROR: Path not found: " & path & " - " & Err.Description
    FileOrFolderExists = False
End Function

' Authenticate user and retrieve JWT token and hospital ID
Private Function LoginAndGetToken(ByRef jwtToken As String, ByRef hospitalId As String) As Boolean
    Dim http As Object
    Dim url As String
    Dim requestBody As String
    Dim response As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build login request
    url = ENV_URL & "/hippo/thirdParty/user/login"
    requestBody = "{""userMessage"":{""email"":""" & LOGIN_ACCOUNT & """},""password"":""" & LOGIN_ACCOUNT_PASSWORD & """}"
    
    On Error GoTo ErrorHandler
    
    ' 3. Send login request
    Debug.Print "Sending login request"
    http.Open "POST", url, False
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 4. Process response
    response = http.ResponseText
    Debug.Print "Login response status: " & http.Status
    
    ' 5. Extract token and hospital ID from JSON response
    On Error GoTo JsonError
    Dim jsonResponse As Object
    Set jsonResponse = JsonConverter.ParseJson(response)
    
    jwtToken = jsonResponse("data")("jwtToken")
    hospitalId = jsonResponse("data")("userInfo")("hospitalId")
    On Error GoTo ErrorHandler
    
    ' 6. Validate extracted data
    LoginAndGetToken = (jwtToken <> "" And hospitalId <> "")
    Set http = Nothing
    Exit Function
    
JsonError:
    ' 7. Handle JSON parsing errors
    Debug.Print "ERROR: JSON parsing failed - " & Err.Description
    jwtToken = ""
    hospitalId = ""
    On Error GoTo ErrorHandler
    
ErrorHandler:
    ' 8. Handle general errors
    Debug.Print "ERROR: Login failed - " & Err.Description
    LoginAndGetToken = False
    Set http = Nothing
End Function

' Create patient record and return patient ID
Private Function CreatePatient(ByVal jwtToken As String, ByVal hospitalId As String, ByRef patientId As String) As Boolean
    Dim http As Object
    Dim url As String
    Dim requestBody As String
    Dim response As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build patient creation request
    url = ENV_URL & "/hippo/thirdParty/queryOrCreatePatient"
    requestBody = "{""hospitalId"":""" & hospitalId & """,""user"":{""name"":""" & DEFAULT_PATIENT_NAME & """,""roles"":[3],""hospitalId"":""" & hospitalId & """,""mrn"":""" & DEFAULT_MRN & """}}"
    
    On Error GoTo ErrorHandler
    
    ' 3. Send patient creation request
    Debug.Print "Creating patient record"
    http.Open "POST", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 4. Process response
    response = http.ResponseText
    Debug.Print "Create patient response status: " & http.Status
    
    ' 5. Extract patient ID from JSON response
    On Error GoTo JsonError
    Dim jsonResponse As Object
    Set jsonResponse = JsonConverter.ParseJson(response)
    
    patientId = jsonResponse("data")("patientId")
    On Error GoTo ErrorHandler
    
    ' 6. Validate patient ID
    CreatePatient = (patientId <> "")
    Set http = Nothing
    Exit Function
    
JsonError:
    ' 7. Handle JSON parsing errors
    Debug.Print "ERROR: JSON parsing failed - " & Err.Description
    patientId = ""
    On Error GoTo ErrorHandler
    
ErrorHandler:
    ' 8. Handle general errors
    Debug.Print "ERROR: Patient creation failed - " & Err.Description
    CreatePatient = False
    Set http = Nothing
End Function

' Retrieve S3 credentials for patient folder access
Private Function GetS3Credentials(ByVal jwtToken As String, ByVal patientId As String, ByRef s3Credentials As String, ByRef s3ExpirationTimestamp As String) As Boolean
    Dim http As Object
    Dim url As String
    Dim requestBody As String
    Dim response As String
    Dim accessKeyId As String
    Dim secretAccessKey As String
    Dim sessionToken As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build S3 credentials request
    url = ENV_URL & "/hippo/thirdParty/file/getS3Credentials"
    requestBody = "{""keyId"":""" & patientId & """,""resourceType"":2}"
    
    On Error GoTo ErrorHandler
    
    ' 3. Send S3 credentials request
    Debug.Print "Requesting S3 credentials"
    http.Open "POST", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 4. Process response
    response = http.ResponseText
    Debug.Print "S3 credentials response status: " & http.Status
    
    ' 5. Extract S3 credentials from JSON response
    If response <> "" Then
        On Error GoTo JsonError
        Dim jsonResponse As Object
        Set jsonResponse = JsonConverter.ParseJson(response)
        
        Dim credentials As Object
        Set credentials = jsonResponse("data")("amazonTemporaryCredentials")
        
        s3ExpirationTimestamp = credentials("expirationTimestampSecondsInUTC")
        accessKeyId = credentials("accessKeyId")
        secretAccessKey = credentials("secretAccessKey")
        sessionToken = credentials("sessionToken")
        
        ' 6. Store full credentials JSON
        s3Credentials = JsonConverter.ConvertToJson(credentials)
        On Error GoTo ErrorHandler
    Else
        s3ExpirationTimestamp = ""
        accessKeyId = ""
        secretAccessKey = ""
        sessionToken = ""
        s3Credentials = ""
    End If
    
    ' 7. Validate all required credentials
    GetS3Credentials = (s3ExpirationTimestamp <> "" And accessKeyId <> "" And secretAccessKey <> "" And sessionToken <> "")
    Set http = Nothing
    Exit Function
    
JsonError:
    ' 8. Handle JSON parsing errors
    Debug.Print "ERROR: JSON parsing failed - " & Err.Description
    s3ExpirationTimestamp = ""
    accessKeyId = ""
    secretAccessKey = ""
    sessionToken = ""
    s3Credentials = ""
    On Error GoTo ErrorHandler
    
ErrorHandler:
    ' 9. Handle general errors
    Debug.Print "ERROR: S3 credentials request failed - " & Err.Description
    GetS3Credentials = False
    Set http = Nothing
End Function

' Generate unique data ID for file upload
Private Function GenerateDataId(ByVal jwtToken As String, ByRef dataId As String) As Boolean
    Dim http As Object
    Dim url As String
    Dim response As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build data ID generation request
    url = ENV_URL & "/hippo/thirdParty/file/generateUniqueKey" & "/1"
    
    On Error GoTo ErrorHandler
    
    ' 3. Send data ID generation request
    Debug.Print "Generating data ID"
    http.Open "GET", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.Send
    
    ' 4. Process response
    response = http.ResponseText
    Debug.Print "Data ID response status: " & http.Status
    
    ' 5. Extract first key from keys array
    If response <> "" Then
        On Error GoTo JsonError
        Dim jsonResponse As Object
        Set jsonResponse = JsonConverter.ParseJson(response)
        
        Dim keysArray As Object
        Set keysArray = jsonResponse("data")("keys")
        
        ' 6. Get first element from array
        dataId = keysArray(1)
        On Error GoTo ErrorHandler
    Else
        dataId = ""
    End If
    
    ' 7. Validate data ID
    GenerateDataId = (dataId <> "")
    Set http = Nothing
    Exit Function
    
JsonError:
    ' 8. Handle JSON parsing errors
    Debug.Print "ERROR: JSON parsing failed - " & Err.Description
    dataId = ""
    On Error GoTo ErrorHandler
    
ErrorHandler:
    ' 9. Handle general errors
    Debug.Print "ERROR: Data ID generation failed - " & Err.Description
    GenerateDataId = False
    Set http = Nothing
End Function

' Now using custom AWS S3 implementation from AwsS3Client.bas module
' The UploadToS3 function is defined in the AwsS3Client module

' Confirm file upload with HippoClinic API after successful S3 upload
Private Function ConfirmUploadRawFile(ByVal jwtToken As String, ByVal dataId As String, ByVal uploadDataName As String, ByVal patientId As String, ByVal uploadFileSizeBytes As Long) As Boolean
    Dim http As Object
    Dim url As String
    Dim requestBody As String
    Dim response As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build API endpoint URL
    url = ENV_URL & "/hippo/thirdParty/file/confirmUploadRawFile"
    
    ' 3. Build JSON request body for upload confirmation
    Dim s3UploadFileKey As String
    s3UploadFileKey = "patient/" & patientId & "/source_data/" & dataId & "/"
    
    requestBody = "{""dataId"":""" & dataId & """,""dataName"":""" & uploadDataName & """,""fileName"":""" & s3UploadFileKey & """,""dataSize"":" & uploadFileSizeBytes & ",""patientId"":""" & patientId & """,""dataType"":20,""uploadDataName"":""" & uploadDataName & """,""isRawDataInternal"":1,""dataVersions"":[0]}"
    
    On Error GoTo ErrorHandler
    
    ' 4. Send HTTP POST request with authentication
    Debug.Print "Confirming upload with API"
    http.Open "POST", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 5. Process response
    response = http.ResponseText
    Debug.Print "Upload confirmation response status: " & http.Status
    
    ' 6. Check if response indicates success
    If http.Status = 200 And InStr(response, "error") = 0 Then
        ConfirmUploadRawFile = True
    Else
        ConfirmUploadRawFile = False
    End If
    
    Set http = Nothing
    Exit Function
    
ErrorHandler:
    ' 7. Handle errors
    Debug.Print "ERROR: Upload confirmation failed - " & Err.Description
    ConfirmUploadRawFile = False
    Set http = Nothing
End Function

' Check if path is a folder using file attributes
Public Function IsPathFolder(ByVal path As String) As Boolean
    On Error GoTo ErrorHandler
    
    ' 1. Get file/folder attributes
    Dim attr As Long
    attr = GetAttr(path)
    
    ' 2. Check if it's a directory
    IsPathFolder = ((attr And vbDirectory) = vbDirectory)
    Exit Function
    
ErrorHandler:
    ' 3. Handle errors
    Debug.Print "ERROR: Path check failed - " & Err.Description
    IsPathFolder = False
End Function

' Extract filename from full path using FileSystemObject
Private Function GetFileName(ByVal filePath As String) As String
    Dim fso As Object
    Set fso = CreateObject("Scripting.FileSystemObject")
    GetFileName = fso.GetFileName(filePath)
    Set fso = Nothing
End Function

' Get file size in bytes using FileSystemObject
Private Function GetS3FileSize(ByVal filePath As String) As Long
    Dim fso As Object
    Dim file As Object
    
    On Error GoTo ErrorHandler
    
    ' 1. Initialize file system object
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 2. Check if file exists
    If Not fso.FileExists(filePath) Then
        Debug.Print "ERROR: File does not exist: " & filePath
        GetS3FileSize = 0
        Exit Function
    End If
    
    ' 3. Get file object and return size
    Set file = fso.GetFile(filePath)
    GetS3FileSize = file.Size
    
    ' 4. Cleanup objects
    Set file = Nothing
    Set fso = Nothing
    Exit Function
    
ErrorHandler:
    ' 5. Handle errors
    Debug.Print "ERROR: Failed to get file size for " & filePath & " - " & Err.Description
    GetS3FileSize = 0
    Set file = Nothing
    Set fso = Nothing
End Function

' Check if file exists using FileSystemObject
Private Function FileExists(ByVal filePath As String) As Long
    Dim fso As Object
    
    On Error GoTo ErrorHandler
    
    ' 1. Initialize file system object
    Set fso = CreateObject("Scripting.FileSystemObject")
    
    ' 2. Check if file exists and return result
    If fso.FileExists(filePath) Then
        FileExists = 1  ' Return 1 for success
    Else
        FileExists = 0  ' Return 0 for file not found
    End If
    
    ' 3. Cleanup object
    Set fso = Nothing
    Exit Function
    
ErrorHandler:
    ' 4. Handle errors
    Debug.Print "ERROR: Failed to check file existence for " & filePath & " - " & Err.Description
    FileExists = 0
    Set fso = Nothing
End Function

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
    Debug.Print "Found " & fileCount & " files in folder"
    
    If fileCount = 0 Then
        Debug.Print "WARNING: No files found in folder"
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
        Debug.Print "Processing file " & (uploadedCount + failedCount + 1) & " of " & fileCount
        
        ' 6. Get file size before upload
        currentFileSize = GetS3FileSize(currentFile)
        
        ' 7. Upload the current file
        If UploadSingleFile(currentFile, jwtToken, patientId, s3Credentials, dataId) Then
            uploadedCount = uploadedCount + 1
            totalFileSize = totalFileSize + currentFileSize
            Debug.Print "SUCCESS: Uploaded " & currentFile & " (" & currentFileSize & " bytes)"
        Else
            failedCount = failedCount + 1
            Debug.Print "ERROR: Failed to upload " & currentFile
        End If
    Next file
    
    ' 9. Determine if upload process was successful
    ' If any files failed to upload, the entire process is failed
    If failedCount > 0 Then
        Debug.Print "ERROR: " & failedCount & " files failed to upload - upload process failed"
        UploadFolderContents = False
    Else
        Debug.Print "SUCCESS: All " & uploadedCount & " files uploaded successfully"
        UploadFolderContents = True
    End If
    
    ' 10. Display summary of upload sdkInitResults
    Debug.Print "SUMMARY: Total files: " & fileCount & ", Uploaded: " & uploadedCount & ", Failed: " & failedCount & ", Size: " & totalFileSize & " bytes"
    
    ' 11. Cleanup file system objects to free memory
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
    fileSize = GetS3FileSize(filePath)
    Debug.Print "File exists, size: " & fileSize & " bytes"
    
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
        Debug.Print "SUCCESS: File uploaded to s3://" & S3_BUCKET & "/" & objectKey
        UploadSingleFile = True
    Else
        Debug.Print "ERROR: Upload failed with code: " & code
        Debug.Print "Upload error message: " & message
        Dim s3ErrorMsg As String
        s3ErrorMsg = GetS3LastError()
        If Len(s3ErrorMsg) > 0 Then
            Debug.Print "S3 Error details: " & s3ErrorMsg
        End If
        UploadSingleFile = False
    End If
    
    Exit Function
    
ErrorHandler:
    ' 6. Handle errors
    Debug.Print "ERROR: Upload failed - " & Err.Description
    UploadSingleFile = False
End Function
