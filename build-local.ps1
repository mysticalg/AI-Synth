param(
    [ValidateSet("Standalone", "VST3", "All")]
    [string]$Target = "Standalone",

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",

    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

function Resolve-CMake {
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCommand) {
        return $cmakeCommand.Source
    }

    $vsCMake = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCMake) {
        return $vsCMake
    }

    throw "CMake was not found on PATH or in Visual Studio Build Tools."
}

$cmake = Resolve-CMake
$targets = switch ($Target) {
    "Standalone" { @("AISynth_Standalone") }
    "VST3" { @("AISynth_VST3") }
    "All" { @("AISynth_Standalone", "AISynth_VST3") }
}

Write-Host "Using CMake: $cmake"
Write-Host "Configuring $BuildDir with Visual Studio 2022 (x64)..."
& $cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

foreach ($buildTarget in $targets) {
    Write-Host "Building target: $buildTarget ($Config)"
    & $cmake --build $BuildDir --config $Config --target $buildTarget
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$standalonePath = Join-Path $BuildDir "AISynth_artefacts\$Config\Standalone\AI Synth.exe"
if ($targets -contains "AISynth_Standalone" -and (Test-Path $standalonePath)) {
    Write-Host ""
    Write-Host "Standalone app ready:"
    Write-Host "  $standalonePath"
}
