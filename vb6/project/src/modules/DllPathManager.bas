Attribute VB_Name = "DllPathManager"
Option Explicit

' DLL Path Management Module
' Used to validate DLL file paths and existence, ensuring DLL files can be found correctly

' Windows API declarations - for setting DLL search path
Private Declare Function SetDllDirectory Lib "kernel32" Alias "SetDllDirectoryA" (ByVal lpPathName As String) As Long
Private Declare Function GetDllDirectory Lib "kernel32" Alias "GetDllDirectoryA" (ByVal nBufferLength As Long, ByVal lpBuffer As String) As Long

' Get project root directory path
Public Function GetProjectRootPath() As String
    Dim appPath As String
    appPath = App.Path
    
    ' If currently in src\modules directory, need to go back to project root
    If Right(appPath, 12) = "src\modules" Then
        GetProjectRootPath = Left(appPath, Len(appPath) - 12)
    ElseIf Right(appPath, 5) = "forms" Then
        GetProjectRootPath = Left(appPath, Len(appPath) - 10) ' src\forms
    ElseIf Right(appPath, 3) = "src" Then
        GetProjectRootPath = Left(appPath, Len(appPath) - 4)
    Else
        GetProjectRootPath = appPath
    End If
    
    ' Ensure path ends with backslash
    If Right(GetProjectRootPath, 1) <> "\" Then
        GetProjectRootPath = GetProjectRootPath & "\"
    End If
End Function

' Get full path of S3UploadLib.dll
Public Function GetS3UploadLibPath() As String
    GetS3UploadLibPath = GetProjectRootPath & "lib\S3UploadLib.dll"
End Function

' Check if DLL file exists
Public Function IsDllExists(ByVal dllPath As String) As Boolean
    On Error GoTo ErrorHandler
    IsDllExists = (Dir(dllPath) <> "")
    Exit Function
ErrorHandler:
    IsDllExists = False
End Function

' Set DLL search path
Public Function SetDllSearchPath() As Boolean
    Dim libPath As String
    Dim result As Long
    
    On Error GoTo ErrorHandler
    
    ' Get lib directory path
    libPath = GetProjectRootPath() & "lib"
    
    ' Check if lib directory exists
    If Not IsDllExists(libPath & "\S3UploadLib.dll") Then
        Debug.Print "ERROR: lib directory not found or S3UploadLib.dll missing: " & libPath
        SetDllSearchPath = False
        Exit Function
    End If
    
    ' Set DLL search path
    result = SetDllDirectory(libPath)
    If result <> 0 Then
        Debug.Print "SUCCESS: DLL search path set to: " & libPath
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

' Validate DLL files exist (simplified version)
Public Function ValidateDlls() As Boolean
    Dim libPath As String
    
    On Error GoTo ErrorHandler
    
    ' Get lib directory path
    libPath = GetProjectRootPath() & "lib"
    
    ' Check if key DLL file exists
    If IsDllExists(libPath & "\S3UploadLib.dll") Then
        Debug.Print "SUCCESS: S3UploadLib.dll found at: " & libPath
        ValidateDlls = True
    Else
        Debug.Print "ERROR: S3UploadLib.dll not found at: " & libPath
        ValidateDlls = False
    End If
    
    Exit Function
    
ErrorHandler:
    Debug.Print "ERROR: DLL validation failed - " & Err.Description
    ValidateDlls = False
End Function

' Cleanup function (maintain interface compatibility)
Public Sub CleanupDlls()
    ' Static declarations don't need manual cleanup
    Debug.Print "DLL cleanup completed (static declarations)"
End Sub
