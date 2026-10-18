#include "patched_launch.h"
#include "../../vendor/nds-bootstrap/retail/common/include/np3d_rom.h"
#include "common/nds_loader_arm9.h"
#include "resources.h"
#include <nds.h>
#include <cstring>
#include <sys/stat.h>

namespace {

uint32_t cluster(const std::string& path, uint32_t* size = nullptr)
{
    struct stat info;
    np3d::require(stat(path.c_str(), &info) == 0 && (info.st_size == 0 || (info.st_ino >= 2 && info.st_ino < 0x0ffffff0)),
        "Cannot locate launch file: " + path);
    if (size) *size = info.st_size;
    return info.st_size ? info.st_ino : 0;
}

}

void writePatchedLaunch(const np3d::Prepared& prepared, np3d::SourcePtr card, uint32_t chipId, int consoleModel)
{
    using namespace np3d;
    makeDirectory("sd:/nitro-patcher/config");
    makeDirectory("sd:/nitro-patcher/runtime");
    writeResource("bootstrap.nds", "sd:/nitro-patcher/runtime/bootstrap.nds");
    Np3dLaunch launch = {};
    launch.magic = NP3D_LAUNCH_MAGIC;
    launch.version = 1;
    launch.romSize = prepared.rom->size;
    launch.cardSize = card->size;
    launch.cardId = chipId;
    require(prepared.directory.size() < sizeof(launch.cachePath), "Cache path exceeds launch descriptor capacity");
    std::memcpy(launch.cachePath, prepared.directory.c_str(), prepared.directory.size() + 1);
    {
        auto prefix = readBytes(card, 0, 0x8000);
        std::memcpy(&launch.cardControl, prefix.data() + 0x60, 4);
        std::memcpy(&launch.cardCode, prefix.data() + 12, 4);
        Disk output("sd:/nitro-patcher/runtime/card-prefix.bin", "wb");
        output.write(0, prefix.data(), prefix.size());
        output.sync();
    }
    launch.mapCluster = cluster(prepared.directory + "/map.bin", &launch.mapSize);
    launch.dataCluster = cluster(prepared.directory + "/data.bin", &launch.dataSize);
    launch.prefixCluster = cluster("sd:/nitro-patcher/runtime/card-prefix.bin");
    {
        Disk descriptor("sd:/nitro-patcher/config/launch.bin", "wb");
        descriptor.write(0, &launch, sizeof(launch));
        descriptor.sync();
    }
    const std::string configuration =
        "[NDS-BOOTSTRAP]\nNDS_PATH=np3d:/rom.nds\nSAVE_RELOCATION=0\nDSI_MODE=0\nCONSOLE_MODEL=" + std::to_string(consoleModel) + "\n"
        "BOOST_CPU=1\nBOOST_VRAM=0\nASYNC_CARD_READ=-1\nCARD_READ_DMA=-1\nDEBUG=0\nLOGGING=0\n";
    Disk settings("sd:/nitro-patcher/config/nds-bootstrap.ini", "wb");
    settings.write(0, configuration.data(), configuration.size());
    settings.sync();
#if NP3D_LAUNCH_TRACE
    static const char stage[] = "launcher: launch files ready; starting bootstrap\n";
    Disk trace("sd:/nitro-patcher/logs/launch.log", "wb");
    trace.write(0, stage, sizeof(stage) - 1);
    trace.sync();
#endif
}

void startPatchedGame()
{
    const char* arguments[] = {"sd:/nitro-patcher/runtime/bootstrap.nds"};
    int result = runNdsFile(arguments[0], 1, arguments, isDSiMode(), !isDSiMode(), true, false, isDSiMode(), false, false, -1, 0);
    np3d::fail("Cannot start patched game: " + std::to_string(result));
}
