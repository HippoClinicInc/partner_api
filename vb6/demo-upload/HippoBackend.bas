Option Explicit

' HippoBackend.bas - HippoClinic backend API functions
' This module contains functions that interact with the HippoClinic backend API

Private Const ENV_URL As String = "https://hippoclinic.com"
' You need to change to your account when testing this.
Private Const LOGIN_ACCOUNT As String = "2546566177@qq.com"
Private Const LOGIN_ACCOUNT_PASSWORD As String = "u3LJ2lXv"

' Module-level constant for default Medical Record Number (MRN)
' This hardcoded value is used for demonstration purposes only
Private Const DEFAULT_MRN As String = "123"
Private Const DEFAULT_PATIENT_NAME As String = "Test api"

' Authenticate user and retrieve JWT token and hospital ID
Public Function LoginAndGetToken(ByRef jwtToken As String, ByRef hospitalId As String) As Boolean
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
    http.Open "POST", url, False
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 4. Process response
    response = http.ResponseText
    
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
    Debug.Print "ERROR: JSON parsing failed"
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
Public Function CreatePatient(ByVal jwtToken As String, ByVal hospitalId As String, ByRef patientId As String) As Boolean
    Dim http As Object
    Dim url As String
    Dim requestBody As String
    Dim response As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build patient creation request
    url = ENV_URL & "/hippo/thirdParty/queryOrCreateNatusPatient"
    requestBody = "{""hospitalId"":""" & hospitalId & """,""user"":{""name"":""" & DEFAULT_PATIENT_NAME & """,""roles"":[3],""hospitalId"":""" & hospitalId & """,""mrn"":""" & DEFAULT_MRN & """}}"
    
    On Error GoTo ErrorHandler
    
    ' 3. Send patient creation request
    http.Open "POST", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 4. Process response
    response = http.ResponseText
    
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
Public Function GetS3Credentials(ByVal jwtToken As String, ByVal patientId As String, ByRef s3Credentials As String, ByRef s3ExpirationTimestamp As String) As Boolean
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
    http.Open "POST", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 4. Process response
    response = http.ResponseText
    
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
Public Function GenerateDataId(ByVal jwtToken As String, ByRef dataId As String) As Boolean
    Dim http As Object
    Dim url As String
    Dim response As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build data ID generation request
    url = ENV_URL & "/hippo/thirdParty/file/generateUniqueKey" & "/1"
    
    On Error GoTo ErrorHandler
    
    ' 3. Send data ID generation request
    http.Open "GET", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.Send
    
    ' 4. Process response
    response = http.ResponseText
    
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
Public Function ConfirmUploadRawFile(ByVal jwtToken As String, ByVal dataId As String, ByVal uploadDataName As String, ByVal patientId As String, ByVal uploadFileSizeBytes As Long, ByVal s3FileKey As String) As Boolean
    Dim http As Object
    Dim url As String
    Dim requestBody As String
    Dim response As String
    
    ' 1. Initialize HTTP client
    Set http = CreateObject("WinHttp.WinHttpRequest.5.1")
    
    ' 2. Build API endpoint URL
    url = ENV_URL & "/hippo/thirdParty/file/confirmUploadRawFile"
    
    ' 3. Build JSON request body for upload confirmation
    requestBody = "{""dataId"":""" & dataId & """,""dataName"":""" & uploadDataName & """,""fileName"":""" & s3FileKey & """,""dataSize"":" & uploadFileSizeBytes & ",""patientId"":""" & patientId & """,""dataType"":20,""uploadDataName"":""" & uploadDataName & """,""isRawDataInternal"":1,""dataVersions"":[0]}"
    
    On Error GoTo ErrorHandler
    
    ' 4. Send HTTP POST request with authentication
    http.Open "POST", url, False
    http.SetRequestHeader "Authorization", "Bearer " & jwtToken
    http.SetRequestHeader "Content-Type", "application/json"
    http.Send requestBody
    
    ' 5. Process response
    response = http.ResponseText
    
    ' 6. Parse response JSON and validate upload status
    Dim jsonResponse As Object
    Set jsonResponse = JsonConverter.ParseJson(response)
    
    ' 7. Check HTTP status and response validity
    If http.Status <> 200 Or jsonResponse Is Nothing Then
        ConfirmUploadRawFile = False
        Set http = Nothing
        Exit Function
    End If
    
    ' 8. Check for failed uploads
    If Not IsEmpty(jsonResponse("data")("failedUploads")) Then
        If IsArray(jsonResponse("data")("failedUploads")) Or TypeName(jsonResponse("data")("failedUploads")) = "Collection" Then
            If jsonResponse("data")("failedUploads").Count > 0 Then
                Debug.Print "ERROR: Failed to create record for some files."
                ConfirmUploadRawFile = False
                Set http = Nothing
                Exit Function
            End If
        End If
    End If
    
    ' 9. All checks passed - upload successful
    ConfirmUploadRawFile = True
    
    Set http = Nothing
    Exit Function
    
ErrorHandler:
    ' 7. Handle errors
    Debug.Print "ERROR: Upload confirmation failed - " & Err.Description
    ConfirmUploadRawFile = False
    Set http = Nothing
End Function
