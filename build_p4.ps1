param([ValidateSet('e6', 'ov9281')][string]$Sensor = 'e6')
$ErrorActionPreference = 'Stop'
$env:PROCESSOR_ARCHITECTURE = 'AMD64'
. C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1
$buildDirectory = "build_$Sensor"
idf.py -B $buildDirectory -D "SDKCONFIG=sdkconfig.$Sensor" -D "P4SCAN_PROJECT_NAME=agv_scan_$Sensor" build
if ($LASTEXITCODE -ne 0) { throw "$Sensor build failed: $LASTEXITCODE" }
