#include "rom.h"
#include "package.h"
#include "banner.h"
#include "filename.h"
#include "fnt.h"
#include <cstring>
#include <map>
#include <set>

namespace np3d {

struct Allocation { unsigned_int start, end; };
struct RomFile { std::string name; uint32_t id; SourcePtr data; uint32_t original; };

static uint32_t addNamedFile(std::vector<NamedFile>& names, std::vector<NamedDirectory>& directories, const std::string& path) {
    std::vector<std::string> parts;
    for (size_t first = 0;;) {
        size_t end = path.find('/', first);
        parts.push_back(path.substr(first, end == std::string::npos ? end : end - first));
        if (end == std::string::npos) break;
        first = end + 1;
    }
    require(parts.size() >= 2 && normalize(parts.front()) == "data", "Invalid NitroFS path: " + path);
    require(parts.size() <= 258, "FNT directory depth exceeds 256");
    uint32_t parent = 0;
    std::string output = "data";
    for (size_t index = 1; index < parts.size(); index++) {
        const auto& name = parts[index];
        require(!name.empty() && name != "." && name != ".." && name.find('\0') == std::string::npos, "Invalid NitroFS path: " + path);
        std::string key = normalize(name);
        for (auto file : directories[parent].files)
            require(normalize(decodeName(names[file].rawName)) != key, "ROM path conflicts with a file: " + path);
        uint32_t child = UINT32_MAX;
        for (auto folder : directories[parent].children) if (normalize(directories[folder].name) == key) { child = folder; break; }
        if (index + 1 == parts.size()) {
            require(child == UINT32_MAX, "ROM path conflicts with a directory: " + path);
            uint32_t file = names.size();
            names.push_back({output + "/" + name, UINT32_MAX, 0, 0, encodeName(name), parent});
            directories[parent].files.push_back(file);
            return file;
        }
        if (child == UINT32_MAX) {
            require(directories.size() < 4096, "FNT directory count exceeds 4096");
            child = directories.size();
            auto raw = encodeName(name);
            directories.push_back({name, raw, parent, {}, {}});
            directories[parent].children.push_back(child);
        }
        parent = child;
        output += "/" + directories[parent].name;
    }
    fail("Invalid NitroFS path: " + path);
}

static SourcePtr layoutFiles(Workspace& work, SourcePtr input, std::vector<RomFile>& files,
    std::vector<NamedFile>& names, std::vector<NamedDirectory>& directories,
    const std::vector<uint32_t>& positions, std::vector<Allocation>& allocations,
    std::vector<uint32_t>& order, std::vector<uint32_t>& oldIds) {
    std::vector<bool> named(allocations.size(), false), overlays(allocations.size(), false);
    std::vector<uint32_t> existing(allocations.size(), UINT32_MAX);
    for (const auto& name : names) if (name.id != UINT32_MAX) named[name.id] = true;
    for (uint32_t index = 0; index < files.size(); index++) {
        const auto& file = files[index];
        if (file.id == 0xffff || file.id == UINT32_MAX) continue;
        existing[file.id] = index;
        if (file.name.compare(0, 8, "overlay/") == 0) overlays[file.id] = true;
    }
    for (bool overlay : {true, false}) for (uint32_t id = 0; id < allocations.size(); id++) {
        if (named[id] || overlays[id] != overlay) continue;
        uint32_t index = existing[id];
        if (index == UINT32_MAX) {
            index = files.size();
            files.push_back({"FAT entry " + std::to_string(id), id,
                std::make_shared<Slice>(input, allocations[id].start, uint32_t(allocations[id].end) - uint32_t(allocations[id].start)), id});
        }
        order.push_back(index);
    }
    std::vector<uint32_t> pending{0}, orderedDirectories, firstFiles(directories.size());
    while (!pending.empty()) {
        uint32_t index = pending.back();
        pending.pop_back();
        auto& folder = directories[index];
        std::sort(folder.files.begin(), folder.files.end(), [&](uint32_t a, uint32_t b) { return names[a].rawName < names[b].rawName; });
        std::sort(folder.children.begin(), folder.children.end(), [&](uint32_t a, uint32_t b) { return directories[a].rawName < directories[b].rawName; });
        firstFiles[index] = order.size();
        orderedDirectories.push_back(index);
        for (auto file : folder.files) order.push_back(positions[file]);
        for (auto child = folder.children.rbegin(); child != folder.children.rend(); child++) pending.push_back(*child);
    }
    require(order.size() <= 0xf000, "Filesystem file IDs exceed 0xEFFF");
    oldIds.assign(allocations.size(), UINT32_MAX);
    for (uint32_t id = 0; id < order.size(); id++) {
        uint32_t old = files[order[id]].id;
        if (old != UINT32_MAX) oldIds[old] = id;
    }
    for (auto& file : files) if (file.id != 0xffff && file.id != UINT32_MAX) file.id = oldIds[file.id];
    for (uint32_t id = 0; id < order.size(); id++) files[order[id]].id = id;
    std::vector<uint32_t> directoryIds(directories.size());
    for (uint32_t index = 0; index < orderedDirectories.size(); index++) directoryIds[orderedDirectories[index]] = 0xf000 + index;
    auto table = std::make_shared<Disk>(work.directory + "/fnt-" + std::to_string(work.next++) + ".bin", "w+b");
    unsigned int entry = directories.size() * 8;
    for (auto index : orderedDirectories) {
        const auto& folder = directories[index];
        std::vector<FntName> entries;
        for (auto file : folder.files) entries.push_back({names[file].rawName.data(), unsigned(names[file].rawName.size()), 0});
        for (auto child : folder.children) entries.push_back({directories[child].rawName.data(), unsigned(directories[child].rawName.size()), uint16_t(directoryIds[child])});
        uint32_t parent = folder.parent == UINT32_MAX ? directories.size() : directoryIds[folder.parent];
        require(WriteFntDirectory(table->handle, 0, directoryIds[index], entry, firstFiles[index], parent, entries), "Cannot write NDS file name table");
    }
    table->flush();
    table->size = entry;
    table->writing = true;
    auto output = work.map();
    std::vector<uint8_t> buffer(BufferSize);
    for (uint32_t offset = 0; offset < entry;) {
        uint32_t take = std::min(BufferSize, entry - offset);
        table->read(offset, buffer.data(), take);
        output->literal(buffer.data(), take);
        offset += take;
    }
    output->finish();
    return output;
}

std::shared_ptr<Map> overwrite(Workspace& work, SourcePtr input, uint32_t offset, const void* data, uint32_t length) {
    require(offset <= input->size && length <= input->size - offset, "Write outside virtual ROM");
    auto output = work.map();
    output->copy(input, 0, offset);
    output->literal(data, length);
    output->copy(input, offset + length, input->size - offset - length);
    output->finish();
    return output;
}

void secureArea(std::vector<uint8_t>& data, const Header& header, bool decrypted) {
    if (decrypted) {
        require(data.size() >= 0x800, "Secure area is truncated");
        uint32_t code;
        std::memcpy(&code, header.gamecode, sizeof(code));
        encrypt_arm9(code, data.data());
    }
}

static std::vector<uint8_t> bannerBytes(SourcePtr input, uint32_t hardSize) {
    unsigned_short version;
    input->read(0, &version, sizeof(version));
    uint32_t size = CalcBannerSize(version);
    if ((uint16_t(version) >> 8) == 1) size = hardSize && hardSize != 0xffffffff ? std::min<uint32_t>(hardSize, 0x23c0) : 0x23c0;
    auto bytes = readBytes(input, 0, size);
    uint32_t titles = version >= 3 ? 8 : version >= 2 ? 7 : 6;
    for (uint32_t language = 0; language < titles; language++) {
        auto text = reinterpret_cast<unsigned_short*>(bytes.data() + 0x240 + language * 256);
        bool ended = false;
        for (unsigned int index = 0; index < 128; index++) {
            if (ended) text[index] = 0;
            else if (text[index] == 0) ended = true;
        }
    }
    auto checksums = reinterpret_cast<unsigned_short*>(bytes.data() + 2);
    checksums[0] = CalcCrc16(bytes.data() + 0x20, 0x820);
    if (version >= 2) checksums[1] = CalcCrc16(bytes.data() + 0x20, 0x920);
    if (version >= 3) checksums[2] = CalcCrc16(bytes.data() + 0x20, 0xa20);
    if ((uint16_t(version) >> 8) >= 1) {
        require(size >= 0x23c0, "Animated banner is truncated");
        checksums[3] = CalcCrc16(bytes.data() + 0x1240, 0x1180);
    }
    return bytes;
}

std::shared_ptr<Map> buildRom(Workspace& work, SourcePtr input, Package& package) {
    require(input->size >= 512, "NDS header is truncated");
    auto headerStorage = std::make_unique<Header>();
    Header& h = *headerStorage;
    std::memset(&h, 0, sizeof(h));
    input->read(0, &h, std::min<uint32_t>(input->size, sizeof(h)));
    bool format2 = package.metadata.formatVersion == 2;
    if (format2 && h.unitcode != 0) {
        work.error = "XZP format 2 DSi processing is not available in this build";
        return nullptr;
    }
    uint32_t headerSize = h.rom_header_size;
    require(headerSize >= 0x170 && headerSize <= input->size, "Invalid NDS header size");
    if (h.unitcode & 2) require(headerSize >= 0x1000, "DSi header is truncated");
    auto headerData = readBytes(input, 0, headerSize);
    uint32_t marker = 0;
    uint32_t footer = uint32_t(h.arm9_rom_offset) + uint32_t(h.arm9_size);
    if (footer <= input->size - 4) input->read(footer, &marker, sizeof(marker));
    bool nitrocode = marker == 0xdec00621;
    uint64_t secureMarker = 0;
    if (input->size >= 0x4008) input->read(0x4000, &secureMarker, sizeof(secureMarker));
    bool decrypted = secureMarker == UINT64_C(0xe7ffdeffe7ffdeff);

    require(uint32_t(h.fat_size) % sizeof(Allocation) == 0 && uint32_t(h.fat_size) / 8 <= 65535, "Invalid NDS FAT size");
    std::vector<Allocation> allocations(uint32_t(h.fat_size) / sizeof(Allocation));
    input->read(h.fat_offset, allocations.data(), h.fat_size);
    std::vector<uint32_t> sorted;
    for (uint32_t index = 0; index < allocations.size(); index++) {
        auto& item = allocations[index];
        require(item.start <= item.end && item.end <= input->size, "NDS allocation outside ROM");
        sorted.push_back(index);
    }
    std::sort(sorted.begin(), sorted.end(), [&](uint32_t a, uint32_t b) {
        return allocations[a].start == allocations[b].start ? a < b : allocations[a].start < allocations[b].start;
    });
    std::vector<RomFile> files;
    auto add = [&](const std::string& name, uint32_t id, uint32_t offset, uint32_t size) {
        files.push_back({name, id, std::make_shared<Slice>(input, offset, size), id});
    };
    auto bannerInput = std::make_shared<Slice>(input, h.banner_offset, input->size - uint32_t(h.banner_offset));
    auto originalBanner = bannerBytes(bannerInput, h.unitcode & 2 ? uint32_t(h.banner_size) : 0);
    add("header.bin", 0xffff, 0, headerSize);
    add("banner.bin", 0xffff, h.banner_offset, originalBanner.size());
    add("fnt.bin", 0xffff, h.fnt_offset, h.fnt_size);
    add("fat.bin", 0xffff, h.fat_offset, h.fat_size);
    add("arm9.bin", 0xffff, h.arm9_rom_offset, uint32_t(h.arm9_size) + (nitrocode ? 12 : 0));
    add("arm7.bin", 0xffff, h.arm7_rom_offset, h.arm7_size);
    if (h.arm9_overlay_size) add("overarm9.bin", 0xffff, h.arm9_overlay_offset, h.arm9_overlay_size);
    if (h.arm7_overlay_size) add("overarm7.bin", 0xffff, h.arm7_overlay_offset, h.arm7_overlay_size);
    progress("Reading game directory", 0, 0);
    std::vector<NamedDirectory> directories;
    auto names = enumerateFiles(input, h, format2, format2 ? &directories : nullptr);
    std::vector<uint32_t> namedPositions;
    for (const auto& item : names) {
        namedPositions.push_back(files.size());
        add(item.name, item.id, item.offset, item.length);
    }
    for (unsigned int cpu : {9u, 7u}) {
        uint32_t offset = cpu == 9 ? uint32_t(h.arm9_overlay_offset) : uint32_t(h.arm7_overlay_offset);
        uint32_t size = cpu == 9 ? uint32_t(h.arm9_overlay_size) : uint32_t(h.arm7_overlay_size);
        require(size % sizeof(OverlayEntry) == 0, "Invalid NDS overlay table size");
        for (uint32_t position = 0; position < size; position += sizeof(OverlayEntry)) {
            OverlayEntry item;
            input->read(offset + position, &item, sizeof(item));
            if (item.id == 0xffffffff) break;
            require(item.file_id < allocations.size(), "Overlay file ID outside FAT");
            auto& allocation = allocations[item.file_id];
            char name[48];
            std::snprintf(name, sizeof(name), cpu == 9 ? "overlay/overlay_%04u.bin" : "overlay/overlay7_%04u.bin", unsigned(item.id));
            add(name, item.file_id, allocation.start, uint32_t(allocation.end) - uint32_t(allocation.start));
        }
    }
    std::set<std::string> unique;
    for (const auto& file : files) require(unique.insert(normalize(file.name)).second, "Duplicate ROM path: " + file.name);
    std::vector<std::pair<std::string, std::string>> additions;
    if (format2) {
        for (const auto& folder : directories) {
            std::set<std::string> children;
            for (auto file : folder.files) require(children.insert(normalize(decodeName(names[file].rawName))).second, "Duplicate FNT path");
            for (auto child : folder.children) require(children.insert(normalize(directories[child].name)).second, "Duplicate FNT path");
        }
        for (const auto& item : package.entries) {
            bool delta = item.first.compare(0, 7, "xdelta/") == 0;
            std::string target = delta ? item.first.substr(7) : item.first;
            if (target.compare(0, 5, "data/") != 0 || unique.count(target)) continue;
            if (!delta && package.has("xdelta/" + target)) continue;
            additions.push_back({delta ? item.second.name.substr(7) : item.second.name, target});
        }
        std::sort(additions.begin(), additions.end());
        require(additions.empty() || (!package.has("fnt.bin") && !package.has("xdelta/fnt.bin")), "Adding files conflicts with an fnt.bin replacement");
    }
    uint32_t completed = 0;
    uint32_t fileCount = files.size() + additions.size();
    for (auto& file : files) {
        progress("Preparing game files", completed, fileCount, false, file.name.c_str());
        file.data = package.replace(file.name, file.data);
        if (!file.data) return nullptr;
        progress("Preparing game files", ++completed, fileCount, false, file.name.c_str());
    }
    for (const auto& addition : additions) {
        uint32_t file = addNamedFile(names, directories, addition.first);
        progress("Preparing game files", completed, fileCount, false, names[file].name.c_str());
        auto data = package.replace(addition.second, std::make_shared<Slice>(input, 0, 0));
        if (!data) return nullptr;
        namedPositions.push_back(files.size());
        files.push_back({names[file].name, UINT32_MAX, data, UINT32_MAX});
        progress("Preparing game files", ++completed, fileCount, false, names[file].name.c_str());
    }
    auto system = [&](const std::string& name) -> SourcePtr {
        auto found = std::find_if(files.begin(), files.end(), [&](const RomFile& item) { return item.name == name; });
        require(found != files.end(), "Missing system file: " + name);
        return found->data;
    };
    SourcePtr fnt = system("fnt.bin");
    if (package.has("fnt.bin") || package.has("xdelta/fnt.bin")) {
        auto combined = work.map();
        combined->copy(input, 0, input->size);
        combined->copy(fnt, 0, fnt->size);
        combined->finish();
        auto replacementStorage = std::make_unique<Header>(h);
        Header& replacement = *replacementStorage;
        replacement.fnt_offset = input->size;
        replacement.fnt_size = fnt->size;
        auto newNames = enumerateFiles(combined, replacement, format2);
        std::vector<uint32_t> before, after;
        for (const auto& item : names) before.push_back(item.id);
        for (const auto& item : newNames) after.push_back(item.id);
        std::sort(before.begin(), before.end());
        std::sort(after.begin(), after.end());
        require(before == after, "Replacement FNT must preserve existing file IDs");
    }
    bool changed = !additions.empty();
    std::vector<uint32_t> layout, oldIds;
    if (changed) {
        fnt = layoutFiles(work, input, files, names, directories, namedPositions, allocations, layout, oldIds);
        sorted.resize(layout.size());
        for (uint32_t index = 0; index < sorted.size(); index++) sorted[index] = index;
    }
    names.clear();
    names.shrink_to_fit();
    directories.clear();
    directories.shrink_to_fit();
    if (package.has("header.bin") || package.has("xdelta/header.bin")) {
        auto replacementStorage = std::make_unique<Header>();
        Header& replacement = *replacementStorage;
        auto data = system("header.bin");
        require(data->size >= 0x170, "Replacement header is truncated");
        data->read(0, &replacement, std::min<uint32_t>(sizeof(replacement), data->size));
        require(uint32_t(replacement.rom_header_size) >= 0x170 && replacement.rom_header_size <= data->size,
            "Invalid replacement header size");
        std::memcpy(h.title, replacement.title, sizeof(h.title));
        std::memcpy(h.gamecode, replacement.gamecode, sizeof(h.gamecode));
    }
    std::vector<uint8_t> signature;
    uint32_t logicalSize = h.application_end_offset;
    if (logicalSize >= headerSize && logicalSize <= input->size && input->size - logicalSize >= 0x88) {
        uint32_t tag;
        input->read(logicalSize, &tag, sizeof(tag));
        if (tag == 0x00016361) signature = readBytes(input, logicalSize, 0x88);
    }
    auto output = work.map();
    progress("Arranging game files", 0, input->size, true);
    output->copy(input, 0, headerSize);
    std::vector<Allocation> placements(changed ? layout.size() : allocations.size());
    std::vector<RomFile*> byId(placements.size(), nullptr);
    for (auto& file : files) if (file.id != 0xffff && (!byId[file.id] || (changed && file.name.compare(0, 8, "overlay/") == 0))) byId[file.id] = &file;
    std::vector<bool> written(placements.size(), false);
    std::memset(placements.data(), 0, placements.size() * sizeof(Allocation));
    auto pad = [&]() { output->append(Kind::Fill, 255, (512 - output->size % 512) % 512); };
    auto write = [&](SourcePtr data, uint32_t id, bool padding) {
        if (changed && id != 0xffff) {
            if (written[id]) return;
            written[id] = true;
            data = byId[id]->data;
        }
        progress("Arranging game files", 0, data->size, true);
        uint32_t start = output->size;
        output->copy(data, 0, data->size);
        if (id != 0xffff) { placements[id].start = start; placements[id].end = output->size; }
        if (padding) pad();
        progress("Arranging game files", data->size, data->size, true);
    };
    std::set<uint32_t> overlayIds;
    auto overlays = [&](unsigned int cpu) {
        std::string name = cpu == 9 ? "overarm9.bin" : "overarm7.bin";
        if (!unique.count(name)) return;
        auto data = system(name);
        require(data->size % sizeof(OverlayEntry) == 0, "Invalid replacement overlay table");
        std::vector<OverlayEntry> table;
        for (uint32_t position = 0; position < data->size; position += sizeof(OverlayEntry)) {
            OverlayEntry item;
            data->read(position, &item, sizeof(item));
            if (item.id == 0xffffffff) break;
            table.push_back(item);
        }
        std::string prefix = cpu == 9 ? "overlay/overlay_" : "overlay/overlay7_";
        std::vector<RomFile*> payloads;
        for (auto& file : files) if (file.name.compare(0, prefix.size(), prefix) == 0) payloads.push_back(&file);
        std::stable_sort(payloads.begin(), payloads.end(), [](const RomFile* a, const RomFile* b) { return a->original < b->original; });
        require(payloads.size() == table.size(), "Changing overlay file count is unsupported");
        std::map<uint32_t, OverlayEntry*> entries;
        for (auto& item : table) entries.emplace(item.file_id, &item);
        for (const auto* file : payloads) {
            auto found = entries.find(file->original);
            require(found != entries.end(), "Replacement overlay table is missing a file ID");
            auto item = found->second;
            uint32_t size = changed ? byId[file->id]->data->size : file->data->size;
            uint32_t flags = item->reserved;
            if (((flags >> 24) & 1) || (cpu == 7 && flags > 0)) item->reserved = (flags & 0xff000000) | (size & 0xffffff);
            if (changed) for (auto& matching : table) if (matching.file_id == file->original) {
                uint32_t flags = matching.reserved;
                if (((flags >> 24) & 1) || (cpu == 7 && flags > 0)) matching.reserved = (flags & 0xff000000) | (size & 0xffffff);
            }
            overlayIds.insert(file->id);
        }
        if (changed) for (auto& item : table) {
            require(item.file_id < oldIds.size(), "Overlay file ID outside FAT");
            item.file_id = oldIds[item.file_id];
        }
        if (cpu == 9) { h.arm9_overlay_offset = output->size; h.arm9_overlay_size = table.size() * sizeof(OverlayEntry); }
        else { h.arm7_overlay_offset = output->size; h.arm7_overlay_size = table.size() * sizeof(OverlayEntry); }
        output->literal(table.data(), table.size() * sizeof(OverlayEntry));
        pad();
        for (const auto* file : payloads) write(file->data, file->id, true);
    };
    SourcePtr arm9 = system("arm9.bin");
    if (package.has("arm9.bin") || package.has("xdelta/arm9.bin")) {
        marker = 0;
        if (arm9->size >= 12) arm9->read(arm9->size - 12, &marker, sizeof(marker));
        nitrocode = marker == 0xdec00621;
    }
    h.arm9_rom_offset = output->size;
    h.arm9_size = arm9->size - (nitrocode ? 12 : 0);
    write(arm9, 0xffff, true);
    h.arm9_overlay_offset = 0;
    overlays(9);
    h.arm7_rom_offset = output->size;
    h.arm7_size = system("arm7.bin")->size;
    write(system("arm7.bin"), 0xffff, true);
    h.arm7_overlay_offset = 0;
    overlays(7);
    h.fnt_offset = output->size;
    h.fnt_size = fnt->size;
    write(fnt, 0xffff, true);
    h.fat_offset = output->size;
    h.fat_size = placements.size() * sizeof(Allocation);
    output->append(Kind::Fill, 0, h.fat_size);
    pad();
    h.banner_offset = output->size;
    SourcePtr banner = system("banner.bin");
    auto encoded = bannerBytes(banner, package.has("banner.bin") || package.has("xdelta/banner.bin") ? banner->size : h.unitcode & 2 ? uint32_t(h.banner_size) : 0);
    if (h.unitcode & 2) h.banner_size = encoded.size();
    output->literal(encoded.data(), encoded.size());
    pad();
    for (uint32_t index = 0; index < sorted.size(); index++) {
        uint32_t id = sorted[index];
        if (overlayIds.count(id)) continue;
        auto named = byId[id];
        if (named) write(named->data, id, index + 1 < sorted.size());
        else if (allocations[id].end > allocations[id].start) {
            write(std::make_shared<Slice>(input, allocations[id].start, uint32_t(allocations[id].end) - uint32_t(allocations[id].start)), id, index + 1 < sorted.size());
        }
    }
    output->finish();
    output = overwrite(work, output, h.fat_offset, placements.data(), placements.size() * sizeof(Allocation));
    secureMarker = 0;
    if (output->size >= 0x4008) output->read(0x4000, &secureMarker, sizeof(secureMarker));
    decrypted = secureMarker == UINT64_C(0xe7ffdeffe7ffdeff);
    if ((h.unitcode & 2) && (h.dsi_flags & 1) && h.tid_high && h.tid_high != 0xffffffff) output = writeTwl(work, output, input, h, decrypted);
    h.application_end_offset = output->size;
    uint32_t capacity = 0x20000;
    while (capacity < output->size + signature.size()) capacity *= 2;
    h.devicecap = 0;
    for (uint32_t value = capacity; value > 0x20000; value /= 2) h.devicecap++;
    auto secure = readBytes(output, 0x4000, std::min<uint32_t>(0x4000, output->size - 0x4000));
    secureArea(secure, h, decrypted);
    h.secure_area_crc = CalcCrc16(secure.data(), secure.size());
    std::memcpy(headerData.data(), &h, std::min<uint32_t>(headerSize, sizeof(h)));
    auto finalHeader = reinterpret_cast<Header*>(headerData.data());
    finalHeader->logo_crc = CalcCrc16(headerData.data() + 0xc0, 156);
    finalHeader->header_crc = CalcCrc16(headerData.data(), 0x15e);
    output = overwrite(work, output, 0, headerData.data(), headerData.size());
    output->literal(signature.data(), signature.size());
    output->append(Kind::Fill, 255, capacity - output->size);
    output->finish();
    return output;
}

}
