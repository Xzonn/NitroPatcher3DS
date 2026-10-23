# Building NitroPatcherDS

## Requirements

Install Git, PowerShell 7 and Docker. On Windows, use Docker Desktop with Linux containers enabled. On Linux, use Docker Engine. Start the Docker service before building.

Compilation uses the `devkitpro/devkitarm:20241104` container. The build script creates the required image and installs dependencies through the Dockerfile. You do not need to install devkitARM on the host.

## Get the source

```sh
git clone --recursive https://github.com/Xzonn/NitroPatcher3DS.git NitroPatcherDS
cd NitroPatcherDS
```

For an existing checkout, initialize the submodules:

```sh
git submodule update --init --recursive
```

## Build and package

Run this command in PowerShell from the project root:

```powershell
./build.ps1
```

The script builds the 3DS / DSi and DS applications, compares the packaged ARM9 / ARM7 binaries against the linked binaries, creates the 3DS CIA, and packages the three platform ZIPs. Each package includes a patch folder and user guides in English and Chinese.

| Output                                                         | Contents                                                                                                                       |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| `out/NitroPatcher3DS.zip`                                      | 3DS / 2DS package                                                                                                              |
| `out/NitroPatcherDSi.zip`                                      | DSi package                                                                                                                    |
| `out/NitroPatcherDS.zip`                                       | DS / DS Lite package                                                                                                           |
| `out/NitroPatcher3DS.cia`                                      | 3DS / 2DS installer                                                                                                            |
| `out/NitroPatcherDSi.nds`                                      | 3DS / DSi app; packaged as `nitro-patcher/app/NitroPatcher3DS.nds` for 3DS and `nitro-patcher/app/NitroPatcherDSi.nds` for DSi |
| `out/NitroPatcherDS.nds`                                       | DS / Slot-2 app; packaged as `BOOT.NDS`                                                                                        |
| `out/NitroPatcherDSi.arm9.elf`, `out/NitroPatcherDSi.arm7.elf` | Debug symbols for the 3DS / DSi app                                                                                            |
| `out/NitroPatcherDS.arm9.elf`, `out/NitroPatcherDS.arm7.elf`   | Debug symbols for the DS app                                                                                                   |
| `out/nds-bootstrap.nds`                                        | Built launch engine embedded in the app                                                                                        |

The first build downloads the container and dependencies. Later builds reuse the Docker image and intermediate files.

## Diagnostic builds

Enable the relevant option when investigating a runtime problem:

```powershell
./build.ps1 -Diagnostic
./build.ps1 -LaunchTrace
./build.ps1 -PreparationTrace
```

`-Diagnostic` enables runtime read diagnostics, `-LaunchTrace` enables launch tracing, and `-PreparationTrace` enables detailed patch preparation tracing. These options can be combined. All three are disabled by default.
