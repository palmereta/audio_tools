# Builds Audio Tools plugins on Windows (x64, MSVC) and packages them.
# Requirements: Visual Studio 2022 Build Tools (C++), CMake >= 3.22, Ninja.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (!(Test-Path external\JUCE\CMakeLists.txt)) { git submodule update --init --depth 1 external/JUCE }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "call `"$vcvars`" && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build"
if ($LASTEXITCODE -ne 0) { throw "build failed" }

function Package-Plugin([string]$dir, [string]$product) {
    $upper = $dir.ToUpper()
    $ver = (Select-String -Path "plugins\$dir\CMakeLists.txt" -Pattern "^set\(${upper}_VERSION (.*)\)").Matches[0].Groups[1].Value
    $name = "$product-$ver-Windows"
    $out = "dist\$name"
    Remove-Item -Recurse -Force $out, "dist\$name.zip" -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $out, web\downloads | Out-Null
    $r = "build\plugins\$dir"
    # Todas las variantes del plugin (p. ej. AT20V y AT20V FX).
    Get-ChildItem "$r\*_artefacts\Release\VST3\*.vst3" | ForEach-Object { Copy-Item -Recurse $_.FullName $out }
    Get-ChildItem "$r\*_artefacts\Release\Standalone\*.exe" | ForEach-Object { Copy-Item $_.FullName $out }
    # Herramientas de línea de comandos del producto (p. ej. lacquerrender).
    foreach ($t in @("lacquerrender", "halationrender", "tarnishrender", "at303render", "plumbrender")) {
        foreach ($f in @("$r\$t.exe", "$r\${t}_artefacts\Release\$t.exe")) { if (Test-Path $f) { Copy-Item $f $out; break } }
    }
    Copy-Item LICENSE "$out\LICENSE.txt"
@"
$product $ver by Audio Tools - Windows 10/11 x64

1. Copy the .vst3 folders to C:\Program Files\Common Files\VST3\ (requires administrator rights).
2. Rescan plugins in your DAW (Ableton Live: Settings > Plug-Ins > Rescan).
$product.exe is the standalone version.

The binaries are not code-signed: Windows SmartScreen may ask for confirmation the first time.

--- Español ---
1. Copiar los .vst3 a C:\Program Files\Common Files\VST3\ (pide permisos de administrador).
2. Reescanear los plugins en el DAW. $product.exe es la versión standalone.

Free and open source software under the GNU AGPL v3 (see LICENSE.txt).
"@ | Set-Content -Encoding UTF8 "$out\INSTALL.txt"
    # Notas propias del producto (p. ej. Orrery: cómo rutear su MIDI).
    if (Test-Path "plugins\$dir\INSTALL-notes.txt") { Get-Content -Encoding UTF8 "plugins\$dir\INSTALL-notes.txt" | Add-Content -Encoding UTF8 "$out\INSTALL.txt" }
    # tar.exe (bsdtar, incluido en Windows 10/11): Compress-Archive y ZipFile de .NET Framework
    # guardan las rutas con barras invertidas y otros descompresores las toman como nombres de archivo.
    tar.exe -a -c -f "dist\$name.zip" -C dist $name
    if ($LASTEXITCODE -ne 0) { throw "zip failed" }
    Copy-Item "dist\$name.zip" web\downloads\ -Force
    Get-Item "web\downloads\$name.zip" | Select-Object Name, Length
}

Package-Plugin "at20v" "AT20V"
Package-Plugin "at101v" "AT101V"
Package-Plugin "at42" "AT42"
Package-Plugin "at700" "AT700"
Package-Plugin "atfs1" "ATFS1"
Package-Plugin "orrery" "Orrery"
Package-Plugin "murmur" "Murmur"
Package-Plugin "atmd" "ATMD"
Package-Plugin "lacquer" "Lacquer"
Package-Plugin "halation" "Halation"
Package-Plugin "tarnish" "Tarnish"
Package-Plugin "at303" "AT303"
Package-Plugin "plumb" "Plumb"

# The launcher (tools/launcher): one .exe that installs and updates the plugins from web/downloads/manifest.json.
function Package-Launcher {
    $ver = (Select-String -Path "tools\launcher\CMakeLists.txt" -Pattern "^set\(LAUNCHER_VERSION (.*)\)").Matches[0].Groups[1].Value
    $name = "AudioToolsLauncher-$ver-Windows"
    $out = "dist\$name"
    Remove-Item -Recurse -Force $out, "dist\$name.zip" -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $out, web\downloads | Out-Null
    Copy-Item "build\tools\launcher\AudioToolsLauncher_artefacts\Release\Audio Tools Launcher.exe" $out
    Copy-Item LICENSE "$out\LICENSE.txt"
@"
Audio Tools Launcher $ver - Windows 10/11 x64

1. Run "Audio Tools Launcher.exe" (you can keep it anywhere, e.g. on the Desktop).
2. The binary is not code-signed: Windows SmartScreen may ask the first time (More info > Run anyway).
3. Pick the plugins and press Install. VST3 goes to %LOCALAPPDATA%\Programs\Common\VST3 (or, if you choose
   all users in Settings, to C:\Program Files\Common Files\VST3, which needs administrator rights);
   standalone apps go to %LOCALAPPDATA%\Programs\Audio Tools.

No account and nothing to sign up for. The launcher only downloads the list of plugins
(https://audiotools.com.ar/downloads/manifest.json), the images and demos it shows, and the plugins you choose.

--- Español ---
1. Abrir "Audio Tools Launcher.exe". 2. Si SmartScreen pregunta: Más información > Ejecutar de todas formas.
3. Elegir los plugins y tocar Install.

Free and open source software under the GNU AGPL v3 (see LICENSE.txt).
"@ | Set-Content -Encoding UTF8 "$out\INSTALL.txt"
    tar.exe -a -c -f "dist\$name.zip" -C dist $name
    if ($LASTEXITCODE -ne 0) { throw "zip failed" }
    Copy-Item "dist\$name.zip" web\downloads\ -Force
    Get-Item "web\downloads\$name.zip" | Select-Object Name, Length
}
Package-Launcher
