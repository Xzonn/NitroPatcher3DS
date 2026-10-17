# NitroPatcher3DS

在 3DS／2DS 上使用正版 NDS 卡带和 `.xzp` 汉化补丁的项目。

[开发方案](docs/development-plan.md)包含代码复用、补丁处理、运行读取、卡带存档和持久缓存设计。

## 当前阶段

第一阶段已实现卡带信息显示和原卡带启动入口，复用 TWiLight Menu++ 的 Slot-1 加载流程。nds-bootstrap 已接入项目构建。当前安装包为原卡带启动程序。

完整构建、NDS 头部和图标校验、ARM9／ARM7 打包内容比较已通过，CIA 内容摘要与生成的 NDS 一致。真机启动及原卡带存档读写仍待硬件验收。

第二阶段实现 `.xzp` 准备、虚拟 ROM 映射和持久缓存。

## 构建

安装 Docker Desktop 并启动 Linux 容器服务，然后执行：

```powershell
./build.ps1
```

构建使用参考项目的 `devkitpro/devkitarm:20241104` 环境，并安装编译 lzss 所需的宿主 C 编译器。构建过程检查 NDS 头信息，并比较安装包中的 ARM9／ARM7 与链接产物。

| 产物 | 内容 |
| --- | --- |
| `out/NitroPatcher3DS.cia` | 原卡带启动程序安装包 |
| `out/NitroPatcher3DS.nds` | DS／DSi 程序 |
| `out/NitroPatcher3DS.arm9.elf`、`out/NitroPatcher3DS.arm7.elf` | 启动程序调试符号 |
| `out/nds-bootstrap.nds` | nds-bootstrap 参考引擎构建结果 |

## 参考源码

TWiLight Menu++：https://github.com/DS-Homebrew/TWiLightMenu ，提交 `c69b06a6f194e0151746b04759e84cee24e8b401`。

nds-bootstrap：https://github.com/DS-Homebrew/nds-bootstrap ，提交 `eb7feabbbb1e1ddbf46aa92dc453416205105444`。

补丁行为依据 nitro-patcher：https://github.com/Xzonn/nitro-patcher ，提交 `e9239aeddad3f185f67b5d4c0e15c0a488bffc4d`。

各上游源码的许可证保存在对应 `vendor/` 目录。
