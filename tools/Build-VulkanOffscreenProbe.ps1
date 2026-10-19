param(
    [Parameter(Mandatory)] [string] $ZigExe,
    [string] $SdkRoot = $(if ($env:VULKAN_SDK) { $env:VULKAN_SDK } else { 'C:\VulkanSDK\1.4.363.0' }),
    [string] $OutputPath
)

$ErrorActionPreference = 'Stop'
$OutputPath = if ($OutputPath) { $OutputPath } else { Join-Path $PSScriptRoot 'VulkanOffscreenPresentProbe.dll' }
$ZigExe = (Resolve-Path -LiteralPath $ZigExe).Path
$SdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$zigRoot = Split-Path -Parent $ZigExe
$windowsHeaders = Join-Path $zigRoot 'lib\libc\include\any-windows-any'
$source = Join-Path $PSScriptRoot 'VulkanOffscreenPresentProbe.c'
$include = Join-Path $SdkRoot 'Include'
$lib = Join-Path $SdkRoot 'Lib'
if (!(Test-Path (Join-Path $windowsHeaders 'windows.h'))) { throw "Zig Windows headers not found: $windowsHeaders" }
if (!(Test-Path (Join-Path $include 'vulkan\vulkan.h'))) { throw "Vulkan headers not found: $include" }
if (!(Test-Path (Join-Path $lib 'vulkan-1.lib'))) { throw "Vulkan loader import library not found: $lib" }

& $ZigExe cc -shared -nostdlib -fno-sanitize=undefined -fno-stack-check `
    -target x86_64-windows-gnu "-I$windowsHeaders" "-I$include" $source `
    "-L$lib" -lvulkan-1 '-Wl,--entry,DllMainCRTStartup' -o $OutputPath
if ($LASTEXITCODE -ne 0) { throw "Offscreen Vulkan probe build failed with exit code $LASTEXITCODE" }
Write-Output "Built offscreen Vulkan probe: $OutputPath"
