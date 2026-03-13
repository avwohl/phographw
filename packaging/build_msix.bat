@echo off
setlocal enabledelayedexpansion

:: -----------------------------------------------------------------------
:: build_msix.bat - Build Phograph MSIX package for Windows Store
::
:: Usage:  build_msix.bat [Release|Debug]
::         Default is Release.
::
:: Prerequisites:
::   - Visual Studio 2022 with C++ workload
::   - Windows SDK 10.0.26100.0 (or adjust SDK_VER below)
::   - CMake (included with Visual Studio)
:: -----------------------------------------------------------------------

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set ROOT=%~dp0..
set BUILD_DIR=%ROOT%\build
set STAGE_DIR=%ROOT%\build\msix_staging
set OUTPUT=%ROOT%\build\Phograph.msix

:: Windows SDK paths
set SDK_VER=10.0.26100.0
set SDK_BIN=C:\Program Files (x86)\Windows Kits\10\bin\%SDK_VER%\x64
set MAKEAPPX="%SDK_BIN%\makeappx.exe"
set MAKEPRI="%SDK_BIN%\makepri.exe"
set SIGNTOOL="%SDK_BIN%\signtool.exe"

:: Find CMake (VS 2022 bundled or system)
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    for /d %%G in ("C:\Program Files\Microsoft Visual Studio\*") do (
        if exist "%%G\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE=%%G\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        )
    )
) else (
    set CMAKE=cmake
)

echo ===== Phograph MSIX Builder =====
echo Config:  %CONFIG%
echo CMake:   %CMAKE%
echo SDK:     %SDK_VER%
echo.

:: -----------------------------------------------------------------------
:: Step 1: Build the executable
:: -----------------------------------------------------------------------
echo [1/5] Building Phograph (%CONFIG%)...
"%CMAKE%" -B "%BUILD_DIR%" "%ROOT%" >nul 2>&1
"%CMAKE%" --build "%BUILD_DIR%" --config %CONFIG%
if %errorlevel% neq 0 (
    echo ERROR: Build failed.
    exit /b 1
)

set EXE=%BUILD_DIR%\%CONFIG%\Phograph.exe
if not exist "%EXE%" (
    echo ERROR: %EXE% not found.
    exit /b 1
)
echo    Built: %EXE%

:: -----------------------------------------------------------------------
:: Step 2: Create staging directory
:: -----------------------------------------------------------------------
echo [2/5] Staging package contents...
if exist "%STAGE_DIR%" rmdir /s /q "%STAGE_DIR%"
mkdir "%STAGE_DIR%"
mkdir "%STAGE_DIR%\Assets"

:: Copy executable
copy /y "%EXE%" "%STAGE_DIR%\" >nul

:: Copy manifest
copy /y "%ROOT%\packaging\AppxManifest.xml" "%STAGE_DIR%\" >nul

:: Copy store assets
copy /y "%ROOT%\store_assets\Square44x44Logo.png"  "%STAGE_DIR%\Assets\" >nul
copy /y "%ROOT%\store_assets\Square71x71Logo.png"   "%STAGE_DIR%\Assets\" >nul
copy /y "%ROOT%\store_assets\Square150x150Logo.png" "%STAGE_DIR%\Assets\" >nul
copy /y "%ROOT%\store_assets\Square310x310Logo.png" "%STAGE_DIR%\Assets\" >nul
copy /y "%ROOT%\store_assets\Wide310x150Logo.png"   "%STAGE_DIR%\Assets\" >nul
copy /y "%ROOT%\store_assets\StoreLogo.png"          "%STAGE_DIR%\Assets\" >nul

echo    Staged to: %STAGE_DIR%

:: -----------------------------------------------------------------------
:: Step 3: Create resources.pri
:: -----------------------------------------------------------------------
echo [3/5] Creating resources.pri...
if exist %MAKEPRI% (
    pushd "%STAGE_DIR%"
    %MAKEPRI% createconfig /cf priconfig.xml /dq en-US >nul 2>&1
    %MAKEPRI% new /pr "%STAGE_DIR%" /cf priconfig.xml /of "%STAGE_DIR%\resources.pri" >nul 2>&1
    del priconfig.xml >nul 2>&1
    popd
    echo    Created resources.pri
) else (
    echo    WARNING: MakePri.exe not found - skipping resources.pri
    echo    Package may not pass Store validation without it.
)

:: -----------------------------------------------------------------------
:: Step 4: Create MSIX package
:: -----------------------------------------------------------------------
echo [4/5] Packing MSIX...
if exist "%OUTPUT%" del "%OUTPUT%"
%MAKEAPPX% pack /d "%STAGE_DIR%" /p "%OUTPUT%" /o >nul
if %errorlevel% neq 0 (
    echo ERROR: makeappx pack failed.
    exit /b 1
)
echo    Created: %OUTPUT%

:: -----------------------------------------------------------------------
:: Step 5: Sign with self-signed certificate (for sideloading/testing)
:: -----------------------------------------------------------------------
echo [5/5] Signing package...

set CERT_SUBJECT=CN=avwohl
set PFX_FILE=%ROOT%\build\Phograph_dev.pfx
set PFX_PASS=PhographDev2026

:: Create self-signed cert if .pfx doesn't exist
if not exist "%PFX_FILE%" (
    echo    Creating self-signed certificate...
    powershell -NoProfile -Command ^
        "$cert = New-SelfSignedCertificate -Type Custom -Subject '%CERT_SUBJECT%' -KeyUsage DigitalSignature -FriendlyName 'Phograph Dev Signing' -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}'); ^
         $pwd = ConvertTo-SecureString -String '%PFX_PASS%' -Force -AsPlainText; ^
         Export-PfxCertificate -Cert $cert -FilePath '%PFX_FILE%' -Password $pwd | Out-Null; ^
         Write-Host '   Certificate thumbprint:' $cert.Thumbprint"
    if %errorlevel% neq 0 (
        echo    WARNING: Could not create certificate. Package is unsigned.
        echo    For Store submission, sign with your Partner Center certificate.
        goto :done
    )
)

%SIGNTOOL% sign /fd SHA256 /a /f "%PFX_FILE%" /p "%PFX_PASS%" "%OUTPUT%" >nul 2>&1
if %errorlevel% neq 0 (
    echo    WARNING: Signing failed. Package created but unsigned.
    echo    For Store submission, sign with your Partner Center certificate.
) else (
    echo    Signed with development certificate.
)

:done
echo.
echo ===== Done =====
echo MSIX package: %OUTPUT%
echo.
echo To sideload for testing:
echo   1. Double-click %OUTPUT%
echo   2. Or: Add-AppPackage -Path "%OUTPUT%" -AllowUnsigned
echo.
echo For Windows Store submission:
echo   1. Register at https://partner.microsoft.com/dashboard
echo   2. Update packaging\AppxManifest.xml with your Store identity
echo   3. Sign with your Store certificate
echo   4. Upload the .msix to Partner Center
echo.

endlocal
