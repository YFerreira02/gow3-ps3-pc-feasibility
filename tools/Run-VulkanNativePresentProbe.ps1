param(
    [ValidateRange(1, 100000)] [int] $Frames = 300,
    [ValidateRange(160, 7680)] [int] $Width = 960,
    [ValidateRange(120, 4320)] [int] $Height = 540,
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
using System.Runtime.InteropServices;
using System.Windows.Forms;
public static class Gow3NativeProbeHost {
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  public delegate void Log([MarshalAs(UnmanagedType.LPStr)] string message);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
  public static extern IntPtr GetModuleHandle(string name);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern bool SetDllDirectory(string path);
  [DllImport("VulkanNativePresentProbe.dll", CallingConvention=CallingConvention.Cdecl, EntryPoint="RunNativeVulkanProbe")]
  public static extern int Run(IntPtr instance, IntPtr hwnd, UInt32 frames, Log log);
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
$logger = [Gow3NativeProbeHost+Log]{
    param($message)
    [Console]::WriteLine($message)
    if ($message -eq 'Cleanup: destroying swapchain.') {
        Start-Sleep -Milliseconds 1500
    }
}
$result = [Gow3NativeProbeHost]::Run([Gow3NativeProbeHost]::GetModuleHandle($null), $form.Handle, $Frames, $logger)
$form.Close()
Write-Output "Native probe exit result: $result"
exit $result
