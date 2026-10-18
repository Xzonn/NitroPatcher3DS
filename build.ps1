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

    $containerUser = @()
    if ($IsLinux) {
        $containerUser = @("--user", "$(id -u):$(id -g)")
    }
    docker run --rm @containerUser --mount "type=bind,source=$projectRoot,target=/work" --workdir /work nitropatcher3ds-build:20241104 make verify "DIAGNOSTIC=$([int]$Diagnostic.IsPresent)" "LAUNCH_TRACE=$([int]$LaunchTrace.IsPresent)" "PREPARE_TRACE=$([int]$PreparationTrace.IsPresent)"
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    $ciaTool = "./vendor/TWiLightMenu/booter/make_cia.exe"
    if ($IsLinux) {
        $ciaTool = "./vendor/TWiLightMenu/booter/make_cia"
        chmod +x $ciaTool
        if ($LASTEXITCODE -ne 0) {
            throw "CIA tool setup failed with exit code $LASTEXITCODE."
        }
    }
    & $ciaTool --srl=out/NitroPatcher3DS.nds --micro=1
    if ($LASTEXITCODE -ne 0) {
        throw "CIA packaging failed with exit code $LASTEXITCODE."
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    foreach ($target in @(
        @{ Package = "NitroPatcher3DS.zip"; Source = "NitroPatcher3DS.cia"; Entry = "NitroPatcher3DS.cia" },
        @{ Package = "NitroPatcherDSi.zip"; Source = "NitroPatcher3DS.nds"; Entry = "nitro-patcher/app/NitroPatcher3DS.nds" }
    )) {
        $package = [IO.Compression.ZipArchive]::new(
            [IO.File]::Create("$projectRoot/out/$($target.Package)"),
            [IO.Compression.ZipArchiveMode]::Create
        )
        try {
            [void]$package.CreateEntry("nitro-patcher/")
            [void]$package.CreateEntry("nitro-patcher/patches/")
            [void]$package.CreateEntry("nitro-patcher/patches/Place patches here")
            if ($target.Source.EndsWith(".nds")) {
                [void]$package.CreateEntry("nitro-patcher/app/")
            }
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $package, "$projectRoot/out/$($target.Source)", $target.Entry
            )
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $package, "$projectRoot/docs/usage.md", "使用说明.md"
            )
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $package, "$projectRoot/docs/usage.en.md", "User Guide.md"
            )
        }
        finally {
            $package.Dispose()
        }
    }
}
finally {
    Pop-Location
}
