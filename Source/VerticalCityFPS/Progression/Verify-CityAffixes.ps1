[CmdletBinding()]
param(
    [string]$Editor = 'E:\UE_5.9\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$GameExecutable,
    [string]$Project = (Join-Path $PSScriptRoot '..\..\..\VerticalCityFPS.uproject'),
    [string]$EvidenceRoot = (Join-Path $env:TEMP 'VerticalCityAffixes')
)
$ErrorActionPreference = 'Stop'
$program = $Editor
$launchPrefix = @()
if ($GameExecutable) {
    $program = (Resolve-Path -LiteralPath $GameExecutable).Path
    $parent = Split-Path -Parent $program
    $stem = [IO.Path]::GetFileNameWithoutExtension($program)
    $inner = Join-Path $parent "$stem/Binaries/Win64/$stem.exe"
    if (Test-Path -LiteralPath $inner -PathType Leaf) { $program = (Resolve-Path -LiteralPath $inner).Path }
    $binary = $program
} else {
    $projectFile = (Resolve-Path -LiteralPath $Project).Path
    $launchPrefix += ('"{0}"' -f $projectFile)
    $binary = Join-Path ([IO.Path]::GetDirectoryName($projectFile)) 'Binaries/Win64/UnrealEditor-VerticalCityFPS.dll'
}
$dir = Join-Path $EvidenceRoot ('affixes-' + [guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $dir)
$argsList = $launchPrefix + @(
    '/Game/VerticalCity/Maps/L_Campaign?game=/Script/Engine.GameModeBase', '-game',
    '-CityAffixVerify', '-nullrhi', '-nosound', '-unattended', '-nosplash', '-NoP4', '-NoLiveCoding',
    ('-CityProfile="{0}"' -f (Join-Path $dir 'profile.json')), ('-abslog="{0}"' -f (Join-Path $dir 'affixes.log')),
    '-ExecCmds="Automation RunTests VerticalCity.Weapons.AffixCombat"', '-TestExit="Automation Test Queue Empty"',
    ('-ReportExportPath="{0}"' -f (Join-Path $dir 'Report'))
)
Write-Output $dir
$process = Start-Process -FilePath $program -ArgumentList $argsList -WindowStyle Hidden -PassThru
try {
    if (!$process.WaitForExit(300000)) { throw "Affix verification timed out. Inspect $dir." }
} finally {
    if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
}
$report = Get-Content -Raw -LiteralPath (Join-Path $dir 'Report/index.json') | ConvertFrom-Json
$cases = @(
    foreach ($entry in $report.tests[0].entries) {
        if ($entry.event.message -match '^CITY_AFFIX_CASE (\w+) (PASS|FAIL) (.*)$') {
            $id=$Matches[1]; $result=$Matches[2]; $measurement=$Matches[3]
            $kind = if ($measurement.StartsWith('hp=')) { 'numeric' } else { 'effect' }
            [ordered]@{id=$id; kind=$kind; result=$result; measurement=$measurement}
        }
    }
)
$numeric = @($cases | Where-Object { $_.kind -eq 'numeric' -and $_.result -eq 'PASS' }).Count
$effects = @($cases | Where-Object { $_.kind -eq 'effect' -and $_.result -eq 'PASS' }).Count
$summary = [ordered]@{
    suite='VerticalCity.Weapons.AffixCombat.AllInstances'; binarySha256=(Get-FileHash -LiteralPath $binary).Hash
    numericPassed=$numeric; numericExpected=50; effectsPassed=$effects; effectsExpected=60
    failures=@($cases | Where-Object {$_.result -ne 'PASS'} | ForEach-Object {$_.id})
    cases=$cases
}
[IO.File]::WriteAllText((Join-Path $dir 'affix-results.json'),($summary | ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
if ($process.ExitCode -ne 0 -or $report.failed -ne 0 -or $numeric -ne 50 -or $effects -ne 60 -or
    @($cases | ForEach-Object {$_.id} | Sort-Object -Unique).Count -ne 110) {
    throw "Affix checks: $numeric/50 numeric, $effects/60 effects. Inspect $dir."
}
Write-Output "Affixes verified: $numeric/50 numeric and $effects/60 effects. Evidence: $dir"

