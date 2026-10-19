param(
    [string] $DotnetExe = $(if (Test-Path (Join-Path $PSScriptRoot '..\work\dotnet-sdk\dotnet.exe')) { Join-Path $PSScriptRoot '..\work\dotnet-sdk\dotnet.exe' } else { (Get-Command dotnet -ErrorAction Stop).Source }),
    [string] $OutputPath = (Join-Path $PSScriptRoot '..\work\Gow3LauncherRelease')
)

$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot '..\src\Gow3Launcher\Gow3Launcher.csproj'
$project = (Resolve-Path -LiteralPath $project).Path
$DotnetExe = (Resolve-Path -LiteralPath $DotnetExe).Path
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$workRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\work'))
if (-not $OutputPath.StartsWith(($workRoot + [System.IO.Path]::DirectorySeparatorChar), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "For safety, publish output must stay inside the ignored work folder: $workRoot"
}

& $DotnetExe publish $project --configuration Release --runtime win-x64 --self-contained true `
    --output $OutputPath -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true `
    -p:RestoreIgnoreFailedSources=true
if ($LASTEXITCODE -ne 0) { throw "Launcher publish failed with exit code $LASTEXITCODE" }

$exe = Join-Path $OutputPath 'GOW3 PS3 Launcher.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Published launcher was not found: $exe" }
$probe = Join-Path $OutputPath 'tools\Test-Vulkan12.ps1'
if (-not (Test-Path -LiteralPath $probe)) { throw "Published Vulkan probe was not found: $probe" }
$readme = Join-Path $OutputPath 'LEEME.txt'
@'
GOW III PS3 - Launcher experimental para Windows 11 x64

REQUISITOS
- RPCS3 instalado por separado. Este paquete no lo incluye.
- Una copia propia/legal de God of War III PS3 como carpeta PS3_GAME o ISO compatible con RPCS3.
- Firmware PS3 instalado por el usuario en RPCS3.

USO
1. Abre "GOW3 PS3 Launcher.exe".
2. Selecciona rpcs3.exe.
3. Selecciona la carpeta extraida que contiene PS3_GAME o pulsa Buscar ISO para cargarla directamente sin extraer otra copia.
4. Si falta firmware, descárgalo de Sony e instálalo con el botón del launcher; RPCS3 debe estar cerrado.
5. Elige la escala interna, comprueba Vulkan 1.2 y luego inicia el juego.
6. Desmarca el inicio directo si quieres ver la ventana principal de RPCS3.

La carpeta extraida se valida con PARAM.SFO BCES00510. La ISO se pasa directamente a RPCS3, que debe reconocer el disco; una imagen cifrada puede requerir la clave propia correspondiente.
Escala interna: 25-100% de 1280x720; el perfil anterior se respalda al cambiarlo.
El perfil activa el overlay de FPS/frame time de RPCS3 para medir.
No extrae ni copia la ISO: pasa su ruta a RPCS3. No instala parches; pasa un PUP local a RPCS3 con --installfw. Sigue siendo un frontend, no un port nativo.
La compatibilidad y el rendimiento dependen de RPCS3 y del equipo; 50-60 FPS no esta garantizado.
La configuracion local se guarda en %LOCALAPPDATA%\GOW3PS3PC.
'@ | Set-Content -LiteralPath $readme -Encoding utf8
$zip = Join-Path (Split-Path -Parent $OutputPath) 'Gow3Launcher-win-x64.zip'
Compress-Archive -LiteralPath $exe, $probe, $readme -DestinationPath $zip -CompressionLevel Optimal -Force
Write-Output "Published Windows x64 launcher: $exe"
Write-Output "Vulkan probe: $probe"
Write-Output "Package: $zip"
