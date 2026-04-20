@echo off
setlocal EnableDelayedExpansion

REM Switch to script directory
cd /d "%~dp0"

REM Set version and release directory
set VERSION=v1.0.8
set RELEASE_DIR=release-%VERSION%

echo ========================================
echo PonyOffice Build and Release Script %VERSION%
echo ========================================
echo Current directory: %CD%
echo.

REM Set Qt and MinGW paths
set QT_PATH=D:\Qt\5.15.2\mingw81_64
set MINGW_PATH=D:\Qt\Tools\mingw810_64

REM IMPORTANT: Add MinGW to PATH for g++ compiler
set PATH=%MINGW_PATH%\bin;%QT_PATH%\bin;%PATH%

echo [PATH] Added to PATH: %MINGW_PATH%\bin
echo.

echo [1/7] Clean and create build directory...
if exist build (
    rd /s /q build 2>nul
    timeout /t 2 /nobreak >nul
)
mkdir build >nul 2>&1
if not exist build (
    echo Failed to create build directory!
    pause
    exit /b 1
)
cd build

echo [2/7] Run qmake...
call "%QT_PATH%\bin\qmake.exe" "..\PonyWork.pro"
if errorlevel 1 (
    echo qmake failed!
    pause
    exit /b 1
)

echo [3/7] Clean previous build artifacts...
call "%MINGW_PATH%\bin\mingw32-make.exe" clean 2>nul

echo [4/7] Build release version - please wait...
echo This may take a few minutes...
call "%MINGW_PATH%\bin\mingw32-make.exe" release
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo [4.1/7] Build Updater...
cd ..
mkdir build\Updater >nul 2>&1
cd build\Updater
call "%QT_PATH%\bin\qmake.exe" "..\..\Updater\Updater.pro"
if errorlevel 1 (
    echo Updater qmake failed!
    pause
    exit /b 1
)
call "%MINGW_PATH%\bin\mingw32-make.exe" release
if errorlevel 1 (
    echo Updater build failed!
    pause
    exit /b 1
)
cd ..\..

echo [5/7] Check executable file...
if not exist "build\release\PonyWork.exe" (
    echo Executable file generation failed!
    pause
    exit /b 1
)
echo Main program generated successfully!

if not exist "build\Updater\release\Updater.exe" (
    echo [WARNING] Updater program was not generated
) else (
    echo Updater program generated successfully!
)

echo [6/7] Deploy application...
if exist deploy (
    rd /s /q deploy 2>nul
    timeout /t 1 /nobreak >nul
)
mkdir deploy >nul 2>&1
copy "build\release\PonyWork.exe" "deploy\" >nul
if exist "build\release\Updater.exe" copy "build\release\Updater.exe" "deploy\" >nul
if exist "build\Updater\release\Updater.exe" copy "build\Updater\release\Updater.exe" "deploy\" >nul

echo Running windeployqt...
call "%QT_PATH%\bin\windeployqt.exe" "deploy\PonyWork.exe" >nul 2>&1
if errorlevel 1 (
    echo Deployment failed!
    pause
    exit /b 1
)

echo Copy additional DLL files...
if exist "dll\libcrypto-1_1-x64.dll" copy "dll\libcrypto-1_1-x64.dll" "deploy\" >nul 2>&1
if exist "dll\libssl-1_1-x64.dll" copy "dll\libssl-1_1-x64.dll" "deploy\" >nul 2>&1
if exist "img" xcopy "img" "deploy\img\" /s /y /i >nul 2>&1

echo Copy FRPC executable...
set FRPC_FOUND=0
if exist "release\frpc.exe" (
    copy "release\frpc.exe" "deploy\" >nul 2>&1
    set FRPC_FOUND=1
)
if exist "frpc.exe" (
    if !FRPC_FOUND! EQU 0 (
        copy "frpc.exe" "deploy\" >nul 2>&1
        set FRPC_FOUND=1
    )
)
if exist "dll\frpc.exe" (
    if !FRPC_FOUND! EQU 0 (
        copy "dll\frpc.exe" "deploy\" >nul 2>&1
        set FRPC_FOUND=1
    )
)
if !FRPC_FOUND! EQU 1 (
    echo [OK] frpc.exe copied to deploy folder
) else (
    echo [WARNING] frpc.exe not found, remote desktop relay may not work
)

echo Copy app_icons folder...
if exist "app_icons" xcopy "app_icons" "deploy\app_icons\" /s /y /i >nul 2>&1

echo [7/7] Organize release files...
if exist "%RELEASE_DIR%" rd /s /q "%RELEASE_DIR%" 2>nul
move "deploy" "%RELEASE_DIR%" >nul 2>&1

REM Copy documentation files
if exist "GITHUB_RELEASE_NOTES_%VERSION%.md" copy "GITHUB_RELEASE_NOTES_%VERSION%.md" "%RELEASE_DIR%\" >nul 2>&1
if exist "README.md" copy "README.md" "%RELEASE_DIR%\" >nul 2>&1

echo.
echo ========================================
echo Build and release process completed!
echo Release directory: %RELEASE_DIR%
echo ========================================
echo.

echo [8/7] Verify release files...
if exist "%RELEASE_DIR%\PonyWork.exe" (
    echo [OK] Main program exists
) else (
    echo [ERROR] Main program does not exist!
)

if exist "%RELEASE_DIR%\Updater.exe" (
    echo [OK] Updater program exists
) else (
    echo [WARNING] Updater program does not exist
)

echo.
echo Build and release script execution completed!
echo You can now manually publish to GitHub.
echo.
pause
