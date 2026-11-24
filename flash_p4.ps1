param([string]$Port = 'COM63')
$ErrorActionPreference = 'Stop'
$env:PROCESSOR_ARCHITECTURE = 'AMD64'
. C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1
idf.py -B build_e6 -D SDKCONFIG=sdkconfig.e6 -p $Port flash
if ($LASTEXITCODE -ne 0) { throw "E6 flash failed: $LASTEXITCODE" }
