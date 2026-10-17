#include "progress.h"
#include "../patch/core.h"
#include <nds.h>
#include <cstdio>

namespace {

volatile uint16_t* bitmap;
std::string lastStage;
std::string lastDetail;
uint32_t lastCompleted;
uint32_t lastTotal;
uint64_t started;
uint64_t rendered;

void drawBar(uint32_t completed, uint32_t total)
{
    unsigned int filled = total ? std::min<uint64_t>(112, uint64_t(completed) * 112 / total) : 0;
    for (unsigned int y = 56; y < 70; y++)
    {
        for (unsigned int pair = 0; pair < 114; pair++)
        {
            uint16_t color = y == 56 || y == 69 || pair == 0 || pair == 113 ? 19 : pair <= filled ? 17 : 18;
            bitmap[y * 128 + 7 + pair] = color | (color << 8);
        }
    }
}

void showProgress(const char* stage, uint32_t completed, uint32_t total, bool bytes, const char* detail)
{
    uint64_t now = np3d::preparationMilliseconds();
    bool changed = lastStage != stage || lastDetail != detail || lastTotal != total || completed < lastCompleted;
    if (!changed && now - rendered < 100 && (completed != total || completed == lastCompleted)) return;
    lastStage = stage;
    lastDetail = detail;
    lastCompleted = completed;
    lastTotal = total;
    rendered = now;

    iprintf("\x1b[3;0H\x1b[2K%.31s", stage);
    iprintf("\x1b[5;0H\x1b[2K");
    if (total) iprintf("%u%%", unsigned(uint64_t(completed) * 100 / total));
    else iprintf("Working...");
    iprintf("\x1b[10;0H\x1b[2K");
    if (bytes)
    {
        if (total >= 1024 * 1024)
        {
            iprintf("%u.%u / %u.%u MiB", unsigned(completed / 1048576), unsigned((completed % 1048576) * 10 / 1048576),
                unsigned(total / 1048576), unsigned((total % 1048576) * 10 / 1048576));
        }
        else iprintf("%u / %u bytes", unsigned(completed), unsigned(total));
    }
    else if (total) iprintf("%u / %u items", unsigned(completed), unsigned(total));
    iprintf("\x1b[12;0H\x1b[2K%.31s", detail);
    iprintf("\x1b[14;0H\x1b[2KElapsed: %u s", unsigned((now - started) / 1000));
    drawBar(completed, total);
    iprintf("\x1b[18;0H\x1b[J");
}

}

void beginPatchProgress()
{
    consoleClear();
    iprintf("Preparing patch cache\n");
    iprintf("\x1b[16;0HKeep the Game Card inserted.");
    videoSetModeSub(MODE_5_2D | DISPLAY_BG0_ACTIVE);
    int background = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 4, 0);
    bgSetPriority(background, 3);
    bgUpdate();
    bitmap = bgGetGfxPtr(background);
    dmaFillHalfWords(0, const_cast<uint16_t*>(bitmap), 65536);
    BG_PALETTE_SUB[17] = RGB15(4, 25, 12);
    BG_PALETTE_SUB[18] = RGB15(5, 5, 7);
    BG_PALETTE_SUB[19] = RGB15(18, 18, 20);
    lastStage.clear();
    lastDetail.clear();
    lastCompleted = lastTotal = 0;
    started = rendered = np3d::preparationMilliseconds();
    np3d::setProgressHandler(showProgress);
}

void finishPatchProgress(bool reused)
{
    np3d::progress(reused ? "Patch cache reused" : "Patch cache prepared", 1, 1);
    np3d::setProgressHandler(nullptr);
}
