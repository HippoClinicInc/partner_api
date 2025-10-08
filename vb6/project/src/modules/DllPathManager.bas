Attribute VB_Name = "DllPathManager"
Option Explicit

' DLL Path Management Module
' Used to validate DLL file paths and existence, ensuring DLL files can be found correctly

' Windows API declarations - for setting DLL search path
Private Declare Function SetDllDirectory Lib "kernel32" Alias "SetDllDirectoryA" (ByVal lpPathName As String) As Long

' Get project root directory path
Private Function GetProjectRootPath() As String
    Dim appPath As String
    appPath = App.Path
    
    ' If currently in src\modules directory, go back to project root
    If Right(appPath, 12) = "src\modules" Then
        GetProjectRootPath = Left(appPath, Len(appPath) - 12)
    Else
        GetProjectRootPath = appPath
    End If
    
    ' Ensure path ends with backslash
    If Right(GetProjectRootPath, 1) <> "\" Then
        GetProjectRootPath = GetProjectRootPath & "\"
    End If
End Function

' Check if DLL file exists
Private Function IsDllExists(ByVal dllPath As String) As Boolean
    On Error GoTo ErrorHandler
    IsDllExists = (Dir(dllPath) <> "")
    Exit Function
ErrorHandler:
    IsDllExists = False
End Function

' Set DLL search path and validate DLL files exist
Public Function SetDllSearchPath() As Boolean
    Dim libPath As String
    Dim result As Long
    
    On Error GoTo ErrorHandler
    
    ' Get lib directory path
    libPath = GetProjectRootPath() & "lib"
    
    ' Check if lib directory exists and contains S3UploadLib.dll
    If Not IsDllExists(libPath & "\S3UploadLib.dll") Then
        Debug.Print "ERROR: lib directory not found or S3UploadLib.dll missing: " & libPath
        SetDllSearchPath = False
        Exit Function
    End If
    
    ' Set DLL search path
    result = SetDllDirectory(libPath)
    If result <> 0 Then
        Debug.Print "SUCCESS: DLL search path set to: " & libPath
        Debug.Print "SUCCESS: S3UploadLib.dll found and validated"
        SetDllSearchPath = True
    Else
        Debug.Print "ERROR: Failed to set DLL search path to: " & libPath
        SetDllSearchPath = False
    End If
    
    Exit Function
    
ErrorHandler:
    Debug.Print "ERROR: SetDllSearchPath failed - " & Err.Description
    SetDllSearchPath = False
End Function
