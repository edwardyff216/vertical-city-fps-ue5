[CmdletBinding()]
param(
    [string]$Editor = 'E:\UE_5.9\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$GameExecutable,
    [string]$Project = (Join-Path $PSScriptRoot '..\..\..\VerticalCityFPS.uproject'),
    [string]$EvidenceRoot = (Join-Path $env:TEMP 'VerticalCityProgression'),
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = 'Stop'
$program = $Editor
$launchPrefix = @()
if ($GameExecutable) {
    $program = (Resolve-Path -LiteralPath $GameExecutable).Path
    $parent = Split-Path -Parent $program
    $name = [IO.Path]::GetFileNameWithoutExtension($program)
    $inner = Join-Path $parent "$name/Binaries/Win64/$name.exe"
    if (Test-Path -LiteralPath $inner -PathType Leaf) { $program = (Resolve-Path -LiteralPath $inner).Path }
} else {
    $projectFile = (Resolve-Path -LiteralPath $Project).Path
    $launchPrefix += ('"{0}"' -f $projectFile)
}
$runDir = Join-Path $EvidenceRoot ([guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $runDir -Force)
$slot = Join-Path $runDir 'profile.json'
$source = Join-Path $runDir 'legacy.json'
function New-FixtureItem($id, $kind, $frame, $rarity, [array]$affixes) {
    [ordered]@{ id=$id; template=$kind; frame=$frame; level=1; rarity=$rarity; damage_roll=10000; affixes=@($affixes) }
}
$legacy = [ordered]@{
    version=1; active=$false; last_result=''; parts=4
    equipped=@((New-FixtureItem ('1'*32) 'rifle' 'automatic' 3 @('damage','rapid')), (New-FixtureItem ('2'*32) 'sword' 'sweep' 1 @()))
    bag=@((New-FixtureItem ('3'*32) 'rifle' 'automatic' 5 @()), (New-FixtureItem ('4'*32) 'shotgun' 'scatter' 3 @()))
}
[IO.File]::WriteAllText($source, ($legacy | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
$oldHash = (Get-FileHash -LiteralPath $source).Hash

function Start-Probe([string]$step) {
    $argsList = $launchPrefix + @(
        '/Game/VerticalCity/Maps/L_Campaign?game=/Script/Engine.GameModeBase', '-game',
        '-unattended', '-nosplash', '-nosound', '-nullrhi', '-NoLiveCoding', '-NoP4',
        ('-CityProfile="{0}"' -f $slot), ('-CityPersistenceProbe={0}' -f $step),
        ('-abslog="{0}"' -f (Join-Path $runDir "$step.log")),
        ('-ExecCmds="Automation RunTests VerticalCity.Progression.StandaloneRestart.{0}"' -f $step),
        ('-ReportExportPath="{0}"' -f (Join-Path $runDir "$step-report"))
    )
    if ($step -eq 'Recover') { $argsList += '-TestExit="Automation Test Queue Empty"' }
    Start-Process -FilePath $program -ArgumentList $argsList -WindowStyle Hidden -PassThru
}
function Wait-Marker($process, [string]$marker) {
    $limit = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $limit) {
        if (Test-Path -LiteralPath $marker -PathType Leaf) { return }
        if ($process.HasExited) { throw "Process exited before writing $marker. Inspect $runDir." }
        Start-Sleep -Milliseconds 250
    }
    throw "Timed out waiting for $marker. Inspect $runDir."
}
$writeProc = Start-Probe 'Write'
try {
    $pidFile = Join-Path $runDir 'active.pid'
    Wait-Marker $writeProc $pidFile
    $reportedPid = [int]([IO.File]::ReadAllText($pidFile))
    if ($reportedPid -ne $writeProc.Id) { throw "Process marker PID mismatch. Inspect $runDir." }
    $inFlight = Get-Content -Raw -LiteralPath ($slot+'.imported.json') | ConvertFrom-Json
    if (!$inFlight.active -or $inFlight.bag.Count -ne 3) { throw "Missing active transaction before termination." }
} finally {
    if (!$writeProc.HasExited) { $writeProc.Kill(); $writeProc.WaitForExit() }
}
$recoverProc = Start-Probe 'Recover'
try {
    Wait-Marker $recoverProc (Join-Path $runDir 'recovery.ok')
    if (!$recoverProc.WaitForExit($TimeoutSeconds*1000)) { throw "Recovery process timed out. Inspect $runDir." }
    if ($recoverProc.ExitCode -ne 0) { throw "Recovery process exit code $($recoverProc.ExitCode). Inspect $runDir." }
} finally {
    if (!$recoverProc.HasExited) { $recoverProc.Kill(); $recoverProc.WaitForExit() }
}
$report = Get-Content -Raw -LiteralPath (Join-Path $runDir 'Recover-report\index.json') | ConvertFrom-Json
if (($report.succeeded + $report.succeededWithWarnings) -lt 1 -or $report.failed -ne 0) { throw "Recovery automation report did not pass. Inspect $runDir." }
$cleanProc = Start-Probe 'Clean'
try {
    Wait-Marker $cleanProc (Join-Path $runDir 'clean.ok')
    if (!$cleanProc.WaitForExit($TimeoutSeconds*1000) -or $cleanProc.ExitCode -ne 0) { throw "Normal app exit failed. Inspect $runDir." }
} finally {
    if (!$cleanProc.HasExited) { $cleanProc.Kill(); $cleanProc.WaitForExit() }
}
$cleanReport = Get-Content -Raw -LiteralPath (Join-Path $runDir 'Clean-report/index.json') | ConvertFrom-Json
if ($cleanReport.failed -ne 0 -or ($cleanReport.succeeded + $cleanReport.succeededWithWarnings) -ne 1) { throw "Clean launch report did not pass. Inspect $runDir." }
$stored = Get-Content -Raw -LiteralPath ($slot + '.imported.json') | ConvertFrom-Json
if ($stored.active -or $stored.parts -ne 4 -or $stored.last_result -ne 'defeat') { throw 'Unexpected final profile after normal exit.' }
[ordered]@{ executable=$program; binarySha256=(Get-FileHash -LiteralPath $program).Hash; sourceSha256=$oldHash; interruptedPid=$writeProc.Id; restorePid=$recoverProc.Id; cleanPid=$cleanProc.Id; cleanExit=$cleanProc.ExitCode; profile=$slot } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runDir 'process-results.json') -Encoding UTF8
if ((Get-FileHash -LiteralPath $source).Hash -ne $oldHash) { throw 'The Godot source fixture changed.' }
Write-Output "Progression restart verified. Evidence: $runDir"


