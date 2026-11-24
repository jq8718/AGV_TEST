param([string]$Port = 'COM63', [ValidateSet('auto', 'e6', 'ov9281')][string]$Sensor = 'auto')
$ErrorActionPreference = 'Stop'
$env:PROCESSOR_ARCHITECTURE = 'AMD64'
. C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1
idf.py -B "build_$Sensor" -D "SDKCONFIG=sdkconfig.$Sensor" -D "P4SCAN_PROJECT_NAME=agv_scan_$Sensor" -p $Port flash
if ($LASTEXITCODE -ne 0) { throw "$Sensor flash failed: $LASTEXITCODE" }
