@echo off
setlocal
set "JAVA_HOME=D:\Android\jdk17"
set "ANDROID_HOME=D:\Android\Sdk"
set "ANDROID_SDK_ROOT=D:\Android\Sdk"
set "NDKROOT=D:\Android\Sdk\ndk\27.2.12479018"
set "PATH=D:\Android\jdk17\bin;D:\Android\Sdk\platform-tools;%PATH%"
set "LOG=%~dp0bcr_%1.log"
echo [wrapper] start %DATE% %TIME% > "%LOG%"
call "D:\UE5\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
 -project="D:\UE5\Project\PinWorld\PinWorld.uproject" ^
 -noP4 -platform=Android -cookflavor=ASTC -clientconfig=%1 ^
 -build -cook -stage -pak -package -archive ^
 -archivedirectory="D:\UE5\Project\PinWorld\Build\Android" ^
 -unattended -utf8output -nocompileeditor -nodebuginfo >> "%LOG%" 2>&1
echo [wrapper] UAT exit=%ERRORLEVEL% >> "%LOG%"
endlocal
