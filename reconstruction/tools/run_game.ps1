# Starts the game from a run directory with a watchdog that ends it after a time limit.
#
#   powershell -NoProfile -Command "& 'reconstruction\tools\run_game.ps1' -RunDir C:\run\ss -Minutes 10"
#
# Defaults: -RunDir C:\run\ss -Minutes 10 -GameArgs '-windowed','-800'. Call it with -Command (or
# from a PowerShell prompt) when passing -GameArgs: -File does not split a list into arguments.
#
# The game runs on the live desktop and takes the foreground. If it hangs or grabs the mouse, the
# desktop stays blocked until it is gone, so every launch gets a hard deadline: a separate hidden
# process waits for the game to exit and kills it when the limit is reached. The watchdog is
# independent of this script and of whoever started it, and ends by itself when the game exits.
param(
	[string]$RunDir = 'C:\run\ss',
	[int]$Minutes = 10,
	[string[]]$GameArgs = @('-windowed', '-800'),
	[string]$Exe = 'Game.exe',
	[int]$Seconds = 0,         # overrides -Minutes when > 0 (for testing the watchdog itself)
	[switch]$Hidden            # start the program without a window (for testing the watchdog itself)
)
$ErrorActionPreference = 'Stop'
$limit = if ($Seconds -gt 0) { $Seconds } else { $Minutes * 60 }
if ($limit -le 0) { throw 'the time limit must be positive' }
$path = Join-Path $RunDir $Exe
if (-not (Test-Path $path)) { throw "$path not found" }

$start = @{ FilePath = $path; ArgumentList = $GameArgs; WorkingDirectory = $RunDir; PassThru = $true }
if ($Hidden) { $start.WindowStyle = 'Hidden' }
$game = Start-Process @start
# The start time identifies the process: after the game has exited its id may be reused by another one.
$started = $game.StartTime.ToFileTimeUtc()
$watch = @"
`$p = Get-Process -Id $($game.Id) -ErrorAction SilentlyContinue
if (`$p -and `$p.StartTime.ToFileTimeUtc() -eq $started) {
	if (-not `$p.WaitForExit($($limit * 1000))) { Stop-Process -Id $($game.Id) -Force -ErrorAction SilentlyContinue }
}
"@
$encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($watch))
$dog = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile', '-EncodedCommand', $encoded

$deadline = (Get-Date).AddSeconds($limit)
Write-Output ("{0} started, pid {1}; watchdog pid {2} ends it at {3:HH:mm:ss} ({4} s)" -f $Exe, $game.Id, $dog.Id, $deadline, $limit)
