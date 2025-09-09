Option Explicit

' FileLib.bas - System file operations used in demo.bas
' This module contains only the file system functions actually used in the demo
' Required references: Microsoft Scripting Runtime (for FileSystemObject)

' Check if file or folder exists using file attributes
' Check if file or folder exists using file attributes
Public Function FileOrFolderExists(ByVal path As String) As Boolean
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
Public Function GetFileName(ByVal filePath As String) As String
    Dim fso As Object
    Set fso = CreateObject("Scripting.FileSystemObject")
    GetFileName = fso.GetFileName(filePath)
    Set fso = Nothing
End Function

' Get file size in bytes using FileSystemObject
Public Function GetS3FileSize(ByVal filePath As String) As Long
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
Public Function FileExists(ByVal filePath As String) As Long
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
