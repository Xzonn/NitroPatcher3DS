#include "core.h"

#include <cerrno>
#include <cstring>
#ifdef ARM9
#include <nds/timers.h>
#else
#include <chrono>
#endif
#include <dirent.h>
#include <iconv.h>
#include "uni_algo/case.h"
#include <sys/stat.h>
#include <unistd.h>

#include "mbedtls/md5.h"
#include "mbedtls/sha256.h"

namespace np3d {

static FatalHandler fatalHandler;
static ProgressHandler progressHandler;
static FILE* preparationLog;
static uint64_t logStarted;
static uint64_t logUpdated;
static std::string logStage;

#ifdef ARM9
static uint32_t clockTicks;
static uint64_t clockTotal;
#endif

uint64_t preparationMilliseconds() {
#ifdef ARM9
    uint16_t high, low;
    do {
        high = TIMER3_DATA;
        low = TIMER2_DATA;
    } while (high != TIMER3_DATA);
    uint32_t ticks = uint32_t(high) << 16 | low;
    clockTotal += uint32_t(ticks - clockTicks);
    clockTicks = ticks;
    return clockTotal * 1024000 / BUS_CLOCK;
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

void logPreparation(const std::string& message) {
    if (!preparationLog) return;
    int savedError = errno;
    FILE* output = preparationLog;
    preparationLog = nullptr;
#ifdef ARM9
    uintptr_t position;
    asm volatile("mov %0, sp" : "=r"(position));
    require(std::fprintf(output, "[%lu s sp=%08lx] %s\n", static_cast<unsigned long>((preparationMilliseconds() - logStarted) / 1000),
        static_cast<unsigned long>(position), message.c_str()) >= 0,
#else
    require(std::fprintf(output, "[%lu s] %s\n", static_cast<unsigned long>((preparationMilliseconds() - logStarted) / 1000), message.c_str()) >= 0,
#endif
        "Cannot write preparation log");
    require(std::fflush(output) == 0, "Cannot flush preparation log");
    require(fsync(fileno(output)) == 0, "Cannot sync preparation log");
    preparationLog = output;
    errno = savedError;
}

void beginPreparationLog(const std::string& root, const std::string& patch) {
    makeDirectory(root);
    makeDirectory(root + "/logs");
    preparationLog = std::fopen((root + "/logs/prepare.log").c_str(), "w");
    require(preparationLog, "Cannot open preparation log");
#ifdef ARM9
    TIMER2_CR = 0;
    TIMER3_CR = 0;
    TIMER2_DATA = 0;
    TIMER3_DATA = 0;
    clockTicks = 0;
    clockTotal = 0;
    TIMER3_CR = TIMER_ENABLE | TIMER_CASCADE;
    TIMER2_CR = TIMER_ENABLE | TIMER_DIV_1024;
#endif
    logStarted = logUpdated = preparationMilliseconds();
    logStage.clear();
    logPreparation("BEGIN patch=" + patch);
}

void finishPreparationLog(bool reused, const std::string& error) {
    logPreparation(error.empty() ? (reused ? "SUCCESS cache reused" : "SUCCESS cache prepared") : "ERROR " + error);
    FILE* output = preparationLog;
    preparationLog = nullptr;
    if (output) require(std::fclose(output) == 0, "Cannot close preparation log");
#ifdef ARM9
    TIMER2_CR = 0;
    TIMER3_CR = 0;
#endif
}

void setFatalHandler(FatalHandler handler) { fatalHandler = handler; }

void setProgressHandler(ProgressHandler handler) { progressHandler = handler; }

void progress(const char* stage, uint32_t completed, uint32_t total, bool bytes, const char* detail) {
    if (preparationLog) {
        uint64_t now = preparationMilliseconds();
        if (logStage != stage || now - logUpdated >= 5000) {
            logPreparation(std::string(stage) + " | " + std::to_string(completed) + "/" + std::to_string(total)
                + (bytes ? " bytes | " : " items | ") + detail);
            logStage = stage;
            logUpdated = now;
        }
    }
    if (progressHandler) progressHandler(stage, completed, total, bytes, detail);
}

[[noreturn]] void fail(const std::string& message) {
    int error = errno;
    logPreparation("ERROR " + message + " | errno snapshot=" + std::to_string(error) + " (" + std::strerror(error) + ")");
    if (fatalHandler) fatalHandler(message.c_str());
    std::fprintf(stderr, "%s\n", message.c_str());
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

Disk::Disk(const std::string& name, const char* mode) : path(name) {
    logPreparation("FILE open begin mode=" + std::string(mode) + " path=" + path);
    handle = std::fopen(path.c_str(), mode);
    require(handle, "Cannot open " + path);
    require(std::fseek(handle, 0, SEEK_END) == 0, "Cannot seek " + path);
    long length = std::ftell(handle);
    require(length >= 0 && uint64_t(length) <= 0x40000000, "Invalid file size: " + path);
    size = length;
    logPreparation("FILE open done size=" + std::to_string(size) + " path=" + path);
}

Disk::~Disk() {
    logPreparation("FILE close begin " + path);
    require(std::fclose(handle) == 0, "Cannot close " + path);
    logPreparation("FILE close done " + path);
}

void Disk::read(uint32_t offset, void* output, uint32_t length) {
    require(offset <= size && length <= size - offset, "Read outside " + path);
    require(std::fseek(handle, offset, SEEK_SET) == 0, "Cannot seek " + path);
    require(std::fread(output, 1, length, handle) == length, "Cannot read " + path);
}

void Disk::write(uint32_t offset, const void* input, uint32_t length) {
    require(offset <= 0x40000000 && length <= 0x40000000 - offset, "Output exceeds ROM size limit");
    if (std::ftell(handle) != long(offset)) require(std::fseek(handle, offset, SEEK_SET) == 0, "Cannot seek " + path);
    require(std::fwrite(input, 1, length, handle) == length, "Cannot write " + path);
    size = std::max(size, offset + length);
}

void Disk::sync() {
    logPreparation("FILE sync begin " + path);
    require(std::fflush(handle) == 0, "Cannot flush " + path);
    require(fsync(fileno(handle)) == 0, "Cannot sync " + path);
    logPreparation("FILE sync done " + path);
}

Slice::Slice(SourcePtr source, uint32_t offset, uint32_t length) : input(source), base(offset) {
    require(offset <= input->size && length <= input->size - offset, "Slice outside source");
    size = length;
}

void Slice::read(uint32_t offset, void* output, uint32_t length) {
    require(offset <= size && length <= size - offset, "Read outside source slice");
    input->read(base + offset, output, length);
}

Map::Map(const std::string& path, std::shared_ptr<Disk> data, SourcePtr original, bool create)
    : table(std::make_shared<Disk>(path, create ? "w+b" : "rb")), payload(data), card(original) {
    uint32_t header[8] = {0x4433504e, 0x0050414d, FormatVersion, RulesVersion, 0, 0, 16, 0};
    if (create) {
        table->write(0, header, sizeof(header));
    } else {
        table->read(0, header, sizeof(header));
        require(header[0] == 0x4433504e && header[1] == 0x0050414d && header[2] == FormatVersion
            && header[3] == RulesVersion && header[6] == sizeof(Extent), "Unsupported cache mapping format");
        count = header[4];
        size = header[5];
        require(uint64_t(count) * sizeof(Extent) + 32 == table->size && count > 0, "Incomplete cache mapping");
        uint32_t end = 0;
        progress("Checking cache", 0, count);
        for (uint32_t number = 0; number < count; number++) {
            Extent item = at(number);
            require(item.start == end && item.length && item.length <= size - end, "Invalid cache mapping range");
            uint32_t available = item.kind == Kind::Card ? card->size : payload->size;
            require(item.kind == Kind::Card || item.kind == Kind::Patch || item.kind == Kind::Fill, "Invalid cache data source");
            if (item.kind == Kind::Fill) require(item.source <= 255, "Invalid cache fill byte");
            else require(item.source <= available && item.length <= available - item.source, "Cache source outside file");
            end += item.length;
            if (number % 256 == 0) index.push_back(item);
            last = item;
            if ((number + 1) % 256 == 0 || number + 1 == count) progress("Checking cache", number + 1, count);
        }
        require(end == size, "Incomplete virtual ROM");
    }
}

static Map* cachedMap;
static uint32_t cachedFirst = UINT32_MAX;
static std::array<Extent, 256> cached;

Map::~Map() { if (cachedMap == this) cachedMap = nullptr; }

Extent Map::at(uint32_t number) {
    require(number < count, "Mapping entry outside table");
    if (pending && number >= pendingFirst) return (*pending)[number - pendingFirst];
    uint32_t first = number & ~255u;
    if (cachedMap != this || cachedFirst != first) {
        table->read(32 + first * sizeof(Extent), cached.data(), std::min<uint32_t>(256, count - first) * sizeof(Extent));
        cachedFirst = first;
        cachedMap = this;
    }
    return cached[number - first];
}

void Map::flush() {
    if (pending) {
        table->write(32 + pendingFirst * sizeof(Extent), pending->data(),
            std::min<uint32_t>(256, count - pendingFirst) * sizeof(Extent));
        if (cachedMap == this) cachedMap = nullptr;
    }
}

Extent Map::find(uint32_t offset) {
    require(offset < size, "Address outside virtual ROM");
    if (offset >= last.start) return last;
    uint32_t first = 0, end = count;
    if (!index.empty()) {
        auto found = std::upper_bound(index.begin(), index.end(), offset,
            [](uint32_t address, const Extent& item) { return address < item.start; });
        if (found != index.begin()) first = uint32_t(found - index.begin() - 1) * 256;
        end = std::min(count, first + 256);
    }
    while (first < end) {
        uint32_t middle = first + (end - first) / 2;
        Extent item = at(middle);
        if (offset < item.start) end = middle;
        else if (offset >= item.start + item.length) first = middle + 1;
        else return item;
    }
    fail("Virtual ROM mapping has a gap");
}

void Map::append(Kind kind, uint32_t offset, uint32_t length) {
    if (!length) return;
    require(size <= 0x40000000 && length <= 0x40000000 - size, "Virtual ROM exceeds size limit");
    bool merge = count && last.kind == kind
        && (kind == Kind::Fill ? last.source == offset : last.source + last.length == offset);
    uint32_t slot = merge ? count - 1 : count;
    uint32_t first = slot & ~255u;
    if (!pending || pendingFirst != first) {
        flush();
        pending = std::make_unique<std::array<Extent, 256>>();
        pendingFirst = first;
        if (first < count) table->read(32 + first * sizeof(Extent), pending->data(), (count - first) * sizeof(Extent));
    }
    if (merge) last.length += length;
    else {
        last = {size, length, kind, offset};
        if (count % 256 == 0) {
            index.push_back(last);
        }
        count++;
    }
    (*pending)[slot - pendingFirst] = last;
    size += length;
}

void Map::literal(const void* input, uint32_t length) {
    if (!length) return;
    uint32_t offset = payload->size;
    payload->write(offset, input, length);
    append(Kind::Patch, offset, length);
}

void Map::copy(SourcePtr input, uint32_t offset, uint32_t length) {
    require(offset <= input->size && length <= input->size - offset, "Copy outside source");
    if (auto slice = std::dynamic_pointer_cast<Slice>(input)) {
        copy(slice->input, slice->base + offset, length);
    } else if (auto map = std::dynamic_pointer_cast<Map>(input)) {
        while (length) {
            Extent item = map->find(offset);
            uint32_t take = std::min(length, item.length - (offset - item.start));
            append(item.kind, item.source + (item.kind == Kind::Fill ? 0 : offset - item.start), take);
            offset += take;
            length -= take;
        }
    } else {
        require(input == card, "Unregistered mapping source");
        append(Kind::Card, offset, length);
    }
}

void Map::read(uint32_t offset, void* output, uint32_t length) {
    require(offset <= size && length <= size - offset, "Read outside virtual ROM");
    auto bytes = static_cast<uint8_t*>(output);
    while (length) {
        Extent item = find(offset);
        uint32_t take = std::min(length, item.length - (offset - item.start));
        if (item.kind == Kind::Fill) std::memset(bytes, item.source, take);
        else {
            SourcePtr input = item.kind == Kind::Card ? card : payload;
            input->read(item.source + offset - item.start, bytes, take);
        }
        bytes += take;
        offset += take;
        length -= take;
    }
}

void Map::finish() {
    flush();
    uint32_t values[] = {count, size};
    table->write(16, values, sizeof(values));
    table->sync();
    payload->sync();
    pending.reset();
    pendingFirst = UINT32_MAX;
}

bool exists(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) == 0) return true;
    require(errno == ENOENT, "Cannot inspect " + path);
    return false;
}

void makeDirectory(const std::string& path) {
    if (mkdir(path.c_str(), 0777) == 0) return;
    require(errno == EEXIST, "Cannot create directory " + path);
    struct stat info;
    require(stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode), "Expected directory " + path);
}

void removeTree(const std::string& path) {
    if (!exists(path)) return;
    logPreparation("DIRECTORY open " + path);
    DIR* directory = opendir(path.c_str());
    require(directory, "Cannot open directory " + path);
    while (dirent* entry = readdir(directory)) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string child = path + "/" + name;
        struct stat info;
        require(stat(child.c_str(), &info) == 0, "Cannot inspect " + child);
        if (S_ISDIR(info.st_mode)) removeTree(child);
        else {
            logPreparation("FILE remove begin " + child);
            require(unlink(child.c_str()) == 0, "Cannot remove " + child);
            logPreparation("FILE remove done " + child);
        }
    }
    require(closedir(directory) == 0, "Cannot close directory " + path);
    logPreparation("DIRECTORY remove begin " + path);
#ifdef ARM9
    require(unlink(path.c_str()) == 0, "Cannot remove directory " + path);
#else
    require(rmdir(path.c_str()) == 0, "Cannot remove directory " + path);
#endif
    logPreparation("DIRECTORY remove done " + path);
}

