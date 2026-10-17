#include <nds.h>
#include <fat.h>

#include <cstdio>
#include <cstring>
#if NP3D_DIAGNOSTIC
#include <sys/stat.h>
#endif

#include "launch_engine.h"
#include "card_source.h"
#include "progress.h"
#include "environment.h"

bool scfgUnlock = false;
int TWLMODE = 0;
bool TWLCLK = false;
int TWLVRAM = 0;
bool TWLTOUCH = false;
bool soundFreq = false;
bool sleepMode = true;
bool runCardEngine = false;
#if NP3D_DIAGNOSTIC
unsigned long diagnosticCluster = 0;
#endif
bool EnableSD = true;
int language = -1;

#if NP3D_DIAGNOSTIC
static void stopWithMessage(const char *message);

static void prepareDiagnostic()
{
    np3d::makeDirectory("sd:/nitro-patcher/logs");
    FILE *output = std::fopen("sd:/nitro-patcher/logs/NP3DTRACE.BIN", "wb");
    if (!output)
    {
        stopWithMessage("Unable to create NP3DTRACE.BIN.");
    }
    static const unsigned char empty[4096] = {};
    for (unsigned int offset = 0; offset < 0x408000; offset += sizeof(empty))
    {
        if (std::fwrite(empty, 1, sizeof(empty), output) != sizeof(empty))
        {
            stopWithMessage("Unable to allocate NP3DTRACE.BIN.");
        }
    }
    if (std::fclose(output) != 0)
    {
        stopWithMessage("Unable to close NP3DTRACE.BIN.");
    }
    struct stat info;
    if (stat("sd:/nitro-patcher/logs/NP3DTRACE.BIN", &info) != 0 || info.st_size != 0x408000 || info.st_ino < 2)
    {
        stopWithMessage("Unable to locate NP3DTRACE.BIN.");
    }
    diagnosticCluster = info.st_ino;
    runCardEngine = true;
}
#endif

static void stopWithMessage(const char *message)
{
    iprintf("\n%s\n", message);
    while (true)
    {
        swiWaitForVBlank();
    }
}

static void runEnvironmentDump()
{
    consoleClear();
    iprintf("Collecting environment report...\n");
    dumpEnvironment();
    stopWithMessage("Saved nitro-patcher/logs/\nenvironment.log\nRestart the app to return.");
}

int main()
{
    np3d::setFatalHandler(stopWithMessage);
    fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_DISABLE);
    consoleDemoInit();
    iprintf("NitroPatcher3DS by Xzonn\n\n");

    if (!isDSiMode())
    {
        stopWithMessage("Launch in DSi mode on a 3DS.");
    }

    if (!fatInitDefault())
    {
        stopWithMessage("Unable to access the SD card.");
    }
    auto patches = np3d::listPatches("sd:/nitro-patcher");
    unsigned int selectedPatch = 0;

    fifoSendValue32(FIFO_USER_02, 1);
    fifoSendValue32(FIFO_USER_07, 1);
    fifoSendValue32(FIFO_USER_01, 1);
    fifoWaitValue32(FIFO_USER_03);

    for (int frame = 0; frame < 30; frame++)
    {
        swiWaitForVBlank();
    }

    sysSetCardOwner(BUS_OWNER_ARM9);
    scanKeys();
    if (keysHeld() & KEY_Y)
    {
        runEnvironmentDump();
    }
    alignas(4) unsigned char header[0x200];
    cardReadHeader(header);
    if (header[0] == 0xff && header[1] == 0xff && header[2] == 0xff && header[3] == 0xff)
    {
        stopWithMessage("Insert a DS Game Card and restart.");
    }
    if (swiCRC16(0xffff, header, 0x15e) != reinterpret_cast<sNDSHeader *>(header)->headerCRC16)
    {
        stopWithMessage("Unable to read the Game Card header.");
    }
    const auto *gameHeader = reinterpret_cast<const sNDSHeader *>(header);
    if (gameHeader->unitCode == 3)
    {
        stopWithMessage("This Game Card requires DSi mode.");
    }
#if NP3D_DIAGNOSTIC
    if (std::memcmp(gameHeader->gameCode, "IPGJ", 4) != 0 || gameHeader->headerCRC16 != 0x3c60)
    {
        stopWithMessage("This diagnostic build requires IPGJ-3C60.");
    }
#endif

    char title[13] = {};
    char gameCode[5] = {};
    std::memcpy(title, gameHeader->gameTitle, 12);
    std::memcpy(gameCode, gameHeader->gameCode, 4);
    iprintf("%s\n%s\n\n", title, gameCode);
#if NP3D_DIAGNOSTIC
    iprintf("Diagnostic build\n");
    iprintf("Log: nitro-patcher/logs/\nNP3DTRACE.BIN\n");
    iprintf("After black screen, wait 15s.\n");
    iprintf("Hold L+R+Up+X for 2s, wait 5s,\nthen power off and copy the log.\n\n");
#endif
    iprintf("A: Start Game Card\n");
    iprintf("Y: Environment report\n");
    if (patches.empty())
    {
        iprintf("Place .xzp files in\nnitro-patcher/patches/\n");
    }
    else
    {
        iprintf("Up/Down: Select patch\nX: Prepare patch cache\n");
    }
    bool redrawPatch = !patches.empty();

    while (true)
    {
        swiWaitForVBlank();
        scanKeys();
        unsigned int pressed = keysDown();
        if (pressed & KEY_Y)
        {
            runEnvironmentDump();
        }
        if (!patches.empty() && (pressed & (KEY_UP | KEY_DOWN)))
        {
            selectedPatch = (selectedPatch + patches.size() + (pressed & KEY_DOWN ? 1 : -1)) % patches.size();
            redrawPatch = true;
        }
        if (redrawPatch)
        {
            iprintf("\x1b[20;0H\x1b[J%u/%u %s\n", selectedPatch + 1, unsigned(patches.size()),
                patches[selectedPatch].substr(patches[selectedPatch].find_last_of('/') + 1).c_str());
            redrawPatch = false;
        }
        if ((pressed & KEY_A) || (!patches.empty() && (pressed & KEY_X)))
        {
            alignas(4) unsigned char currentHeader[0x200];
            cardReadHeader(currentHeader);
            if (std::memcmp(header, currentHeader, sizeof(header)) != 0)
            {
                stopWithMessage("The Game Card changed. Restart the app.");
            }
            if (!patches.empty() && (pressed & KEY_X))
            {
                np3d::beginPreparationLog("sd:/nitro-patcher", patches[selectedPatch]);
                beginPatchProgress();
                bool reused;
                {
                    auto card = openGameCard();
                    auto prepared = np3d::prepare(card, patches[selectedPatch], "sd:/nitro-patcher");
                    finishPatchProgress(prepared.reused);
                    reused = prepared.reused;
                }
                np3d::finishPreparationLog(reused);
                stopWithMessage("Restart the app to return.\nCache preparation is complete.");
            }
#if NP3D_DIAGNOSTIC
            prepareDiagnostic();
#endif

            if (!TWLCLK && (gameHeader->unitCode == 0 || !TWLMODE))
            {
                fifoSendValue32(FIFO_USER_04, 1);
                REG_SCFG_CLK = 0x80;
                swiWaitForVBlank();
            }

            const bool alternateLoader = std::memcmp(gameCode, "UBRP", 4) == 0 || std::memcmp(gameCode, "AMFE", 4) == 0 || std::memcmp(gameCode, "ALXX", 4) == 0;
            runLaunchEngine(alternateLoader, std::memcmp(gameCode, "UBRP", 4) == 0);
        }
    }
}
