# 构建 NitroPatcherDS

## 环境

安装 Git、PowerShell 7 和 Docker。Windows 使用 Docker Desktop，并启用 Linux 容器；Linux 使用 Docker Engine。构建前启动 Docker 服务。

编译使用 `devkitpro/devkitarm:20241104` 容器。构建脚本会创建所需镜像，并按 Dockerfile 安装依赖，无需在主机安装 devkitARM。

## 获取源码

```sh
git clone --recursive https://github.com/Xzonn/NitroPatcher3DS.git NitroPatcherDS
cd NitroPatcherDS
```

已有源码时，确认子模块已初始化：

```sh
git submodule update --init --recursive
```

## 构建与打包

在项目根目录使用 PowerShell 执行：

```powershell
./build.ps1
```

脚本会编译 3DS／DSi 和 DS 两个程序，校对打包后的 ARM9／ARM7 二进制，生成 3DS CIA，并分别打包三个平台的 ZIP。每个安装包包含补丁目录及中英文使用说明。

| 产物                                                           | 内容                                                                  |
| -------------------------------------------------------------- | --------------------------------------------------------------------- |
| `out/NitroPatcher3DS.zip`                                      | 3DS／2DS 安装包                                                       |
| `out/NitroPatcherDSi.zip`                                      | DSi 安装包                                                            |
| `out/NitroPatcherDS.zip`                                       | DS／DS Lite 安装包                                                    |
| `out/NitroPatcher3DS.cia`                                      | 3DS／2DS 安装程序                                                     |
| `out/NitroPatcherDSi.nds`                                      | 3DS／DSi 程序；DSi 包内路径为 `nitro-patcher/app/NitroPatcherDSi.nds` |
| `out/NitroPatcherDS.nds`                                       | DS／Slot-2 程序；安装包内命名为 `BOOT.NDS`                            |
| `out/NitroPatcherDSi.arm9.elf`、`out/NitroPatcherDSi.arm7.elf` | 3DS／DSi 程序的调试符号                                               |
| `out/NitroPatcherDS.arm9.elf`、`out/NitroPatcherDS.arm7.elf`   | DS 程序的调试符号                                                     |
| `out/nds-bootstrap.nds`                                        | 内嵌启动引擎的构建结果                                                |

首次构建需要下载容器和依赖。后续构建会复用 Docker 镜像与编译中间文件。

## 诊断构建

需要定位运行问题时，可以启用对应诊断选项：

```powershell
./build.ps1 -Diagnostic
./build.ps1 -LaunchTrace
./build.ps1 -PreparationTrace
```

`-Diagnostic` 启用游戏运行时读取诊断，`-LaunchTrace` 启用启动流程诊断，`-PreparationTrace` 启用补丁准备的详细跟踪。三个选项可以组合使用。常规构建默认关闭这些选项。
