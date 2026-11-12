# Capture ONLY the game's own window (PrintWindow with PW_RENDERFULLCONTENT), never the desktop.
param([string]$Exe, [string]$Dir, [int[]]$At, [string]$GameArgs = "", [string]$OutDir = ".")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
}
"@
if ($GameArgs) { $p = Start-Process -FilePath $Exe -WorkingDirectory $Dir -ArgumentList $GameArgs -PassThru } else { $p = Start-Process -FilePath $Exe -WorkingDirectory $Dir -PassThru }
$t0 = 0
foreach ($s in $At) {
  Start-Sleep -Seconds ($s - $t0); $t0 = $s
  $p.Refresh(); $h = $p.MainWindowHandle
  if ($p.HasExited) { "exited before t=$s code=$($p.ExitCode)"; break }
  if ($h -eq [IntPtr]::Zero) { "t=$s no window"; continue }
  $r = New-Object W+RECT; [W]::GetClientRect($h, [ref]$r) | Out-Null
  $w = [Math]::Max(1, $r.R - $r.L); $hh = [Math]::Max(1, $r.B - $r.T)
  $bmp = New-Object System.Drawing.Bitmap $w, $hh
  $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
  [W]::PrintWindow($h, $dc, 3) | Out-Null
  $g.ReleaseHdc($dc); $g.Dispose()
  $f = Join-Path (Resolve-Path $OutDir) ("game_t{0}.png" -f $s); $bmp.Save($f); $bmp.Dispose()
  "t=$s window '$($p.MainWindowTitle)' ${w}x${hh} -> $f"
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; "terminated" }
