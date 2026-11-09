# Builds and runs the Interval Coach host tests (QA.md gate G1).
# Uses the MinGW that ships with TouchGFX; override with -MinGW.
param(
    [string]$MinGW = 'D:\TouchGFX\4.26.1\env\MinGW\bin',
    [string]$GoogleTestSrc = ''   # optional local googletest checkout (skips the download)
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $env:UNA_SDK) { $env:UNA_SDK = [Environment]::GetEnvironmentVariable('UNA_SDK', 'User') }
$env:Path = "$MinGW;" + [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')

$build = Join-Path $root 'build-host'
$args = @('-G', 'MinGW Makefiles', '-S', (Join-Path $root 'Tests\Host'), '-B', $build,
          '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_C_COMPILER=gcc', '-DCMAKE_CXX_COMPILER=g++')
if ($GoogleTestSrc) { $args += "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$GoogleTestSrc" }

cmake @args | Out-Null
if ($LASTEXITCODE) { throw "configure failed" }
cmake --build $build -j 8
if ($LASTEXITCODE) { throw "build failed" }
ctest --test-dir $build --output-on-failure
exit $LASTEXITCODE
