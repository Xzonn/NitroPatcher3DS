# NitroPatcher3DS

本项目可以在 3DS／2DS 和 DSi／DSi LL（XL）上直接对正版 NDS 卡带使用汉化补丁。

## 构建

安装 Docker Desktop 并启动 Linux 容器服务，然后执行：

```powershell
./build.ps1
```

构建需要使用 `devkitpro/devkitarm` 的 Docker 环境，并安装编译所需依赖环境。

| 产物                                                           | 内容                           |
| -------------------------------------------------------------- | ------------------------------ |
| `out/NitroPatcher3DS.zip`                                      | 3DS／2DS 安装包，包含 CIA 和中英文说明 |
| `out/NitroPatcherDSi.zip`                                      | DSi 安装包，通过 Unlaunch 启动，包含中英文说明 |
| `out/NitroPatcher3DS.cia`                                      | 原卡带启动程序安装包           |
| `out/NitroPatcher3DS.nds`                                      | DS／DSi 程序                   |
| `out/NitroPatcher3DS.arm9.elf`、`out/NitroPatcher3DS.arm7.elf` | 启动程序调试符号               |
| `out/nds-bootstrap.nds`                                        | nds-bootstrap 参考引擎构建结果 |

## 参考源码

- [TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu)
- [nds-bootstrap](https://github.com/DS-Homebrew/nds-bootstrap)
