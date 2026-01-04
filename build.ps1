# Build script for Market Microstructure C++ Engine
# This script automates the CMake build process

Write-Host "=== Market Microstructure C++ Engine Build ===" -ForegroundColor Cyan
Write-Host ""

# Check if CMake is installed
try {
    $cmakeVersion = cmake --version 2>&1 | Select-Object -First 1
    Write-Host "[OK] CMake found: $cmakeVersion" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] CMake not found. Please install CMake from https://cmake.org/download/" -ForegroundColor Red
    exit 1
}

# Navigate to engine directory
$engineDir = Join-Path $PSScriptRoot "engine"
if (-not (Test-Path $engineDir)) {
    Write-Host "[ERROR] Engine directory not found: $engineDir" -ForegroundColor Red
    exit 1
}

Set-Location $engineDir
Write-Host "[INFO] Working directory: $engineDir" -ForegroundColor Yellow

# Create build directory
$buildDir = Join-Path $engineDir "build"
if (Test-Path $buildDir) {
    Write-Host "[INFO] Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

New-Item -ItemType Directory -Path $buildDir | Out-Null
Set-Location $buildDir

Write-Host ""
Write-Host "=== Running CMake Configuration ===" -ForegroundColor Cyan

# Configure with CMake
# cmake -DCMAKE_BUILD_TYPE=Release ..
cmake `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=gcc `
  -DCMAKE_CXX_COMPILER=g++ `
  ..

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Building Project ===" -ForegroundColor Cyan

# Build the project
cmake --build .

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Green
Write-Host ""
Write-Host "Executable location:" -ForegroundColor Cyan

# Find the executable
$exeName = "market_engine.exe"
if (Test-Path (Join-Path $buildDir "Release" $exeName)) {
    $exePath = Join-Path $buildDir "Release" $exeName
} elseif (Test-Path (Join-Path $buildDir $exeName)) {
    $exePath = Join-Path $buildDir $exeName
} else {
    $exeName = "market_engine"
    $exePath = Join-Path $buildDir $exeName
}

Write-Host "  $exePath" -ForegroundColor Yellow
Write-Host ""
Write-Host "To run the engine:" -ForegroundColor Cyan
Write-Host "  cd $buildDir" -ForegroundColor White
Write-Host "  .\market_engine.exe <path-to-event-file>" -ForegroundColor White
Write-Host ""
Write-Host "Example:" -ForegroundColor Cyan
Write-Host "  .\market_engine.exe ..\..\data\test_events.events" -ForegroundColor White
Write-Host ""