std::vector<uint8_t> readBytes(SourcePtr source, uint32_t offset, uint32_t length) {
    std::vector<uint8_t> data(length);
    source->read(offset, data.data(), length);
    return data;
}

void copyFile(const std::string& source, const std::string& destination) {
    auto input = std::make_shared<Disk>(source);
    Disk output(destination, "wb");
    std::vector<uint8_t> buffer(BufferSize);
    for (uint32_t offset = 0; offset < input->size;) {
        uint32_t length = std::min(BufferSize, input->size - offset);
        input->read(offset, buffer.data(), length);
        output.write(offset, buffer.data(), length);
        offset += length;
    }
    output.sync();
}

static std::string hex(const uint8_t* data, unsigned int length) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned int index = 0; index < length; index++) {
        result += digits[data[index] >> 4];
        result += digits[data[index] & 15];
    }
    return result;
}

std::string digest(SourcePtr input, bool sha256, const char* stage) {
    mbedtls_md5_context md5;
    mbedtls_sha256_context sha;
    mbedtls_md5_init(&md5);
    mbedtls_sha256_init(&sha);
    require((sha256 ? mbedtls_sha256_starts_ret(&sha, 0) : mbedtls_md5_starts_ret(&md5)) == 0, "Cannot initialize digest");
    std::vector<uint8_t> buffer(BufferSize);
    if (stage) progress(stage, 0, input->size, true);
    for (uint32_t offset = 0; offset < input->size;) {
        uint32_t length = std::min(BufferSize, input->size - offset);
        input->read(offset, buffer.data(), length);
        require((sha256 ? mbedtls_sha256_update_ret(&sha, buffer.data(), length)
            : mbedtls_md5_update_ret(&md5, buffer.data(), length)) == 0, "Cannot update digest");
        offset += length;
        if (stage) progress(stage, offset, input->size, true);
    }
    uint8_t bytes[32];
    require((sha256 ? mbedtls_sha256_finish_ret(&sha, bytes) : mbedtls_md5_finish_ret(&md5, bytes)) == 0, "Cannot finish digest");
    mbedtls_md5_free(&md5);
    mbedtls_sha256_free(&sha);
    return hex(bytes, sha256 ? 32 : 16);
}

