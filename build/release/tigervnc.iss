[Setup]
#ifdef WIN64
ArchitecturesInstallIn64BitMode=x64
#endif
AppName=TigerVNC
AppVerName=TigerVNC 1.13.80 ()
AppVersion=1.13.80
AppPublisher=TigerVNC project
AppPublisherURL=https://tigervnc.org
DefaultDirName={pf}\TigerVNC
DefaultGroupName=TigerVNC
LicenseFile=C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug\LICENCE.TXT

[Files]
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug/build\vncviewer\vncviewer.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace; 
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug\README.rst"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug\LICENCE.TXT"; DestDir: "{app}"; Flags: ignoreversion

#define LINGUAS
#define Lang
#sub AddLanguage
  #define Lang = FileRead(LINGUAS)
  Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug/build\po\{#Lang}.mo"; DestDir: "{app}\locale\{#Lang}\LC_MESSAGES"; DestName: "tigervnc.mo"; Flags: ignoreversion
#endsub
#for {LINGUAS = FileOpen("C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug\po\LINGUAS"); !FileEof(LINGUAS); ""} AddLanguage

[Icons]
Name: "{group}\TigerVNC Viewer"; FileName: "{app}\vncviewer.exe";
Name: "{group}\Listening TigerVNC Viewer"; FileName: "{app}\vncviewer.exe"; Parameters: "-listen";

Name: "{group}\License"; FileName: "write.exe"; Parameters: "LICENCE.TXT"; WorkingDir: "{app}"; Flags: "useapppaths"
Name: "{group}\Read Me"; FileName: "write.exe"; Parameters: "README.rst"; WorkingDir: "{app}"; Flags: "useapppaths"
Name: "{group}\Uninstall TigerVNC"; FileName: "{uninstallexe}"; WorkingDir: "{app}";
