# Starts a program, waits, reports its windows (title + dialog text), saves a screenshot, then stops it.
# usage: powershell -File probe_run.ps1 -Exe C:\a5b\s\bin\Game.exe -Cwd C:\a5b\run -Seconds 15 -Shot C:\a5b\run\shot.png [-ExeArgs "-nosound"]
param(
	[Parameter(Mandatory=$true)][string]$Exe,
	[Parameter(Mandatory=$true)][string]$Cwd,
	[int]$Seconds = 15,
	[string]$Shot = "",
	[string]$ExeArgs = "",
	[switch]$KeepRunning
)
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class Win {
	public delegate bool EnumProc(IntPtr h, IntPtr l);
	[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
	[DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc p, IntPtr l);
	[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
	[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
	[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
	[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
	[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
	public struct RECT { public int L, T, R, B; }
	public static List<string> Describe(uint pid) {
		var res = new List<string>();
		EnumWindows((h, l) => {
			uint p; GetWindowThreadProcessId(h, out p);
			if (p != pid) return true;
			var t = new StringBuilder(512); var c = new StringBuilder(256);
			GetWindowText(h, t, 512); GetClassName(h, c, 256);
			RECT r; GetWindowRect(h, out r);
			res.Add(String.Format("window class='{0}' title='{1}' visible={2} rect={3},{4},{5},{6}", c, t, IsWindowVisible(h), r.L, r.T, r.R, r.B));
			EnumChildWindows(h, (ch, l2) => {
				var ct = new StringBuilder(2048); var cc = new StringBuilder(256);
				GetWindowText(ch, ct, 2048); GetClassName(ch, cc, 256);
				if (ct.Length > 0) res.Add(String.Format("    child class='{0}' text='{1}'", cc, ct));
				return true;
			}, IntPtr.Zero);
			return true;
		}, IntPtr.Zero);
		return res;
	}
}
"@
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $Exe
$psi.WorkingDirectory = $Cwd
$psi.Arguments = $ExeArgs
$psi.UseShellExecute = $false
$p = [System.Diagnostics.Process]::Start($psi)
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline -and -not $p.HasExited) { Start-Sleep -Milliseconds 500 }
if ($p.HasExited) {
	Write-Output ("exited: code=0x{0:X8} after {1:N1}s" -f $p.ExitCode, ($p.ExitTime - $p.StartTime).TotalSeconds)
	exit 0
}
$p.Refresh()
Write-Output ("alive after {0}s: threads={1} workingset={2:N0} MB cpu={3:N1}s responding={4}" -f $Seconds, $p.Threads.Count, ($p.WorkingSet64/1MB), $p.TotalProcessorTime.TotalSeconds, $p.Responding)
[Win]::Describe([uint32]$p.Id) | ForEach-Object { Write-Output $_ }
if ($Shot -ne "") {
	try {
		Add-Type -AssemblyName System.Windows.Forms, System.Drawing
		$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
		$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
		$g = [System.Drawing.Graphics]::FromImage($bmp)
		$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
		$bmp.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png)
		Write-Output "screenshot: $Shot ($($b.Width)x$($b.Height))"
	} catch { Write-Output "screenshot failed: $($_.Exception.Message)" }
}
if (-not $KeepRunning) { try { $p.Kill() } catch {} ; Write-Output "killed" }
