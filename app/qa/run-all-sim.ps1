# QA.md section B/C in one go: build the simulator, run every script, check
# logs (B3/B4) and screenshots (B1/B2). Summary in qa\out\sim-report.txt.
#   pwsh -File qa\run-all-sim.ps1
$root = Split-Path $PSScriptRoot -Parent
$qa   = $PSScriptRoot
$out  = Join-Path $qa 'out'
$shots = Join-Path $out 'sim\l1\l2\l3\l4\l5\screenshots'
$fs    = Join-Path $out 'sim\Output'
$report = Join-Path $out 'sim-report.txt'
New-Item -ItemType Directory -Force $out | Out-Null
Remove-Item -LiteralPath $shots -Recurse -Force -ErrorAction SilentlyContinue
"Interval Coach simulator QA  $(Get-Date -Format 'yyyy-MM-dd HH:mm')" | Set-Content $report
function Log($s) { $s; $s | Add-Content $report }
function Sim($script, [switch]$Fresh, [switch]$Build) {
    $a = @{ Script = (Join-Path $qa $script); TimeoutSec = 600 }
    if ($Fresh) { $a.FreshSettings = $true }
    if ($Build) { $a.Build = $true }
    & (Join-Path $qa 'run-sim.ps1') @a | Out-Null
    if ($LASTEXITCODE) { Log "FAIL run $script (exit $LASTEXITCODE)" } else { Log "ran  $script" }
}

Sim 'session.txt'   -Fresh -Build
Sim 'faults.txt'
Sim 'lifecycle.txt'
Sim 'persist-1.txt' -Fresh
Copy-Item (Join-Path $fs 'settings.json') (Join-Path $out 'persist-settings.json')
Sim 'persist-2.txt'
Set-Content (Join-Path $fs 'settings.json') '{ "reps": 8, oops'
Sim 'persist-3.txt'
Sim 'store.txt'     -Fresh

Log ""
Log "== script waits"
$timeouts = Get-ChildItem $out -Filter '*.log' | Select-String 'IC\.Qa: until \S+ TIMEOUT'
Log ("{0} no 'until' timed out ({1})" -f ($(if ($timeouts) { 'FAIL' } else { 'PASS' })), (@($timeouts | ForEach-Object { "$($_.Filename): $($_.Line -replace '^.*IC\.Qa: ', '')" }) -join '; '))

Log ""
Log "== B3 logs"
Log ((python -I (Join-Path $qa 'check_log.py') (Join-Path $out 'session.log') REP_DONE=1 GO=1 SESSION_DONE=1 PAUSED=1 RESUMED=1) -join "`n")
Log ((python -I (Join-Path $qa 'check_log.py') (Join-Path $out 'faults.log') GPS_LOST=1 GPS_BACK=1 REP_DONE=2 GO=2 SESSION_DONE=0) -join "`n")

Log ""
Log "== B4 lifecycle (closed mid-rep)"
$life = Get-Content (Join-Path $out 'lifecycle.log') | Where-Object { $_ -match 'IC\.Life|APP_STOP|exiting' } | ForEach-Object { ($_ -split ': ', 2)[1] }
Log ($life -join "`n")
$stay = ($life | Select-String 'GUI_STOP, session active, staying' -Quiet)
$selfExit = ($life | Select-String 'no GUI and no session, exiting' -Quiet)
Log ("{0} service stays for the active session when the GUI stops" -f ($(if ($stay -and -not $selfExit) { 'PASS' } else { 'FAIL' })))

Log ""
Log "== B5 settings file after persist-1"
Log (Get-Content (Join-Path $out 'persist-settings.json') -Raw)

Log ""
Log "== B1/B2 screenshots"
$setup = 'L1=white,L2=white,R1=amber,R2=white'
$rep   = 'L1=none,L2=none,R1=white,R2=red'
$rest  = 'L1=none,L2=none,R1=amber,R2=red'
$hints = [ordered]@{
    'b-setup-' = $setup; 'p-' = $setup; 'c-01' = $setup
    'b-gps-wait' = 'L1=none,L2=none,R1=none,R2=white'
    'b-gps-ready' = 'L1=none,L2=none,R1=amber,R2=white'; 'c-02' = 'L1=none,L2=none,R1=amber,R2=white'
    'b-rep' = $rep; 'f-gps-lost' = $rep; 'f-rep-nohr' = $rep; 'c-03' = $rep
    'b-paused' = 'L1=none,L2=none,R1=amber,R2=red'
    'b-confirm' = 'L1=none,L2=none,R1=red,R2=white'
    'b-rest' = $rest; 'f-rest' = $rest; 'c-04' = $rest
    'b-summary-scrolled' = 'L1=white,L2=none,R1=white,R2=white'
    'b-summary' = 'L1=none,L2=white,R1=white,R2=white'
    'f-summary' = 'L1=none,L2=white,R1=white,R2=white'; 'c-05' = 'L1=none,L2=white,R1=white,R2=white'
}
$fail = 0
foreach ($f in Get-ChildItem $shots -Filter '*.bmp' | Sort-Object Name) {
    $h = ($hints.GetEnumerator() | Where-Object { $f.BaseName.StartsWith($_.Key) } | Select-Object -First 1).Value
    $res = python -I (Join-Path $qa 'check_round.py') $f.FullName --hints $h
    $ok = ($res | Select-Object -Last 1) -match 'RESULT: PASS'
    if (-not $ok) { $fail++ }
    Log ("{0} {1}" -f ($(if ($ok) { 'PASS' } else { 'FAIL' })), $f.BaseName)
    if (-not $ok) { Log (($res | Where-Object { $_ -match 'FAIL' }) -join "`n") }
}
python -I (Join-Path $qa 'contact_sheet.py') (Join-Path $out 'sim-shots.png') @((Get-ChildItem $shots -Filter '*.bmp' | Sort-Object Name).FullName) | Out-Null
Log ""
Log "screenshots failing: $fail"
Log "contact sheet: $(Join-Path $out 'sim-shots.png')"
