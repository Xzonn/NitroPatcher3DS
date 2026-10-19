#include <nds.h>
#include <fat.h>
#include <nds/arm9/dldi.h>
#include "common/nitrofs.h"
#include <cstdio>
#include <cstring>
#if NP3D_DIAGNOSTIC
#include <sys/stat.h>
#endif
#include "launch_engine.h"
#include "card_source.h"
#include "progress.h"
#include "environment.h"
#include "patched_launch.h"
#include "ui.h"
#include "i18n.h"

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

static void stopWithMessage(const char* message) {
    if (ui::ready()) ui::fatal(message);
    consoleDemoInit();
    iprintf("\n%s\n",message);
    while (true) swiWaitForVBlank();
}

#if NP3D_DIAGNOSTIC
static void prepareDiagnostic() {
    np3d::makeDirectory("sd:/nitro-patcher/logs");
    FILE* output=std::fopen("sd:/nitro-patcher/logs/NP3DTRACE.BIN","wb");
    np3d::require(output,"Unable to create NP3DTRACE.BIN.");
    static const unsigned char empty[4096]={};
    for (unsigned int offset=0;offset<0x408000;offset+=sizeof(empty))
        np3d::require(std::fwrite(empty,1,sizeof(empty),output)==sizeof(empty),"Unable to allocate NP3DTRACE.BIN.");
    np3d::require(std::fclose(output)==0,"Unable to close NP3DTRACE.BIN.");
    struct stat info;
    np3d::require(stat("sd:/nitro-patcher/logs/NP3DTRACE.BIN",&info)==0 && info.st_size==0x408000 && info.st_ino>=2,
        "Unable to locate NP3DTRACE.BIN.");
    diagnosticCluster=info.st_ino;runCardEngine=true;
}
#endif

static void resetGameCard() {
    sysSetCardOwner(BUS_OWNER_ARM7);
    fifoSendValue32(FIFO_USER_08,1);
    fifoWaitValue32(FIFO_USER_08);
    np3d::require(fifoGetValue32(FIFO_USER_08)==0,"Cannot reset the Game Card");
    for (int frame=0;frame<30;frame++) swiWaitForVBlank();
    sysSetCardOwner(BUS_OWNER_ARM9);
}

