# Builds the prototype and runs the measurement scenarios on a machine with a GPU, then puts the
# results into prototype/results/ue5_<date>_<machine>/ (commit and push them: the build server reads
# them from git).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File prototype\ue5\run_bench.ps1 -Engine "C:\Program Files\Epic Games\UE_5.8"
#
# Needs Unreal Engine 5.8 (launcher or installed build) and Visual Studio 2022 with C++ for games.
# Every run opens a 1920x1080 game window and closes itself when its scenario is over; a run that
# hangs is killed after -MinutesPerRun. The very first run also compiles shaders, which takes long.
param(
	[Parameter(Mandatory = $true)][string]$Engine,
	[string[]]$Scenarios = @('idle', 'grenades', 'hundred', 'half'),
	[string[]]$Buildings = @('small', 'large'),
	[int[]]$Chunks = @(0, 4),
	[int]$MinutesPerRun = 30,
	[switch]$Commit
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$uproject = Join-Path $PSScriptRoot 'SSProto.uproject'
$build = Join-Path $Engine 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
if (!(Test-Path $build) -or !(Test-Path $editor)) { throw "no Unreal Engine 5 in $Engine" }

Write-Output 'building the prototype...'
& $build SSProtoEditor Win64 Development "-Project=$uproject" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "the build failed (exit code $LASTEXITCODE)" }

$outRoot = Join-Path $root ("prototype\results\ue5_{0}_{1}" -f (Get-Date -Format 'yyyy-MM-dd_HHmm'), $env:COMPUTERNAME)
New-Item -ItemType Directory -Force $outRoot | Out-Null
Add-Type -AssemblyName System.Drawing
$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$quality = New-Object System.Drawing.Imaging.EncoderParameters 1
$quality.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]85)

$index = @()
foreach ($b in $Buildings) {
	foreach ($c in $Chunks) {
		foreach ($s in $Scenarios) {
			$name = "{0}_{1}_c{2}" -f $s, $b, $c
			$dir = Join-Path $outRoot $name
			Write-Output "run $name ..."
			$runArgs = @("`"$uproject`"", '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-NoSound', '-unattended', '-nosplash',
				"-SSScenario=$s", "-SSBuilding=$b", "-SSChunk=$c", "-SSResults=`"$dir`"")
			$p = Start-Process -FilePath $editor -ArgumentList $runArgs -PassThru
			if (-not $p.WaitForExit($MinutesPerRun * 60 * 1000)) {
				Stop-Process -Id $p.Id -Force
				$index += "$($name): killed after $MinutesPerRun minutes"
				continue
			}
			# Screenshots go to git: JPEG at quality 85 instead of PNG, a few hundred KB each.
			Get-ChildItem $dir -Filter *.png -ErrorAction SilentlyContinue | ForEach-Object {
				$img = [System.Drawing.Image]::FromFile($_.FullName)
				$img.Save([IO.Path]::ChangeExtension($_.FullName, '.jpg'), $jpeg, $quality)
				$img.Dispose()
				Remove-Item $_.FullName
			}
			$summary = Join-Path $dir 'summary.txt'
			$index += $(if (Test-Path $summary) { "== $name"; Get-Content $summary } else { "$($name): no summary (exit code $($p.ExitCode))" })
		}
	}
}
$index | Set-Content (Join-Path $outRoot 'summary-all.txt') -Encoding utf8
Write-Output "results: $outRoot"

if ($Commit) {
	Push-Location $root
	git add -- $outRoot
	git commit -m ("prototype: Unreal 5 measurements on {0}" -f $env:COMPUTERNAME)
	git push
	Pop-Location
}
