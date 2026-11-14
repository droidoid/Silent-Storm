# Runs the bench scenarios without a picture, on a machine without a GPU (the build server): the
# editor binaries as a dedicated server (-server). In that mode the engine creates no Slate
# application, so no window, no viewport and no mouse capture, and draws nothing. It checks the logic,
# the result files and the exit, not the speed: a dedicated server runs at most 30 frames a second.
#
#   powershell -NoProfile -File prototype\ue5\check_headless.ps1 -Engine C:\Users\admin\UE\UE_5.8
#
# Every run goes through reconstruction\tools\run_game.ps1, so a separate watchdog ends it after
# -MinutesPerRun. While a run is going, this script also watches the desktop four times a second: if
# a program of the engine shows a window or the mouse cursor gets confined, the run is killed at once
# and the cursor is freed. Build the project first (Build.bat SSProtoEditor Win64 Development).
param(
	[Parameter(Mandatory = $true)][string]$Engine,
	[string[]]$Scenarios = @('idle', 'grenades', 'hundred', 'half'),
	[string[]]$Buildings = @('small'),
	[int[]]$Chunks = @(0),
	[string]$Results = '',
	[int]$MinutesPerRun = 5,
	[string[]]$ExtraArgs = @()   # more arguments for the game, e.g. -SSCapture
)
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'SSProto.uproject'
$runGame = Join-Path $PSScriptRoot '..\..\reconstruction\tools\run_game.ps1'
$bin = Join-Path $Engine 'Engine\Binaries\Win64'
if (-not (Test-Path (Join-Path $bin 'UnrealEditor.exe'))) { throw "no UnrealEditor.exe in $bin" }
if (-not (Test-Path (Join-Path $PSScriptRoot 'Binaries\Win64\UnrealEditor-SSProto.dll'))) { throw 'build the project first' }
if (-not $Results) { $Results = Join-Path $PSScriptRoot ('Saved\Headless\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
New-Item -ItemType Directory -Force $Results | Out-Null

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class DesktopGuard
{
	delegate bool EnumProc(IntPtr window, IntPtr param);
	[StructLayout(LayoutKind.Sequential)] struct Rect { public int Left, Top, Right, Bottom; }
	[DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr param);
	[DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
	[DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
	[DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int size);
	[DllImport("user32.dll")] static extern bool GetClipCursor(out Rect rect);
	[DllImport("user32.dll")] static extern bool ClipCursor(IntPtr rect);
	[DllImport("user32.dll")] static extern int GetSystemMetrics(int index);

	// Visible top-level windows, each as "handle pid class".
	public static List<string> VisibleWindows()
	{
		var found = new List<string>();
		EnumWindows((window, param) =>
		{
			if (IsWindowVisible(window))
			{
				uint pid;
				GetWindowThreadProcessId(window, out pid);
				var name = new StringBuilder(256);
				GetClassName(window, name, name.Capacity);
				found.Add(window.ToInt64() + " " + pid + " " + name);
			}
			return true;
		}, IntPtr.Zero);
		return found;
	}

	// True when the cursor is confined to less than the whole desktop.
	public static bool CursorConfined()
	{
		Rect r;
		if (!GetClipCursor(out r))
			return false;
		int x = GetSystemMetrics(76), y = GetSystemMetrics(77), w = GetSystemMetrics(78), h = GetSystemMetrics(79);
		return r.Left > x || r.Top > y || r.Right < x + w || r.Bottom < y + h;
	}

	public static void FreeCursor() { ClipCursor(IntPtr.Zero); }
}
'@

# Log errors about the machine, not about the project: Unreal 5.8 asks for the Visual C++ runtime 14.50
# (what Epic builds with), the build server has 14.44 - the compiler its engine was built with, so the
# runtime matches. On a machine with a launcher install the launcher puts in 14.50.
$machineErrors = 'Visual C\+\+ redistributable version .* is outdated'
# Warnings of the editor's own interface, which the editor binaries print in every mode.
$editorNoise = 'LogEditorDataStorageUI: Warning:'

# Programs the engine may start besides itself.
$engineNames = 'UnrealEditor', 'UnrealEditor-Cmd', 'CrashReportClientEditor', 'CrashReportClient', 'zenserver',
	'ShaderCompileWorker', 'UnrealTraceServer', 'LiveCodingConsole'
function Get-EnginePids {
	$pids = @{}
	Get-Process -Name $engineNames -ErrorAction SilentlyContinue | ForEach-Object { $pids[[string]$_.Id] = $_.Name }
	$pids
}

$batchStart = Get-Date
$machine = @()
$report = New-Object System.Collections.Generic.List[string]
$report.Add("SSProto headless check, $($batchStart.ToString('yyyy-MM-dd HH:mm')), engine $Engine")
$report.Add("power plan: $((powercfg /getactivescheme) -join ' ')")
$failed = 0
foreach ($building in $Buildings) {
	foreach ($chunk in $Chunks) {
		foreach ($scenario in $Scenarios) {
			$name = '{0}_{1}_c{2}' -f $building, $scenario, $chunk
			$out = Join-Path $Results $name
			$log = Join-Path $Results "$name.log"
			$gameArgs = @("`"$project`"", '-server', '-unattended', '-nosplash', '-nosound', '-notraceserver', '-nocrashreports',
				'-multihome=127.0.0.1', "-SSScenario=$scenario", "-SSBuilding=$building", "-SSChunk=$chunk", "-SSResults=`"$out`"",
				"-abslog=`"$log`"") + $ExtraArgs

			$windowsBefore = @{}
			foreach ($w in [DesktopGuard]::VisibleWindows()) { $windowsBefore[($w -split ' ')[0]] = $true }
			$confinedBefore = [DesktopGuard]::CursorConfined()
			$started = & $runGame -RunDir $bin -Exe 'UnrealEditor.exe' -Minutes $MinutesPerRun -Hidden -GameArgs $gameArgs
			if ("$started" -notmatch 'pid (\d+);') { throw "run_game.ps1 said: $started" }
			$proc = Get-Process -Id ([int]$Matches[1])
			$null = $proc.Handle   # keeps the exit code readable after the exit
			$t0 = Get-Date
			$stopped = ''
			$foreign = @{}
			while (-not $proc.HasExited) {
				# Not $engine: PowerShell names ignore case, and that is the [string] parameter $Engine.
				$enginePids = Get-EnginePids
				foreach ($w in [DesktopGuard]::VisibleWindows()) {
					$hwnd, $wpid, $class = $w -split ' ', 3
					if ($windowsBefore.ContainsKey($hwnd)) { continue }
					if ($enginePids.ContainsKey($wpid)) { $stopped = "a window of $($enginePids[$wpid]) ($class)" }
					else { $foreign[$hwnd] = (Get-Process -Id ([int]$wpid) -ErrorAction SilentlyContinue).Name }
				}
				if (-not $confinedBefore -and [DesktopGuard]::CursorConfined()) { $stopped = 'the mouse cursor got confined' }
				if ($stopped) {
					Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
					break
				}
				Start-Sleep -Milliseconds 250
			}
			$proc.WaitForExit()
			if (-not $confinedBefore -and [DesktopGuard]::CursorConfined()) { [DesktopGuard]::FreeCursor() }
			$seconds = [int]((Get-Date) - $t0).TotalSeconds

			$summary = Join-Path $out 'summary.txt'
			$frames = Join-Path $out 'frames.csv'
			$finished = if (Test-Path $summary) { Get-Content $summary -TotalCount 1 } else { 'no summary.txt' }
			$rows = if (Test-Path $frames) { (Get-Content $frames).Count - 1 } else { 0 }
			$errors = @(); $warnings = @()
			if (Test-Path $log) {
				# Without the [time][frame] prefix, so that repeated messages collapse into one.
				$errors = @(Select-String -Path $log -CaseSensitive -Pattern 'Error:|Fatal error|Assertion failed|Ensure condition failed' |
					ForEach-Object { $_.Line -replace '^(\[[^\]]*\]){2}', '' } | Sort-Object -Unique)
				$machine += @($errors | Where-Object { $_ -match $machineErrors })
				$errors = @($errors | Where-Object { $_ -notmatch $machineErrors })
				$warnings = @(Select-String -Path $log -CaseSensitive -Pattern 'Warning:' |
					ForEach-Object { $_.Line -replace '^(\[[^\]]*\]){2}', '' } | Where-Object { $_ -notmatch $editorNoise } | Sort-Object -Unique)
			}
			# The run must repeat the back-to-back replay of the same scenario exactly (summary.txt says "equal").
			$diverged = (Test-Path $summary) -and [bool](Select-String -Path $summary -SimpleMatch '(DIFFERENT)')
			$ok = -not $stopped -and $proc.ExitCode -eq 0 -and (Test-Path $summary) -and $rows -gt 0 -and $errors.Count -eq 0 -and
				$warnings.Count -eq 0 -and -not $diverged
			if (-not $ok) { ++$failed }
			$report.Add('')
			$report.Add(('{0} {1}: exit code {2} after {3} s, {4} frame rows, log: {5} errors, {6} warnings' -f
				$(if ($ok) { 'OK  ' } else { 'FAIL' }), $name, $proc.ExitCode, $seconds, $rows, $errors.Count, $warnings.Count))
			if ($stopped) { $report.Add("  STOPPED BY THE GUARD: $stopped") }
			if ($foreign.Count) { $report.Add("  other new windows meanwhile (not the engine's): $(($foreign.Values | Sort-Object -Unique) -join ', ')") }
			if (Test-Path $summary) { Get-Content $summary | ForEach-Object { $report.Add("  $_") } } else { $report.Add("  $finished") }
			$errors + $warnings | Select-Object -First 15 | ForEach-Object { $report.Add("  log: $_") }
		}
	}
}

# Helpers of the engine should end together with it; whatever this check started and left is stopped.
$deadline = (Get-Date).AddSeconds(30)
do {
	$left = @(Get-Process -Name $engineNames -ErrorAction SilentlyContinue |
		Where-Object { try { $_.StartTime -ge $batchStart } catch { $false } })
	if (-not $left.Count) { break }
	Start-Sleep -Seconds 1
} while ((Get-Date) -lt $deadline)
$report.Add('')
foreach ($p in $left) {
	$report.Add("left running after 30 s, stopped: $($p.Name) (pid $($p.Id))")
	Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
}
$machine | Sort-Object -Unique | ForEach-Object { $report.Add("about the machine, not counted: $_") }
$report.Add($(if ($failed) { "$failed run(s) failed" } else { 'all runs passed' }))
$report | Set-Content -Encoding UTF8 (Join-Path $Results 'report.txt')
$report
