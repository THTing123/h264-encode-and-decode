[Setup]
#ifdef WIN64
ArchitecturesInstallIn64BitMode=x64
#endif
AppName=TigerVNC Server
AppVerName=TigerVNC Server v1.13.80 ()
AppVersion=1.13.80
AppPublisher=TigerVNC project
AppPublisherURL=https://tigervnc.org
DefaultDirName={pf}\TigerVNC Server
DefaultGroupName=TigerVNC Server
LicenseFile=C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug\LICENCE.TXT

[Dirs]
; This directory is necessary to prevent the X509 file chooser from causing
; an error dialog to appear when GetOpenFileName is called by SYSTEM account.
Name: "{sys}\config\systemprofile\Desktop"

[Files]
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug/build\win\winvnc\winvnc4.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace; 
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug/build\win\wm_hooks\wm_hooks.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace; 
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug/build\win\vncconfig\vncconfig.exe"; DestDir: "{app}"; Flags: ignoreversion restartreplace; 
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug\README.rst"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:/Users/tthou/Desktop/VNC/tigervnc-master_x264_debug\LICENCE.TXT"; DestDir: "{app}"; Flags: ignoreversion


[Icons]
Name: "{group}\VNC Server (User-Mode)\Run VNC Server"; FileName: "{app}\winvnc4.exe"; Parameters: "-noconsole";
Name: "{group}\VNC Server (User-Mode)\Configure VNC Server"; FileName: "{app}\vncconfig.exe"; Parameters: "-user";

Name: "{group}\VNC Server (Service-Mode)\Configure VNC Service"; FileName: "{app}\vncconfig.exe"; Parameters: "-noconsole -service";
Name: "{group}\VNC Server (Service-Mode)\Register VNC Service"; FileName: "{app}\winvnc4.exe"; Parameters: "-register";
Name: "{group}\VNC Server (Service-Mode)\Unregister VNC Service"; FileName: "{app}\winvnc4.exe"; Parameters: "-unregister";
Name: "{group}\VNC Server (Service-Mode)\Start VNC Service"; FileName: "{app}\winvnc4.exe"; Parameters: "-noconsole -start";
Name: "{group}\VNC Server (Service-Mode)\Stop VNC Service"; FileName: "{app}\winvnc4.exe"; Parameters: "-noconsole -stop";
Name: "{group}\License"; FileName: "write.exe"; Parameters: "LICENCE.TXT"; WorkingDir: "{app}"; Flags: "useapppaths"
Name: "{group}\Read Me"; FileName: "write.exe"; Parameters: "README.rst"; WorkingDir: "{app}"; Flags: "useapppaths"
Name: "{group}\Uninstall TigerVNC Server"; FileName: "{uninstallexe}"; WorkingDir: "{app}";

[Tasks]
Name: installservice; Description: "&Register new TigerVNC Server as a system service"; GroupDescription: "Server configuration:"; 
Name: startservice; Description: "&Start or restart TigerVNC service"; GroupDescription: "Server configuration:";

[Run]
Filename: "{app}\winvnc4.exe"; Parameters: "-register"; Tasks: installservice
Filename: "net"; Parameters: "start winvnc4"; Tasks: startservice

[Code]

{--- IShellLink ---}

const
  CLSID_ShellLink = '{00021401-0000-0000-C000-000000000046}';
  SLDF_RUNAS_USER = $2000;

