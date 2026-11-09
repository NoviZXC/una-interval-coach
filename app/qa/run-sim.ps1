# Builds (optional) and runs the Interval Coach simulator on a QA script.
#   pwsh -File qa\run-sim.ps1 -Script qa\session.txt [-Build] [-FreshSettings]
# Log: qa\out\<script>.log   Screenshots: qa\out\sim\l1\l2\l3\l4\l5\screenshots\<name>.bmp
param(
    [Parameter(Mandatory)] [string]$Script,
    [switch]$Build,
    [switch]$FreshSettings,
    [int]$TimeoutSec = 600
)
$root = Split-Path $PSScriptRoot -Parent
$gui  = Join-Path $root 'Software\Apps\TouchGFX-GUI'
$tg   = 'D:\TouchGFX\4.26.1\env\MinGW'
$env:Path = "$tg\bin;$tg\msys\1.0\bin;$tg\msys\1.0\Ruby30-x64\bin;" + [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
if (-not $env:UNA_SDK) { $env:UNA_SDK = [Environment]::GetEnvironmentVariable('UNA_SDK', 'User') }

if ($Build) {
    Push-Location $gui
    make -f simulator/gcc/Makefile -j8 2>&1 | Where-Object { $_ -match 'error' }
    $code = $LASTEXITCODE
    Pop-Location
    if ($code) { Write-Error "simulator build failed"; exit 1 }
}

# The SDK's mock file system lives at "../../../../../Output/" relative to the
# working directory (Libs/Source/Simulator/Kernel/Kernel.cpp). Run from five
# levels below qa/out/sim so it lands in qa/out/sim/Output, inside the project.
$out  = Join-Path $root 'qa\out'
$sim  = Join-Path $out 'sim'
$cwd  = Join-Path $sim 'l1\l2\l3\l4\l5'
$fs   = Join-Path $sim 'Output'
New-Item -ItemType Directory -Force $cwd, $fs | Out-Null
if ($FreshSettings) {
    Remove-Item -LiteralPath (Join-Path $fs 'settings.json') -ErrorAction SilentlyContinue
}
$name = [IO.Path]::GetFileNameWithoutExtension($Script)
$env:IC_QA_SCRIPT = (Resolve-Path $Script).Path
$env:IC_QA_LOG    = Join-Path $out "$name.log"

$p = Start-Process -FilePath (Join-Path $gui 'build\bin\simulator.exe') -WorkingDirectory $cwd -PassThru
if (-not $p.WaitForExit($TimeoutSec * 1000)) {
    Stop-Process -Id $p.Id -Force
    Write-Error "simulator did not quit within $TimeoutSec s (killed)"
    exit 1
}
"log:         $env:IC_QA_LOG"
"screenshots: $(Join-Path $cwd 'screenshots')"
"settings:    $(Join-Path $fs 'settings.json')"
exit 0
