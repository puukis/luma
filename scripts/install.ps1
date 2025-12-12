Param(
    [string]$InstallDir = "$env:LOCALAPPDATA\Programs\Luma",
    [string]$BinaryName = "lumac.exe",
    [string]$AliasName = "luma.exe"
)

$ErrorActionPreference = 'Stop'

function Write-Info($Message) { Write-Host "[INFO] $Message" -ForegroundColor Cyan }
function Write-Success($Message) { Write-Host "[SUCCESS] $Message" -ForegroundColor Green }
function Write-WarnMessage($Message) { Write-Host "[WARNING] $Message" -ForegroundColor Yellow }
function Write-ErrorMessage($Message) { Write-Host "[ERROR] $Message" -ForegroundColor Red }

function Fail($Message) {
    Write-ErrorMessage $Message
    exit 1
}

# Discover source directory (support remote install via curl/irm)
$SourceDir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$TempDir = $null

# Dependency checks
Write-Info "Checking dependencies..."
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Fail "cmake is not installed. Install CMake 3.12+ and try again." }
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail "git is not installed. Install Git and try again." }

# Detect Visual Studio Build Tools / compiler guidance
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue) -and -not (Get-Command clang -ErrorAction SilentlyContinue) -and -not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-WarnMessage "No C/C++ compiler detected in PATH. Install Visual Studio Build Tools or a compiler supported by CMake."
}

function Cleanup {
    if ($null -ne $TempDir -and (Test-Path $TempDir)) {
        Write-Info "Cleaning up temporary files..."
        Remove-Item -Recurse -Force $TempDir
    }
}

if (-not (Test-Path (Join-Path $SourceDir "CMakeLists.txt"))) {
    Write-WarnMessage "CMakeLists.txt not found in $SourceDir. Assuming remote installation."
    $TempDir = Join-Path ([System.IO.Path]::GetTempPath()) ("luma-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $TempDir | Out-Null
    Write-Info "Created temporary directory: $TempDir"

    try {
        git clone https://github.com/puukis/luma.git (Join-Path $TempDir "luma") | Out-Null
    }
    catch {
        Cleanup
        Fail "Failed to clone repository: $($_.Exception.Message)"
    }

    $SourceDir = Join-Path $TempDir "luma"
    Write-Success "Repository cloned successfully."
}

$BuildDir = Join-Path $SourceDir "build"
if (Test-Path $BuildDir) {
    Write-Info "Cleaning existing build directory..."
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null

Push-Location $BuildDir
try {
    Write-Info "Configuring with CMake..."
    cmake ..

    Write-Info "Building Luma..."
    cmake --build . --config Release
}
catch {
    Pop-Location
    Cleanup
    Fail "Build failed: $($_.Exception.Message)"
}

# Determine binary path (single-config or multi-config generators)
$BinaryPath = Join-Path $BuildDir "lumac.exe"
$ReleaseBinary = Join-Path $BuildDir (Join-Path "Release" "lumac.exe")
if (Test-Path $ReleaseBinary) { $BinaryPath = $ReleaseBinary }

Pop-Location

if (-not (Test-Path $BinaryPath)) {
    Cleanup
    Fail "Build completed but $BinaryName was not found."
}

Write-Success "Build successful!"

# Install
Write-Info "Installing to $InstallDir..."
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$Destination = Join-Path $InstallDir $BinaryName
Copy-Item $BinaryPath $Destination -Force

$AliasPath = Join-Path $InstallDir $AliasName
if ($AliasPath -ne $Destination) {
    try {
        Copy-Item $Destination $AliasPath -Force
        Write-Success "Created alias $AliasName pointing to $BinaryName"
    }
    catch {
        Write-WarnMessage "Failed to create alias $AliasName : $($_.Exception.Message)"
    }
}

Write-Success "Installed $BinaryName to $InstallDir"

# PATH configuration
$pathEntries = ($env:PATH -split ';') | Where-Object { $_ }
if ($pathEntries -contains $InstallDir) {
    Write-Success "$InstallDir is already in your PATH."
}
else {
    Write-WarnMessage "$InstallDir is NOT in your PATH. Adding it to the user PATH..."
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    if ([string]::IsNullOrEmpty($userPath)) {
        $newPath = $InstallDir
    }
    elseif ($userPath -split ';' -contains $InstallDir) {
        $newPath = $userPath
    }
    else {
        $newPath = "$userPath;$InstallDir"
    }

    try {
        [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
        Write-Success "Added $InstallDir to your user PATH. Restart your terminal to pick up the change."
    }
    catch {
        Write-WarnMessage "Could not update PATH automatically. Add this directory manually: $InstallDir"
    }
}

Cleanup

Write-Host ""
Write-Success "Installation complete! 🎉"
Write-Info "You can run: $(Join-Path $InstallDir $BinaryName) --help"
Write-Info "Or use the alias: $(Join-Path $InstallDir $AliasName) <file.lu>"