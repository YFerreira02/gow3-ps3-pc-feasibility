param(
    [string] $PreferDevice = "Vega 8",
    [ValidateRange(160, 7680)] [int] $Width = 960,
    [ValidateRange(120, 4320)] [int] $Height = 540,
    [ValidateRange(1, 10000)] [int] $Frames = 1,
    [switch] $ShowWindow
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms

# Creates a Vulkan 1.2 instance, Win32 surface, logical device, swapchain,
# graphics pipeline, and presents shader-backed triangle frames. Use -ShowWindow
# to display the probe. It contains no game code or assets.
Add-Type -Path (Join-Path $PSScriptRoot "VulkanTriangleProbe.cs")
$window = New-Object System.Windows.Forms.Form
$window.Text = "GOW3 Vulkan triangle probe"
$window.ClientSize = New-Object System.Drawing.Size -ArgumentList $Width,$Height
$window.ShowInTaskbar = $false
$window.Opacity = 0
$window.AutoScaleMode = [System.Windows.Forms.AutoScaleMode]::None
$window.CreateControl()
$vertexShader = Join-Path $PSScriptRoot "..\shaders\prebuilt\triangle.vert.spv"
$fragmentShader = Join-Path $PSScriptRoot "..\shaders\prebuilt\triangle.frag.spv"
try {
    if ($ShowWindow) { $window.Opacity = 1; $window.Show(); [System.Windows.Forms.Application]::DoEvents() }
    $result = [VulkanTriangleProbe]::Run($window.Handle, $PreferDevice, $vertexShader, $fragmentShader, $Frames)
}
finally {
    $window.Dispose()
}
Write-Output "Probe exit result: $result"
exit $result
