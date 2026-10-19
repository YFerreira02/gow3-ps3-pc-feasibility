param(
    [string] $CompilerPath
)

$ErrorActionPreference = "Stop"

if (-not $CompilerPath) {
    $compilerCommand = Get-Command glslangValidator, glslang -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($compilerCommand) {
        $CompilerPath = $compilerCommand.Source
    }
    else {
        $localCompiler = Join-Path $PSScriptRoot "..\work\glslang\bin\glslang.exe"
        if (Test-Path -LiteralPath $localCompiler) {
            $CompilerPath = (Resolve-Path -LiteralPath $localCompiler).Path
        }
        else {
            throw "No glslang compiler found. Pass -CompilerPath or install Vulkan SDK/glslang."
        }
    }
}

$shaderRoot = Join-Path $PSScriptRoot "..\shaders"
$outputRoot = Join-Path $shaderRoot "prebuilt"
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

& $CompilerPath -V --target-env vulkan1.2 -S vert (Join-Path $shaderRoot "triangle.vert") -o (Join-Path $outputRoot "triangle.vert.spv")
if ($LASTEXITCODE -ne 0) { throw "Vertex shader compilation failed with exit code $LASTEXITCODE." }

& $CompilerPath -V --target-env vulkan1.2 -S frag (Join-Path $shaderRoot "triangle.frag") -o (Join-Path $outputRoot "triangle.frag.spv")
if ($LASTEXITCODE -ne 0) { throw "Fragment shader compilation failed with exit code $LASTEXITCODE." }

Write-Output "Shaders compiled for Vulkan 1.2 into $outputRoot"
