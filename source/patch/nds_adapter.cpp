#include "nds_adapter.h"
#include <cstring>
#include <set>

Header header;
FILE* fNDS;
int verbose = 0;
char* filerootdir = nullptr;
char* filemasks[MAX_FILEMASKS] = {};
int filemask_num = 0;
void (*ndsFileVisitor)(const char*, const char*, unsigned int, unsigned int, unsigned int);
extern void ExtractDirectory(const char*, unsigned int);
static std::set<unsigned int> directories;

void ndsRequire(bool condition, const char* message) { np3d::require(condition, message); }

void ndsDirectoryCheck(const char* prefix, unsigned int id) {
    np3d::logPreparation("NDS directory id=" + std::to_string(id) + " path=" + prefix);
    np3d::require(id >= 0xf000 && id <= 0xffff && directories.insert(id).second,
        "Invalid NDS directory tree or cycle");
    np3d::require(std::count(prefix, prefix + std::strlen(prefix), '/') <= 257, "NDS directory nesting exceeds limit");
}

namespace np3d {

struct Cookie {
    SourcePtr source;
    uint32_t position = 0;
};

static std::vector<NamedFile>* files;

static void visit(const char* prefix, const char* name, unsigned int id, unsigned int offset, unsigned int length) {
    require(id < uint32_t(header.fat_size) / 8, "NDS file ID outside FAT");
    require(std::none_of(files->begin(), files->end(), [&](const NamedFile& file) { return file.id == id; }), "Duplicate NDS file ID");
    std::string path = std::string(prefix) + name;
    if (std::any_of(path.begin(), path.end(), [](unsigned char byte) { return byte >= 128; })) path = convert(path, "CP932");
    files->push_back({"data" + path, id, offset, length});
}

#ifdef ARM9
static int readCookie(void* opaque, char* output, int length) {
#else
static ssize_t readCookie(void* opaque, char* output, size_t length) {
#endif
    auto& cookie = *static_cast<Cookie*>(opaque);
    uint32_t take = std::min<uint64_t>(length, cookie.source->size - cookie.position);
    cookie.source->read(cookie.position, output, take);
    cookie.position += take;
    return take;
}

#ifdef ARM9
static fpos_t seekCookie(void* opaque, fpos_t offset, int origin) {
    auto& cookie = *static_cast<Cookie*>(opaque);
    int64_t position = (origin == SEEK_CUR ? cookie.position : origin == SEEK_END ? cookie.source->size : 0) + int64_t(offset);
    require(position >= 0 && uint64_t(position) <= cookie.source->size, "NDS directory seek outside ROM");
    cookie.position = position;
    return position;
}
#else
static int seekCookie(void* opaque, off64_t* offset, int origin) {
    auto& cookie = *static_cast<Cookie*>(opaque);
    int64_t position = (origin == SEEK_CUR ? cookie.position : origin == SEEK_END ? cookie.source->size : 0) + *offset;
    require(position >= 0 && uint64_t(position) <= cookie.source->size, "NDS directory seek outside ROM");
    cookie.position = position;
    *offset = position;
    return 0;
}
#endif

std::vector<NamedFile> enumerateFiles(SourcePtr source, const Header& value) {
    Cookie cookie{source};
#ifdef ARM9
    fNDS = funopen(&cookie, readCookie, nullptr, seekCookie, nullptr);
#else
    cookie_io_functions_t functions = {readCookie, nullptr, seekCookie, nullptr};
    fNDS = fopencookie(&cookie, "rb", functions);
#endif
    require(fNDS, "Cannot open NDS directory reader");
    header = value;
    directories.clear();
    std::vector<NamedFile> result;
    files = &result;
    ndsFileVisitor = visit;
    ExtractDirectory("/", 0xf000);
    require(!std::ferror(fNDS), "Cannot read NDS directory");
    require(std::fclose(fNDS) == 0, "Cannot close NDS directory reader");
    ndsFileVisitor = nullptr;
    for (const auto& file : result) require(file.offset <= source->size && file.length <= source->size - file.offset,
        "NDS file outside ROM: " + file.name);
    return result;
}

}
