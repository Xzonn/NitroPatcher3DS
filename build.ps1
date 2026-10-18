param(
    [switch]$Diagnostic,
    [switch]$LaunchTrace,
    [switch]$PreparationTrace
)

$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot

Push-Location $projectRoot
try {
    docker build --tag nitropatcher3ds-build:20241104 .
    if ($LASTEXITCODE -ne 0) {
        throw "Build environment setup failed with exit code $LASTEXITCODE."
    }

    docker run --rm --mount "type=bind,source=$projectRoot,target=/work" --workdir /work nitropatcher3ds-build:20241104 make verify "DIAGNOSTIC=$([int]$Diagnostic.IsPresent)" "LAUNCH_TRACE=$([int]$LaunchTrace.IsPresent)" "PREPARE_TRACE=$([int]$PreparationTrace.IsPresent)"
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    & ./vendor/TWiLightMenu/booter/make_cia.exe --srl=out/NitroPatcher3DS.nds --micro=1
    if ($LASTEXITCODE -ne 0) {
        throw "CIA packaging failed with exit code $LASTEXITCODE."
    }
    Compress-Archive -Path out/NitroPatcher3DS.cia,out/licenses -DestinationPath out/NitroPatcher3DS.zip -Force
}
finally {
    Pop-Location
}