int main() {
    np3d::setFatalHandler(stopWithMessage);
    fifoSendValue32(FIFO_PM,PM_REQ_SLEEP_DISABLE);
    consoleDemoInit();
#if NP3D_SLOT2
    np3d::require(!isDSiMode() && (dldiGetInternal()->features & FEATURE_SLOT_GBA),
        "Launch this build in DS mode from a Slot-2 flashcard.");
    np3d::require(fatMountSimple("sd",dldiGetInternal()),"Unable to access Slot-2 storage.");
    np3d::require(bootFSInit("sd:/BOOT.NDS"),"Cannot open BOOT.NDS embedded resources.");
#else
    if (!isDSiMode()) stopWithMessage("Launch in DSi mode on a DSi or 3DS.");
#endif
    ui::showStartup();
    fifoWaitValue32(FIFO_USER_05);
    int consoleModel=fifoGetValue32(FIFO_USER_05);
    fifoWaitValue32(FIFO_USER_06);
    uint32_t arm7Scfg=fifoGetValue32(FIFO_USER_06);
    np3d::require(!isDSiMode() || consoleModel!=0 || arm7Scfg!=0,
        "DSi hardware access is locked. Start this app directly from Unlaunch.");
    if (isDSiMode() && !fatInitDefault()) stopWithMessage("Unable to access the SD card.");
    np3d::makeDirectory("sd:/nitro-patcher");
    ui::initialize();
    ui::chooseInitialLanguage();

    alignas(4) unsigned char header[0x200];
    bool hasCard=false, cardInterfaceReady=false, reinsert=false;
    const auto* gameHeader=reinterpret_cast<const sNDSHeader*>(header);
    np3d::SourcePtr persistentCard;
    uint32_t persistentChipId=0;
    ui::noCard();

    while (true) {
        swiWaitForVBlank();
        ui::Action action=ui::update();
        if (action==ui::Action::None) continue;
        if (action==ui::Action::Report) {
            ui::report(true);dumpEnvironment();ui::report(false);continue;
        }
        if (action==ui::Action::RepairStorage) {ui::repairStorage();continue;}
        if (action==ui::Action::ChangeCard) {
            persistentCard.reset();persistentChipId=0;hasCard=false;reinsert=true;
            ui::noCard(false,true);continue;
        }
        if (action==ui::Action::ReadCard) {
            if (isDSiMode()) {
                if (!cardInterfaceReady) {
                    fifoSendValue32(FIFO_USER_02,1);
                    fifoSendValue32(FIFO_USER_07,1);
                    fifoSendValue32(FIFO_USER_01,1);
                    fifoWaitValue32(FIFO_USER_03);
                    for (int frame=0;frame<30;frame++) swiWaitForVBlank();
                    sysSetCardOwner(BUS_OWNER_ARM9);cardInterfaceReady=true;
                } else resetGameCard();
            } else sysSetCardOwner(BUS_OWNER_ARM9);
            std::memset(header,0xff,sizeof(header));
            cardReadHeader(header);
            if (swiCRC16(0xffff,header,0x15e)!=gameHeader->headerCRC16) {
                ui::noCard(true,reinsert);continue;
            }
            np3d::require(gameHeader->unitCode!=3,"This Game Card requires DSi mode.");
#if NP3D_DIAGNOSTIC
            np3d::require(std::memcmp(gameHeader->gameCode,"IPGJ",4)==0 && gameHeader->headerCRC16==0x3c60,
                "This diagnostic build requires IPGJ-3C60.");
#endif
            auto card=openGameCard(&persistentChipId);
            if (!isDSiMode()) persistentCard=card;
            hasCard=true;reinsert=false;ui::setGame(card,*gameHeader);continue;
        }
        if (!hasCard) continue;
        if (isDSiMode()) resetGameCard();
        alignas(4) unsigned char currentHeader[0x200];
        if (persistentCard) {
            if (!gameCardMatches(persistentCard)) {
                persistentCard.reset();persistentChipId=0;hasCard=false;reinsert=true;
                ui::noCard(true,true);continue;
            }
        } else {
            cardReadHeader(currentHeader);
            if (std::memcmp(header,currentHeader,sizeof(header))!=0) {
                hasCard=false;reinsert=true;ui::noCard(true,true);continue;
            }
        }
        if ((action==ui::Action::Launch && !ui::selectedPatches().empty()) || !isDSiMode()) {
            auto paths=action==ui::Action::Launch?ui::selectedPatches():std::vector<std::string>();
            std::string description;
            for (const auto& path : paths) {if (!description.empty()) description+=" -> ";description+=path;}
            np3d::beginPreparationLog("sd:/nitro-patcher",description);
            beginPatchProgress();
            bool reused=false;
            bool mismatch=false;
            std::string error;
            {
                uint32_t chipId=persistentChipId;
                auto card=persistentCard?persistentCard:openGameCard(&chipId);
                planPatchProgress(card,paths);
                np3d::Prepared prepared;
                if (paths.empty()) {
                    prepared.directory="sd:/nitro-patcher/runtime/original";
                    np3d::makeDirectory("sd:/nitro-patcher/runtime");
                    np3d::makeDirectory(prepared.directory);
                    auto payload=std::make_shared<np3d::Disk>(prepared.directory+"/data.bin","w+b");
                    prepared.rom=std::make_shared<np3d::Map>(prepared.directory+"/map.bin",payload,card,true);
                    prepared.rom->append(np3d::Kind::Card,0,card->size);
                    prepared.rom->finish();payload->sync();
                } else prepared=np3d::prepare(card,paths,"sd:/nitro-patcher");
                error=prepared.error;reused=prepared.reused;mismatch=prepared.mismatch;
                if (error.empty()) {writePatchedLaunch(prepared,card,chipId,consoleModel);finishPatchProgress(reused);}
                else np3d::setProgressHandler(nullptr);
            }
            np3d::finishPreparationLog(reused,error);
            if (!error.empty()) {ui::error(error,mismatch);continue;}
            startPatchedGame();
        }
#if NP3D_DIAGNOSTIC
        prepareDiagnostic();
#endif
        if (isDSiMode() && !TWLCLK && (gameHeader->unitCode==0 || !TWLMODE)) {
            fifoSendValue32(FIFO_USER_04,1);REG_SCFG_CLK=0x80;swiWaitForVBlank();
        }
        const bool browser=std::memcmp(gameHeader->gameCode,"UBRP",4)==0;
        const bool alternate=browser || std::memcmp(gameHeader->gameCode,"AMFE",4)==0 || std::memcmp(gameHeader->gameCode,"ALXX",4)==0;
        runLaunchEngine(alternate,browser);
    }
}
