[CmdletBinding()]
param(
    [ValidateSet('Development','Shipping')][string]$Configuration='Shipping',
    [string]$EngineRoot='E:\UE_5.9',
    [string]$ArchiveRoot='D:\UE5\VerticalCityBuilds'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$project=Join-Path $root 'VerticalCityFPS.uproject'
$uat=Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if (!(Test-Path -LiteralPath $uat) -or !(Test-Path -LiteralPath $project)) {throw 'UE installation or game project missing.'}
$destination=Join-Path $ArchiveRoot $Configuration
$logDir=Join-Path $ArchiveRoot 'Evidence\Build'
[IO.Directory]::CreateDirectory($logDir) | Out-Null
$log=Join-Path $logDir ("{0}-{1}.log" -f $Configuration,(Get-Date -Format 'yyyyMMdd-HHmmss'))
$editorBuild=Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
& $editorBuild VerticalCityFPSEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=4 -NoUBA
if ($LASTEXITCODE -ne 0) {throw 'Editor target compilation failed before cook.'}
& $uat BuildCookRun "-project=$project" -noP4 -platform=Win64 "-clientconfig=$Configuration" -skipbuildeditor -prereqs -build -cook -stage -pak -iostore -archive "-archivedirectory=$destination" -map=/Game/VerticalCity/Maps/L_Campaign -utf8output -unattended '-AdditionalCookerOptions=-forcerecook=false' '-ubtargs=-MaxParallelActions=2 -NoUBA' *> $log
$code=$LASTEXITCODE
Get-Content -LiteralPath $log -Tail 12
if ($code -ne 0) {throw "AutomationTool returned $code. See $log"}
$exe=Join-Path $destination 'Windows\VerticalCityFPS.exe'
if (!(Test-Path -LiteralPath $exe)) {throw "No archived game executable in $destination"}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Notices') -Destination (Join-Path $destination 'Windows') -Recurse -Force
$win=Split-Path $exe -Parent
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README-zh-CN.md') -Destination $win -Force
$prerequisiteDir=Join-Path $win 'Prerequisites'
[IO.Directory]::CreateDirectory($prerequisiteDir) | Out-Null
foreach ($redistributable in @('vc_redist.x64.exe','GameInputRedist.msi')) {
    $file=Join-Path $EngineRoot ('Engine\Extras\Redist\en-us\'+$redistributable)
    Copy-Item -LiteralPath $file -Destination $prerequisiteDir -Force
}
Get-FileHash -Algorithm SHA256 -LiteralPath $exe