type
  IShellLinkW = interface(IUnknown)
    '{000214F9-0000-0000-C000-000000000046}'
    procedure Dummy;
    procedure Dummy2;
    procedure Dummy3;
    function GetDescription(pszName: String; cchMaxName: Integer): HResult;
    function SetDescription(pszName: String): HResult;
    function GetWorkingDirectory(pszDir: String; cchMaxPath: Integer): HResult;
    function SetWorkingDirectory(pszDir: String): HResult;
    function GetArguments(pszArgs: String; cchMaxPath: Integer): HResult;
    function SetArguments(pszArgs: String): HResult;
    function GetHotkey(var pwHotkey: Word): HResult;
    function SetHotkey(wHotkey: Word): HResult;
    function GetShowCmd(out piShowCmd: Integer): HResult;
    function SetShowCmd(iShowCmd: Integer): HResult;
    function GetIconLocation(pszIconPath: String; cchIconPath: Integer;
      out piIcon: Integer): HResult;
    function SetIconLocation(pszIconPath: String; iIcon: Integer): HResult;
    function SetRelativePath(pszPathRel: String; dwReserved: DWORD): HResult;
    function Resolve(Wnd: HWND; fFlags: DWORD): HResult;
    function SetPath(pszFile: String): HResult;
  end;

  IShellLinkDataList = interface(IUnknown)
    '{45E2B4AE-B1C3-11D0-B92F-00A0C90312E1}'
    function AddDataBlock(pDataBlock : DWORD) : HResult;
    function CopyDataBlock(dwSig : DWORD; var ppDataBlock : DWORD) : HResult;
    function RemoveDataBlock(dwSig : DWORD) : HResult;
    function GetFlags(var pdwFlags : DWORD) : HResult;
    function SetFlags(dwFlags : DWORD) : HResult;
  end;

  IPersist = interface(IUnknown)
    '{0000010C-0000-0000-C000-000000000046}'
    function GetClassID(var classID: TGUID): HResult;
  end;

  IPersistFile = interface(IPersist)
    '{0000010B-0000-0000-C000-000000000046}'
    function IsDirty: HResult;
    function Load(pszFileName: String; dwMode: Longint): HResult;
    function Save(pszFileName: String; fRemember: BOOL): HResult;
    function SaveCompleted(pszFileName: String): HResult;
    function GetCurFile(out pszFileName: String): HResult;
  end;

var
  OSVersion: TWindowsVersion;

function InitializeSetup: Boolean;
begin
  GetWindowsVersionEx(OSVersion);
  MsgBox('TigerVNC Windows Server is currently unmaintained and may not function correctly.', mbError, MB_OK);
  Result := True;
end;

procedure SetRunAsUserFlag(Path: String);
var
  Obj: IUnknown;
  SL: IShellLinkW;
  SDL: IShellLinkDataList;
  PF: IPersistFile;
  Flags: DWord;
begin
  Obj := CreateComObject(StringToGuid(CLSID_ShellLink));
  SL := IShellLinkW(Obj);
  PF := IPersistFile(Obj);
  SDL := IShellLinkDataList(Obj);
  Path := ExpandConstant(Path);
  OleCheck(PF.Load(Path, 0));
  OleCheck(SDL.GetFlags(Flags));
  OleCheck(SDL.SetFlags(Flags or SLDF_RUNAS_USER));
  OleCheck(PF.Save(Path, True));
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Flags: DWord;
begin
  { Post-install actions on Windows Vista and higher:
    o Modify Service-Mode start menu commands so they run as administrator.
    o Set up the SoftwareSASGeneration system policy so as to allow services to simulate Ctrl+Alt+Del. }
  if (CurStep = ssPostInstall) and (OSVersion.Major >= 6) then begin
    SetRunAsUserFlag('{group}\VNC Server (Service-Mode)\Configure VNC Service.lnk');
    SetRunAsUserFlag('{group}\VNC Server (Service-Mode)\Register VNC Service.lnk');
    SetRunAsUserFlag('{group}\VNC Server (Service-Mode)\Unregister VNC Service.lnk');
    SetRunAsUserFlag('{group}\VNC Server (Service-Mode)\Start VNC Service.lnk');
    SetRunAsUserFlag('{group}\VNC Server (Service-Mode)\Stop VNC Service.lnk');
    if not RegQueryDWordValue(
      HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System',
      'SoftwareSASGeneration', Flags
    ) then Flags := 0;
    RegWriteDWordValue(
      HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System',
      'SoftwareSASGeneration', Flags or 1
    );
  end;
end;  
