#include "card_source.h"
#include <nds.h>
#include <cstring>

extern "C" {
#include "read_card.h"
}
extern void encrypt_arm9(uint32_t gameCode, unsigned char* data);

namespace {

struct GameCard final : np3d::Source {
    alignas(4) uint8_t prefix[0x8000] = {};
    alignas(4) uint8_t headerData[0x1000] = {};
    alignas(4) uint32_t block[128];
    alignas(4) uint32_t identity[128];
    uint32_t blockOffset = UINT32_MAX;

    GameCard(uint32_t* chipId) {
        using namespace np3d;
        auto& header = *reinterpret_cast<sNDSHeaderExt*>(headerData);
        progress("Reading Game Card header", 0, 0x1000, true);
        sysSetCardOwner(BUS_OWNER_ARM9);
        uint32_t id = cardReadID(CARD_CLK_SLOW);
        uint32_t control = CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_DELAY1(0x1fff) | CARD_DELAY2(0x3f);
        if (id & 0x80000000) {
            for (uint32_t offset = 0; offset < 0x1000; offset += 512) {
                cardParamCommand(CARD_CMD_HEADER_READ, offset, control | CARD_BLK_SIZE(1),
                    reinterpret_cast<uint32_t*>(prefix + offset), 512 / sizeof(uint32_t));
            }
        } else {
            cardParamCommand(CARD_CMD_HEADER_READ, 0, control | CARD_BLK_SIZE(4),
                reinterpret_cast<uint32_t*>(prefix), 0x1000 / sizeof(uint32_t));
        }
        progress("Initializing Game Card", 0, 0);
        require(cardInit(&header, &id) == 0, "Cannot initialize Game Card for patch preparation");
        if (chipId) *chipId = id;
        require(header.deviceSize <= 13, "Game Card exceeds supported ROM size");
        size = 0x20000u << header.deviceSize;
        progress("Reading secure area", 0, 0x4000, true);
        cardRead(0x4000, reinterpret_cast<uint32_t*>(prefix + 0x4000), 0x4000);
        progress("Checking secure area", 0, 0);
        std::vector<uint8_t> secure(prefix + 0x4000, prefix + 0x8000);
        uint32_t code;
        std::memcpy(&code, header.gameCode, sizeof(code));
        encrypt_arm9(code, secure.data());
        require(swiCRC16(0xffff, secure.data(), secure.size()) == header.secureCRC16,
            "Game Card secure area checksum mismatch");
        if (!isDSiMode()) cardRead(0x8000,identity,sizeof(identity));
    }

    void read(uint32_t offset, void* output, uint32_t length) override {
        using namespace np3d;
        require(offset <= size && length <= size - offset, "Read outside Game Card");
        auto bytes = static_cast<uint8_t*>(output);
        if (offset < sizeof(prefix)) {
            uint32_t take = std::min<uint32_t>(length, sizeof(prefix) - offset);
            std::memcpy(bytes, prefix + offset, take);
            offset += take;
            bytes += take;
            length -= take;
        }
        while (length) {
            uint32_t base = offset & ~511u;
            uint32_t take = std::min(length, 512 - offset % 512);
            if (base != blockOffset) {
                cardRead(base, block, sizeof(block));
                blockOffset = base;
            }
            std::memcpy(bytes, reinterpret_cast<uint8_t*>(block) + offset % 512, take);
            bytes += take;
            offset += take;
            length -= take;
        }
    }
};

}

np3d::SourcePtr openGameCard(uint32_t* chipId) { return std::make_shared<GameCard>(chipId); }
bool gameCardMatches(np3d::SourcePtr card) {
    alignas(4) uint32_t current[128];
    cardRead(0x8000,current,sizeof(current));
    return std::memcmp(std::static_pointer_cast<GameCard>(card)->identity,current,sizeof(current))==0;
}
