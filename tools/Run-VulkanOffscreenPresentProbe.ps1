param(
    [ValidateRange(1, 100000)] [int] $Frames = 60,
    [ValidateRange(64, 7680)] [int] $Width = 640,
    [ValidateRange(64, 4320)] [int] $Height = 360,
    [ValidateRange(0, 7680)] [int] $OutputWidth = 0,
    [ValidateRange(0, 4320)] [int] $OutputHeight = 0,
    [string] $SdkRoot = $(if ($env:VULKAN_SDK) { $env:VULKAN_SDK } else { 'C:\VulkanSDK\1.4.363.0' }),
    [switch] $Validation
)

$ErrorActionPreference = 'Stop'
if ($OutputWidth -eq 0) { $OutputWidth = $Width }
if ($OutputHeight -eq 0) { $OutputHeight = $Height }
$dll = Join-Path $PSScriptRoot 'VulkanOffscreenPresentProbe.dll'
if (!(Test-Path -LiteralPath $dll)) { throw "Build the offscreen probe first: $dll" }
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$env:PATH = (Join-Path $SdkRoot 'Bin') + ';' + $env:PATH
if ($Validation) {
    $env:VK_LAYER_PATH = Join-Path $SdkRoot 'Bin'
    $env:VK_INSTANCE_LAYERS = 'VK_LAYER_KHRONOS_validation'
} else {
    Remove-Item Env:VK_INSTANCE_LAYERS -ErrorAction SilentlyContinue
    Remove-Item Env:VK_LAYER_PATH -ErrorAction SilentlyContinue
}

$source = @"
using System;
using System.Collections.Concurrent;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;
public static class Gow3OffscreenProbeHost {
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void Log([MarshalAs(UnmanagedType.LPStr)] string message);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void Frame(IntPtr pixels, UInt32 byteCount);
  public static readonly ConcurrentQueue<string> Messages = new ConcurrentQueue<string>();
  public static readonly ConcurrentQueue<byte[]> Frames = new ConcurrentQueue<byte[]>();
  private static readonly Log LogCallback = message => Messages.Enqueue(message);
  private static readonly Frame FrameCallback = (pixels, byteCount) => {
      byte[] copy = new byte[byteCount]; Marshal.Copy(pixels, copy, 0, copy.Length);
      byte[] stale; while (Frames.Count >= 2 && Frames.TryDequeue(out stale)) {}
      Frames.Enqueue(copy);
  };
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern bool SetDllDirectory(string path);
  [DllImport("VulkanOffscreenPresentProbe.dll", CallingConvention=CallingConvention.Cdecl, EntryPoint="RunVulkanOffscreenProbe")]
  public static extern int Run(UInt32 width, UInt32 height, UInt32 frames, Log log, Frame onFrame);
  public static Task<int> Start(UInt32 width, UInt32 height, UInt32 frames) {
      return Task.Run(() => Run(width, height, frames, LogCallback, FrameCallback));
  }
  public static Bitmap MakeBitmap(byte[] rgba, int width, int height) {
      for (int i=0; i+3<rgba.Length; i+=4) { byte r=rgba[i]; rgba[i]=rgba[i+2]; rgba[i+2]=r; }
      Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb);
      BitmapData data = bitmap.LockBits(new Rectangle(0,0,width,height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
      Marshal.Copy(rgba, 0, data.Scan0, rgba.Length);
      bitmap.UnlockBits(data);
      return bitmap;
  }
}
"@
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition $source -ReferencedAssemblies System.Windows.Forms,System.Drawing
$null = [Gow3OffscreenProbeHost]::SetDllDirectory($PSScriptRoot)
$form = New-Object Windows.Forms.Form
$form.Text = "GOW3 Vulkan 1.2 internal ${Width} x ${Height} -> output ${OutputWidth} x ${OutputHeight}"
$form.ClientSize = New-Object Drawing.Size($OutputWidth, $OutputHeight)
$picture = New-Object Windows.Forms.PictureBox
$picture.Dock = [Windows.Forms.DockStyle]::Fill
$picture.SizeMode = [Windows.Forms.PictureBoxSizeMode]::Zoom
$form.Controls.Add($picture)
$form.Show()
[Windows.Forms.Application]::DoEvents()
$watch = [System.Diagnostics.Stopwatch]::StartNew()
$probeTask = [Gow3OffscreenProbeHost]::Start([uint32]$Width, [uint32]$Height, [uint32]$Frames)
while (-not $probeTask.IsCompleted) {
    [Windows.Forms.Application]::DoEvents()
    $message = $null
    while ([Gow3OffscreenProbeHost]::Messages.TryDequeue([ref]$message)) { [Console]::WriteLine($message) }
    $pixels = $null
    while ([Gow3OffscreenProbeHost]::Frames.TryDequeue([ref]$pixels)) {
        $old = $picture.Image
        $picture.Image = [Gow3OffscreenProbeHost]::MakeBitmap($pixels, $Width, $Height)
        if ($old) { $old.Dispose() }
    }
    Start-Sleep -Milliseconds 5
}
$message = $null
while ([Gow3OffscreenProbeHost]::Messages.TryDequeue([ref]$message)) { [Console]::WriteLine($message) }
$pixels = $null
while ([Gow3OffscreenProbeHost]::Frames.TryDequeue([ref]$pixels)) {
    $old = $picture.Image
    $picture.Image = [Gow3OffscreenProbeHost]::MakeBitmap($pixels, $Width, $Height)
    if ($old) { $old.Dispose() }
}
$result = $probeTask.GetAwaiter().GetResult()
$watch.Stop()
$form.Close()
if ($picture.Image) { $picture.Image.Dispose() }
Write-Output "Offscreen probe exit result: $result"
if ($result -eq 0 -and $watch.Elapsed.TotalSeconds -gt 0) {
    $rate = [Math]::Round($Frames / $watch.Elapsed.TotalSeconds, 2)
    Write-Output "Vulkan render + host-readback throughput: $rate frames/s at ${Width}x${Height} (not a game FPS benchmark)."
}
exit $result
