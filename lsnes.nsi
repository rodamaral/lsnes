!define PRODUCT_NAME "lsnes"
!define PRODUCT_VERSION "rr1-delta18epsilon3"
!define PRODUCT_PUBLISHER "Ilari"
!define PRODUCT_WEB_SITE "http://tasvideos.org/Lsnes.html"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

!define INSTALL_DIR "lsnes"

SetCompressor /SOLID lzma
BrandingText "${PRODUCT_NAME} ${PRODUCT_VERSION} Installer"
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${PRODUCT_NAME}-${PRODUCT_VERSION}-installer.exe"
InstallDir "$PROGRAMFILES\${INSTALL_DIR}"
InstallDirRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DIR"
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel admin ;Request elevated privileges for Windows 6.x+

; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; License page
!define MUI_LICENSEPAGE_CHECKBOX
!insertmacro MUI_PAGE_LICENSE "COPYING.GPL"
!define MUI_LICENSEPAGE_CHECKBOX
!insertmacro MUI_PAGE_LICENSE "COPYING.GPL3"
;Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\manual.txt"
!insertmacro MUI_PAGE_FINISH
 
; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES
; Language files
!insertmacro MUI_LANGUAGE "English"
; Reserve files
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

; MUI end ------

Section "Main installer files" SEC01
  SetShellVarContext all

  SetOutPath "$INSTDIR"
  File "COPYING.GPL"
  File "COPYING.GPL3"
  File "lsnesgb-sdl.exe"
  File "lsnesgb-wxwidgets.exe"
  File "lsnes-sdl.exe"
  File "lsnes-wxwidgets.exe"
  File "lsnes.nsi"
  File "manual.txt"

  File /r "buildoptions"
  File /r "data"
SectionEnd

Section -AdditionalIcons
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\${INSTALL_DIR}"
  CreateShortCut "$SMPROGRAMS\${INSTALL_DIR}\Super Nintendo Emulator.lnk" "$INSTDIR\lsnes-wxwidgets.exe"
  CreateShortCut "$SMPROGRAMS\${INSTALL_DIR}\Gameboy Emulator.lnk" "$INSTDIR\lsnesgb-wxwidgets.exe"
  WriteIniStr "$SMPROGRAMS\${INSTALL_DIR}\${PRODUCT_NAME} website.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateShortCut "$SMPROGRAMS\${INSTALL_DIR}\Manual.lnk" "$INSTDIR\manual.txt"
  CreateShortCut "$SMPROGRAMS\${INSTALL_DIR}\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  SetShellVarContext all
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DIR" "$INSTDIR"
SectionEnd


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES Done
  Abort
Done:
FunctionEnd

Section Uninstall
  SetShellVarContext all

  RMDir /R "$SMPROGRAMS\${INSTALL_DIR}"
  RMdir /R "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  SetAutoClose true
SectionEnd
