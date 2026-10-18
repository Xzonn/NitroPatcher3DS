#include "resources.h"
#include "../patch/core.h"
#include "miniz.h"
#if !NP3D_SLOT2
#include "np3d_resources_twl_bin.h"
#include "np3d_base_font_bin.h"
#endif
#include <cstring>
#include <functional>
#include <unistd.h>

namespace np3d {
namespace {
void openArchive(mz_zip_archive* archive, const char* name) {
#if NP3D_SLOT2
    (void)name;
    require(mz_zip_reader_init_file(archive, "boot:/resources.zip", 0), "Cannot open embedded resources");
#else
    bool baseFont=std::strcmp(name,"fonts/song12.nftr")==0;
    require(mz_zip_reader_init_mem(archive, baseFont?np3d_base_font_bin:np3d_resources_twl_bin,
        baseFont?np3d_base_font_bin_size:np3d_resources_twl_bin_size, 0), "Cannot open embedded resources");
#endif
}
void extract(const char* name, const std::function<void(const uint8_t*, size_t)>& consume) {
    auto archive = std::make_unique<mz_zip_archive>();
    *archive = {};
    openArchive(archive.get(),name);
    auto iterator = mz_zip_reader_extract_file_iter_new(archive.get(), name, 0);
    require(iterator, std::string("Cannot open embedded resource: ") + name);
    std::vector<uint8_t> buffer(BufferSize);
    while (true) {
        size_t size = mz_zip_reader_extract_iter_read(iterator, buffer.data(), buffer.size());
        require(iterator->status >= TINFL_STATUS_DONE, std::string("Cannot decode embedded resource: ") + name);
        if (!size) break;
        consume(buffer.data(), size);
    }
    require(mz_zip_reader_extract_iter_free(iterator), std::string("Cannot verify embedded resource: ") + name);
    require(mz_zip_reader_end(archive.get()), "Cannot close embedded resources");
}
}

std::vector<uint8_t> readResource(const char* name) {
    std::vector<uint8_t> result;
    extract(name, [&](const uint8_t* data, size_t size) { result.insert(result.end(), data, data + size); });
    return result;
}

void writeResource(const char* name, const std::string& path) {
    auto archive=std::make_unique<mz_zip_archive>();*archive={};
    openArchive(archive.get(),name);
    int index=mz_zip_reader_locate_file(archive.get(),name,nullptr,0);
    mz_zip_archive_file_stat info;
    require(index>=0 && mz_zip_reader_file_stat(archive.get(),index,&info),"Cannot read embedded resource version");
    require(mz_zip_reader_end(archive.get()),"Cannot close embedded resources");
    std::string version=std::to_string(info.m_crc32)+":"+std::to_string(info.m_uncomp_size);
    std::string marker=path+".version";
    if (exists(path) && exists(marker) && Disk(path).size==info.m_uncomp_size) {
        auto stored=std::make_shared<Disk>(marker);
        if (stored->size==version.size()) {
            auto bytes=readBytes(stored,0,stored->size);
            if (std::string(bytes.begin(),bytes.end())==version) {
                progress("Preparing launch files",0,0,true);
                return;
            }
        }
    }
    uint32_t offset = 0;
    uint32_t total=info.m_uncomp_size;
    progress("Preparing launch files",0,total,true);
    {
        Disk output(path+".new", "wb");
        extract(name, [&](const uint8_t* data, size_t size) {
            output.write(offset, data, size); offset += size;
            progress("Preparing launch files",offset,total,true);
        });
        output.sync();
    }
    if (exists(path)) require(unlink(path.c_str())==0,"Cannot replace launch resource");
    require(std::rename((path+".new").c_str(),path.c_str())==0,"Cannot publish launch resource");
    { Disk output(marker+".new","wb");output.write(0,version.data(),version.size());output.sync(); }
    if (exists(marker)) require(unlink(marker.c_str())==0,"Cannot replace launch resource version");
    require(std::rename((marker+".new").c_str(),marker.c_str())==0,"Cannot publish launch resource version");
}

uint32_t resourceSize(const char* name) {
    auto archive=std::make_unique<mz_zip_archive>();*archive={};
    openArchive(archive.get(),name);
    int index=mz_zip_reader_locate_file(archive.get(),name,nullptr,0);
    mz_zip_archive_file_stat info;
    require(index>=0 && mz_zip_reader_file_stat(archive.get(),index,&info),"Cannot read embedded resource size");
    require(mz_zip_reader_end(archive.get()),"Cannot close embedded resources");
    return info.m_uncomp_size;
}
}
