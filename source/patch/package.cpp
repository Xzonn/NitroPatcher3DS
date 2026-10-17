#include "package.h"
#include <cerrno>

namespace np3d {

static size_t readArchive(void* opaque, mz_uint64 offset, void* output, size_t length) {
    auto& package = *static_cast<Package*>(opaque);
    logPreparation("ZIP read begin offset=" + std::to_string(offset) + " requested=" + std::to_string(length));
    size_t result = package.reader(package.readOpaque, offset, output, length);
    int error = errno;
    logPreparation("ZIP read done offset=" + std::to_string(offset) + " returned=" + std::to_string(result)
        + " errno=" + std::to_string(result == length ? 0 : error));
    errno = error;
    return result;
}

Package::Package(const std::string& path, Workspace& directory) : work(directory) {
    logPreparation("ZIP open begin " + path);
    require(mz_zip_reader_init_file(&archive, path.c_str(), 0), "Cannot open XZP archive");
    reader = archive.m_pRead;
    readOpaque = archive.m_pIO_opaque;
    archive.m_pRead = readArchive;
    archive.m_pIO_opaque = this;
    uint64_t total = 0;
    unsigned int count = mz_zip_reader_get_num_files(&archive);
    logPreparation("ZIP open done entries=" + std::to_string(count));
    require(count <= 100000, "XZP contains too many entries");
    progress("Reading patch directory", 0, count);
    for (unsigned int index = 0; index < count; index++) {
        mz_zip_archive_file_stat info;
        require(mz_zip_reader_file_stat(&archive, index, &info), "Cannot read XZP directory");
        require(info.m_is_supported && !info.m_is_encrypted, "Unsupported XZP compression or encryption");
        total += info.m_uncomp_size;
        require(total <= 0x40000000, "XZP uncompressed size exceeds limit");
        std::string name(info.m_full_filename, info.m_full_filename_size);
        bool ascii = std::all_of(name.begin(), name.end(), [](unsigned char byte) { return byte < 128; });
        if (!ascii) name = convert(name, info.m_filename_utf8 ? "UTF-8" : "CP437");
        name = normalize(name);
        logPreparation("ZIP entry=" + std::to_string(index) + " name=" + name + " method=" + std::to_string(info.m_method)
            + " compressed=" + std::to_string(info.m_comp_size) + " size=" + std::to_string(info.m_uncomp_size)
            + " crc32=" + std::to_string(info.m_crc32) + " header=" + std::to_string(info.m_local_header_ofs));
        if (!name.empty() && name.back() != '/') entries[name] = index;
        progress("Reading patch directory", index + 1, count);
    }
}

Package::~Package() {
    logPreparation("ZIP close begin");
    require(mz_zip_reader_end(&archive), "Cannot close XZP archive");
    logPreparation("ZIP close done");
}

bool Package::has(const std::string& name) const { return entries.find(normalize(name)) != entries.end(); }

std::string Package::extract(const std::string& name) {
    auto found = entries.find(normalize(name));
    require(found != entries.end(), "Missing XZP entry: " + name);
    std::string path = work.directory + "/entry-" + std::to_string(next++);
    logPreparation("EXTRACT begin name=" + name + " output=" + path);
    Disk output(path, "w+b");
    mz_zip_archive_file_stat info;
    require(mz_zip_reader_file_stat(&archive, found->second, &info), "Cannot read XZP directory");
    progress("Extracting patch file", 0, info.m_uncomp_size, true, name.c_str());
    logPreparation("EXTRACT iterator begin name=" + name + " method=" + std::to_string(info.m_method)
        + " compressed=" + std::to_string(info.m_comp_size) + " size=" + std::to_string(info.m_uncomp_size));
    auto iterator = mz_zip_reader_extract_iter_new(&archive, found->second, 0);
    require(iterator, "Cannot initialize XZP extraction: " + name + ": " + mz_zip_get_error_string(mz_zip_peek_last_error(&archive)));
    logPreparation("EXTRACT iterator ready name=" + name);
    std::vector<uint8_t> buffer(BufferSize);
    uint32_t offset = 0;
    while (true) {
        logPreparation("INFLATE begin name=" + name + " output=" + std::to_string(offset));
        size_t length = mz_zip_reader_extract_iter_read(iterator, buffer.data(), buffer.size());
        logPreparation("INFLATE done name=" + name + " bytes=" + std::to_string(length) + " status=" + std::to_string(iterator->status)
            + " inputRemaining=" + std::to_string(iterator->comp_remaining) + " inputBuffered=" + std::to_string(iterator->read_buf_avail));
        require(iterator->status >= TINFL_STATUS_DONE, "Cannot decode XZP entry: " + name + ": " + mz_zip_get_error_string(mz_zip_peek_last_error(&archive)));
        if (!length) break;
        logPreparation("EXTRACT write begin name=" + name + " offset=" + std::to_string(offset) + " bytes=" + std::to_string(length));
        output.write(offset, buffer.data(), length);
        logPreparation("EXTRACT write done name=" + name);
        offset += length;
        progress("Extracting patch file", offset, info.m_uncomp_size, true, name.c_str());
    }
    logPreparation("EXTRACT verify begin name=" + name);
    mz_bool verified = mz_zip_reader_extract_iter_free(iterator);
    require(verified, "Cannot verify XZP entry: " + name + ": " + mz_zip_get_error_string(mz_zip_peek_last_error(&archive)));
    logPreparation("EXTRACT verify done name=" + name);
    logPreparation("EXTRACT flush begin " + path);
    output.sync();
    logPreparation("EXTRACT done name=" + name + " bytes=" + std::to_string(offset));
    return path;
}

std::vector<uint8_t> Package::bytes(const std::string& name) {
    logPreparation("ENTRY bytes begin " + name);
    auto input = std::make_shared<Disk>(extract(name));
    auto result = readBytes(input, 0, input->size);
    logPreparation("ENTRY bytes done name=" + name + " size=" + std::to_string(result.size()));
    return result;
}

SourcePtr Package::replace(const std::string& name, SourcePtr source) {
    std::string delta = "xdelta/" + normalize(name);
    if (has(delta)) return applyDelta(work, source, extract(delta), name);
    if (!has(name)) return source;
    auto file = std::make_shared<Disk>(extract(name));
    auto result = work.map();
    std::vector<uint8_t> buffer(BufferSize);
    progress("Applying replacement", 0, file->size, true, name.c_str());
    for (uint32_t offset = 0; offset < file->size;) {
        uint32_t take = std::min(BufferSize, file->size - offset);
        file->read(offset, buffer.data(), take);
        result->literal(buffer.data(), take);
        offset += take;
        progress("Applying replacement", offset, file->size, true, name.c_str());
    }
    result->finish();
    return result;
}

}
