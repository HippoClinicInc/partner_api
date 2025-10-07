VERSION 5.00
Begin VB.Form Form1 
   Caption         =   "HippoClinic File Upload"
   ClientHeight    =   3015
   ClientLeft      =   120
   ClientTop       =   465
   ClientWidth     =   4560
   LinkTopic       =   "Form1"
   ScaleHeight     =   3015
   ScaleWidth      =   4560
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton Command1 
      Caption         =   "Start Upload"
      Height          =   495
      Left            =   1612
      TabIndex        =   0
      Top             =   300
      Width           =   1335
   End
   Begin VB.Label lblStatus 
      Caption         =   "Ready to upload"
      Height          =   255
      Left            =   120
      TabIndex        =   3
      Top             =   1020
      Width           =   4335
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit



Private Sub Command1_Click()
    ' Start file upload process
    Call StartFileUpload
End Sub

' Helper function to start file upload from UI
Private Sub StartFileUpload()
    ' Call the main upload function from Main.bas
    Call Main
    
    ' Re-enable button after upload
    Command1.Enabled = True
    Command1.Caption = "Start Upload"
    lblStatus.Caption = "Upload process completed"
End Sub
