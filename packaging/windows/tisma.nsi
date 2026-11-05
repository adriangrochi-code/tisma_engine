; The Windows installer of Tisma (owner's request, 2026-11-05: the testers'
; beta needs one, as Tisma Slicer had). Built by NSIS (zlib/libpng licence;
; THIRD_PARTY.md) with zlib compression only.
; - Per user, in the user's own programs folder, without administrator
;   rights (app_updates.md, rule 7).
; - Start menu and, if wanted, desktop shortcuts.
; - Opening .3mf and .stl files with Tisma, offered and on by default.
; - An uninstaller that removes the program and leaves the user's profiles,
;   preferences and projects (%APPDATA%\Tisma) untouched (rule 9).
; - In Spanish, and in English for a system in English.
;
;   makensis /DVERSION=0.11.0-beta.2 /DSOURCE=<package folder> /DOUTFILE=<setup.exe> /DICON=<tisma.ico> tisma.nsi
;
; Own design; see PROVENANCE.md.

Unicode true
SetCompressor /SOLID zlib
RequestExecutionLevel user
ManifestDPIAware true

!ifndef VERSION
  !error "VERSION is needed: /DVERSION=..."
!endif
!ifndef SOURCE
  !error "SOURCE is needed: the folder of the package (/DSOURCE=...)"
!endif
!ifndef OUTFILE
  !define OUTFILE "tisma-${VERSION}-setup-windows-x64.exe"
!endif

!define APP "Tisma"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Tisma"

Name "${APP} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Tisma"
InstallDirRegKey HKCU "Software\Tisma" "InstallDir"
BrandingText "${APP} ${VERSION}"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!ifdef ICON
  !define MUI_ICON "${ICON}"
  !define MUI_UNICON "${ICON}"
!endif
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\tisma.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "English"

LangString SecProgram ${LANG_SPANISH} "Tisma (necesario)"
LangString SecProgram ${LANG_ENGLISH} "Tisma (required)"
LangString SecDesktop ${LANG_SPANISH} "Acceso directo en el escritorio"
LangString SecDesktop ${LANG_ENGLISH} "Desktop shortcut"
LangString SecFiles ${LANG_SPANISH} "Abrir los archivos .3mf y .stl con Tisma"
LangString SecFiles ${LANG_ENGLISH} "Open .3mf and .stl files with Tisma"
LangString DescProgram ${LANG_SPANISH} "El programa, el motor de laminado y el componente de Diseño."
LangString DescProgram ${LANG_ENGLISH} "The program, the slicing engine and the Design component."
LangString DescDesktop ${LANG_SPANISH} "Un acceso directo a Tisma en el escritorio."
LangString DescDesktop ${LANG_ENGLISH} "A shortcut to Tisma on the desktop."
LangString DescFiles ${LANG_SPANISH} "Hacer doble clic en un .3mf o un .stl lo abre en Tisma."
LangString DescFiles ${LANG_ENGLISH} "Double-clicking a .3mf or an .stl opens it in Tisma."
LangString FileModel ${LANG_SPANISH} "Modelo 3D"
LangString FileModel ${LANG_ENGLISH} "3D model"

Section "!$(SecProgram)" SEC_PROGRAM
  SectionIn RO
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${SOURCE}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\Tisma.lnk" "$INSTDIR\tisma.exe"
  WriteRegStr HKCU "Software\Tisma" "InstallDir" "$INSTDIR"
  ; In «Installed apps», for the current user.
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${APP}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "Tisma"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\tisma.exe"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "$(SecDesktop)" SEC_DESKTOP
  SetShellVarContext current
  CreateShortcut "$DESKTOP\Tisma.lnk" "$INSTDIR\tisma.exe"
SectionEnd

; The file types, for this user only (HKCU\Software\Classes).
Section "$(SecFiles)" SEC_FILES
  WriteRegStr HKCU "Software\Classes\Tisma.Model" "" "$(FileModel)"
  WriteRegStr HKCU "Software\Classes\Tisma.Model\DefaultIcon" "" "$INSTDIR\tisma.exe,0"
  WriteRegStr HKCU "Software\Classes\Tisma.Model\shell\open\command" "" '"$INSTDIR\tisma.exe" "%1"'
  WriteRegStr HKCU "Software\Classes\.3mf\OpenWithProgids" "Tisma.Model" ""
  WriteRegStr HKCU "Software\Classes\.stl\OpenWithProgids" "Tisma.Model" ""
  WriteRegStr HKCU "Software\Classes\.3mf" "" "Tisma.Model"
  WriteRegStr HKCU "Software\Classes\.stl" "" "Tisma.Model"
  WriteRegStr HKCU "Software\Tisma" "FileTypes" "1"
  ; Tell Explorer the associations changed (SHCNE_ASSOCCHANGED).
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_PROGRAM} $(DescProgram)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} $(DescDesktop)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_FILES} $(DescFiles)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInit
  ; Spanish unless the system is in English.
  System::Call 'kernel32::GetUserDefaultUILanguage() i .r0'
  IntOp $0 $0 & 0x3FF
  ${If} $0 = 0x09
    StrCpy $LANGUAGE ${LANG_ENGLISH}
  ${Else}
    StrCpy $LANGUAGE ${LANG_SPANISH}
  ${EndIf}
FunctionEnd

Function un.onInit
  System::Call 'kernel32::GetUserDefaultUILanguage() i .r0'
  IntOp $0 $0 & 0x3FF
  ${If} $0 = 0x09
    StrCpy $LANGUAGE ${LANG_ENGLISH}
  ${Else}
    StrCpy $LANGUAGE ${LANG_SPANISH}
  ${EndIf}
FunctionEnd

; Removes the program only: the profiles, the preferences and the projects
; in %APPDATA%\Tisma and the user's folders stay (app_updates.md, rule 9).
Section "Uninstall"
  SetShellVarContext current
  Delete "$SMPROGRAMS\Tisma.lnk"
  Delete "$DESKTOP\Tisma.lnk"
  ReadRegStr $0 HKCU "Software\Tisma" "FileTypes"
  ${If} $0 == "1"
    ReadRegStr $1 HKCU "Software\Classes\.3mf" ""
    ${If} $1 == "Tisma.Model"
      DeleteRegValue HKCU "Software\Classes\.3mf" ""
    ${EndIf}
    ReadRegStr $1 HKCU "Software\Classes\.stl" ""
    ${If} $1 == "Tisma.Model"
      DeleteRegValue HKCU "Software\Classes\.stl" ""
    ${EndIf}
    DeleteRegValue HKCU "Software\Classes\.3mf\OpenWithProgids" "Tisma.Model"
    DeleteRegValue HKCU "Software\Classes\.stl\OpenWithProgids" "Tisma.Model"
    DeleteRegKey HKCU "Software\Classes\Tisma.Model"
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
  ${EndIf}
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
  DeleteRegKey HKCU "Software\Tisma"
  RMDir /r "$INSTDIR"
SectionEnd
