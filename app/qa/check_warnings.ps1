# QA.md gate G3: clean .uapp build with 0 warnings from Interval Coach code.
# Allowed SDK baseline: linker "Forcing branch to absolute symbol" and the
# -Wformat warning in una-sdk/Libs/Source/Fit/RecordingMarker.cpp.
# Also proves no simulator QA hook reaches the watch build.
$root = Split-Path $PSScriptRoot -Parent
if (-not $env:UNA_SDK) { $env:UNA_SDK = [Environment]::GetEnvironmentVariable('UNA_SDK', 'User') }
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')

$build = Join-Path $root 'build'
if (Test-Path $build) { Remove-Item -LiteralPath $build -Recurse -Force }
cmake -G 'Unix Makefiles' -S (Join-Path $root 'Software\Apps\IntervalCoach-CMake') -B $build *> $null
$out = cmake --build $build -j 8 2>&1 | ForEach-Object { "$_" }
$buildOk = ($LASTEXITCODE -eq 0)

$warnings = $out | Where-Object { $_ -match 'warning:' }
$baseline = $warnings | Where-Object { $_ -match 'Forcing branch to absolute symbol' -or $_ -match 'una-sdk/Libs/Source/Fit/RecordingMarker\.cpp.*-Wformat' }
$ours     = $warnings | Where-Object { $_ -match 'IntervalCoach' -and $_ -notmatch 'Forcing branch' }
$other    = $warnings | Where-Object { $_ -notin $baseline -and $_ -notin $ours }

$uapp = Get-ChildItem (Join-Path $root 'Output') -Filter '*.uapp' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$maps = Get-ChildItem (Join-Path $root 'Output') -Filter '*.map'
$qa   = $maps | Select-String -Pattern 'qaFault|QaFault|qaScript|gIcQaShot' -List

"build:           " + ($(if ($buildOk) { 'OK' } else { 'FAILED' }))
"uapp:            " + ($(if ($uapp) { "$($uapp.Name) ($([math]::Round($uapp.Length/1KB)) KB)" } else { 'missing' }))
"baseline (SDK):  $(@($baseline).Count)"
"from our code:   $(@($ours).Count)"
"other:           $(@($other).Count)"
"QA hook symbols in watch maps: $(@($qa).Count)"
$ours + $other | Select-Object -First 20

$pass = $buildOk -and $uapp -and (@($ours).Count -eq 0) -and (@($other).Count -eq 0) -and (@($qa).Count -eq 0)
"RESULT: " + ($(if ($pass) { 'PASS' } else { 'FAIL' }))
exit ([int](-not $pass))
