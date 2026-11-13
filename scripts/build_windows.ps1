<#
.SYNOPSIS
    Script di compilazione CMake per DaMA ChromaCosm (VST3 & Standalone).
.PARAMETER Config
    Configurazione di compilazione: "Release" (predefinito) oppure "Debug".
.PARAMETER LocalJucePath
    Percorso opzionale a un'installazione locale di JUCE (es. "C:/JUCE/JUCE").
#>
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [string]$LocalJucePath = "C:/JUCE/JUCE"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "         DaMA ChromaCosm - Dev Kit Build Utility            " -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Project Root : $projectRoot"
Write-Host "Config       : $Config"

Set-Location $projectRoot

$buildDir = Join-Path $projectRoot "build"
if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

$cmakeArgs = @("-B", "build", "-G", "Visual Studio 17 2022", "-A", "x64")
if (Test-Path $LocalJucePath) {
    Write-Host "[+] Trovata installazione locale JUCE: $LocalJucePath" -ForegroundColor Green
    $cmakeArgs += "-DJUCE_PATH=$LocalJucePath"
} else {
    Write-Host "[*] JUCE locale non rilevato in '$LocalJucePath'. Download automatico via Git (FetchContent)..." -ForegroundColor Yellow
}

Write-Host "`n[*] Configurazione CMake in corso..." -ForegroundColor Cyan
& cmake @cmakeArgs

Write-Host "`n[*] Compilazione parallela in corso ($Config)..." -ForegroundColor Cyan
& cmake --build build --config $Config --parallel 8

$vst3Path = Join-Path $buildDir "ChromaCosm_artefacts\$Config\VST3\ChromaCosm.vst3"
$exePath  = Join-Path $buildDir "ChromaCosm_artefacts\$Config\Standalone\ChromaCosm.exe"

Write-Host "`n============================================================" -ForegroundColor Green
Write-Host " [+] BUILD COMPLETATA CON SUCCESSO!" -ForegroundColor Green
Write-Host " - VST3:       $vst3Path"
Write-Host " - Standalone: $exePath"
Write-Host "============================================================" -ForegroundColor Green
