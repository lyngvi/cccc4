; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
AppName=C and C++ Code Counter
AppVerName=CCCC <unnumbered> for Win32
AppPublisher=Tim Littlefair
AppPublisherURL=http://cccc.sourceforge.net
AppSupportURL=mailto:cccc-users@lists.sourceforge.net
AppUpdatesURL=http://cccc.sourceforge.net
DefaultDirName={pf}\CCCC
DefaultGroupName=C and C++ Code Counter
OutputBaseFilename=CCCC_SETUP
; uncomment the following line if you want your installation to run on NT 3.51 too.
; MinVersion=4,3.51

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4; Flags: unchecked

[Files]
Source: "..\w32installer\ccccwrap.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\w32installer\make_cccc_env.bat"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\cccc\cccc.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\cccc\CCCC User Guide.html"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\readme.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\changes.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\CCCC Command Line"; Filename: "{app}\ccccwrap.bat"; IconIndex: 2; WorkingDir: "{app}"
Name: "{group}\CCCC User Guide"; Filename: "{app}\CCCC User Guide.html"
Name: "{group}\readme.txt"; Filename: "{app}\readme.txt"
Name: "{group}\changes.txt"; Filename: "{app}\changes.txt"
Name: "{userdesktop}\CCCC"; Filename: "{app}\ccccwrap.bat"; MinVersion: 4,4; Tasks: desktopicon ; WorkingDir: "{app}"
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\CCCC"; Filename: "{app}\ccccwrap.bat"; IconIndex: 2; MinVersion: 4,4; Tasks: quicklaunchicon ;  WorkingDir: "{app}"
Name: "{group}\Uninstall CCCC"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\make_cccc_env.bat"; Parameters: """{app}"" ""C:\"""; StatusMsg: "Creating environment script"; Flags: shellexec
Filename: "{app}\ccccwrap.bat"; Description: "Launch CCCC"; Flags: shellexec postinstall skipifsilent

