param(
    [ValidateRange(1, 100000)] [int] $Frames = 300,
    [ValidateRange(160, 7680)] [int] $Width = 960,
    [ValidateRange(120, 4320)] [int] $Height = 540,
    [string] $PreferDevice = 'Vega 8',
    [ValidateSet('fifo', 'immediate', 'mailbox', 'relaxed')] [string] $PresentMode = 'fifo',
    [switch] $SynchronousPresent,
    [switch] $AcquireOnly,
    [string] $SdkRoot = $(if ($env:VULKAN_SDK) { $env:VULKAN_SDK } else { 'C:\VulkanSDK\1.4.363.0' }),
    [switch] $Validation
)

$ErrorActionPreference = 'Stop'
$dll = Join-Path $PSScriptRoot 'VulkanNativePresentProbe.dll'
if (!(Test-Path -LiteralPath $dll)) { throw "Build the native probe first: $dll" }
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
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;
public static class Gow3NativeProbeHost {
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  public delegate void Log([MarshalAs(UnmanagedType.LPStr)] string message);
  public static readonly ConcurrentQueue<string> Messages = new ConcurrentQueue<string>();
  private static readonly Log Callback = message => Messages.Enqueue(message);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
  public static extern IntPtr GetModuleHandle(string name);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern bool SetDllDirectory(string path);
  [DllImport("dwmapi.dll")]
  public static extern int DwmFlush();
  [DllImport("VulkanNativePresentProbe.dll", CallingConvention=CallingConvention.Cdecl, EntryPoint="RunNativeVulkanProbe")]
  public static extern int Run(IntPtr instance, IntPtr hwnd, UInt32 frames,
      [MarshalAs(UnmanagedType.LPStr)] string preferredDevice,
      [MarshalAs(UnmanagedType.LPStr)] string preferredPresentMode, bool acquireOnly, bool synchronousPresent, Log log);
  public static Task<int> Start(IntPtr instance, IntPtr hwnd, UInt32 frames, string preferredDevice,
      string preferredPresentMode, bool acquireOnly, bool synchronousPresent) {
      return Task.Run(() => Run(instance, hwnd, frames, preferredDevice, preferredPresentMode,
          acquireOnly, synchronousPresent, Callback));
  }
}
"@
Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition $source -ReferencedAssemblies System.Windows.Forms
$null = [Gow3NativeProbeHost]::SetDllDirectory($PSScriptRoot)
$form = New-Object Windows.Forms.Form
$form.Text = 'Native Vulkan WSI probe'
$form.ClientSize = New-Object Drawing.Size($Width, $Height)
$form.Show()
[Windows.Forms.Application]::DoEvents()
$probeWindow = $form.Handle
$probeInstance = [Gow3NativeProbeHost]::GetModuleHandle($null)
$probeTask = [Gow3NativeProbeHost]::Start($probeInstance, $probeWindow, $Frames, $PreferDevice, $PresentMode,
    [bool]$AcquireOnly, [bool]$SynchronousPresent)
while (-not $probeTask.IsCompleted) {
    [Windows.Forms.Application]::DoEvents()
    $message = $null
    while ([Gow3NativeProbeHost]::Messages.TryDequeue([ref]$message)) {
        [Console]::WriteLine($message)
        if ($message -eq 'Queue presented.') { [void][Gow3NativeProbeHost]::DwmFlush() }
    }
    Start-Sleep -Milliseconds 10
}
$message = $null
while ([Gow3NativeProbeHost]::Messages.TryDequeue([ref]$message)) {
    [Console]::WriteLine($message)
    if ($message -eq 'Queue presented.') { [void][Gow3NativeProbeHost]::DwmFlush() }
}
$result = $probeTask.GetAwaiter().GetResult()
$form.Close()
Write-Output "Native probe exit result: $result"
exit $result