std::string digest(const void* input, uint32_t length, bool sha256) {
    uint8_t bytes[32];
    require((sha256 ? mbedtls_sha256_ret(static_cast<const uint8_t*>(input), length, bytes, 0)
        : mbedtls_md5_ret(static_cast<const uint8_t*>(input), length, bytes)) == 0, "Cannot calculate digest");
    return hex(bytes, sha256 ? 32 : 16);
}

std::string normalize(std::string value) {
    for (char& byte : value) {
        if (byte == '\\') byte = '/';
        else if (byte >= 'A' && byte <= 'Z') byte += 'a' - 'A';
    }
    if (std::any_of(value.begin(), value.end(), [](unsigned char byte) { return byte >= 128; })) {
        value = una::cases::to_lowercase_utf8(value);
    }
    return value;
}

std::string convert(const std::string& input, const char* encoding) {
    iconv_t converter = iconv_open("UTF-8", encoding);
    require(converter != (iconv_t)-1, "Cannot initialize filename conversion");
    std::string output(input.size() * 4 + 4, '\0');
    char* source = const_cast<char*>(input.data());
    char* target = output.data();
    size_t sourceLength = input.size(), targetLength = output.size();
    require(iconv(converter, &source, &sourceLength, &target, &targetLength) != size_t(-1) && sourceLength == 0,
        "Invalid filename encoding");
    require(iconv_close(converter) == 0, "Cannot close filename converter");
    output.resize(output.size() - targetLength);
    return output;
}

Workspace::Workspace(const std::string& path, SourcePtr input) : directory(path), card(input) {
    makeDirectory(directory);
    payload = std::make_shared<Disk>(directory + "/data.bin", "w+b");
}

std::shared_ptr<Map> Workspace::map() {
    return std::make_shared<Map>(directory + "/" + std::to_string(next++) + ".map", payload, card, true);
}

}
