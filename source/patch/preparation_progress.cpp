#include "preparation_progress.h"
#include "nds_adapter.h"
#include "miniz.h"
#include <cstring>

namespace np3d {
void PreparationWork::update(uint32_t current,uint32_t total) {
    if (total) completed=size*std::min(current,total)/total;
}
int64_t PreparationWork::remaining(uint64_t elapsed) const {
    if (size && completed==size) return 0;
    if (!completed || elapsed<1000) return -1;
    return std::max<uint64_t>(1,(size-completed)*elapsed/completed);
}

void PreparationPlan::initialize(SourcePtr source,const std::vector<std::string>& paths,uint32_t launchSize) {
    *this={};
    auto header=std::make_unique<Header>();
    source->read(0,header.get(),std::min<uint32_t>(sizeof(Header),source->size));
    progress("Reading game directory",0,0);
    auto named=enumerateFiles(source,*header);
    std::map<std::string,uint32_t> sizes;
    for (const auto& file : named) sizes[normalize(file.name)]=file.length;
    sizes["header.bin"]=header->rom_header_size;sizes["banner.bin"]=0x840;
    sizes["arm9.bin"]=uint32_t(header->arm9_size)+12;sizes["arm7.bin"]=header->arm7_size;
    sizes["fnt.bin"]=header->fnt_size;sizes["fat.bin"]=header->fat_size;
    sizes["overarm9.bin"]=header->arm9_overlay_size;sizes["overarm7.bin"]=header->arm7_overlay_size;
    card.size=source->size;launch.size=launchSize;finish.size=1;
    uint64_t fileCount=named.size()+8+uint32_t(header->arm9_overlay_size)/sizeof(OverlayEntry)
        +uint32_t(header->arm7_overlay_size)/sizeof(OverlayEntry);
    check.size=fileCount*sizeof(Extent);
    for (const auto& path : paths) {
        progress("Reading patch directory",patches.size()+1,paths.size(),false,path.c_str());
        patches.emplace_back();auto& item=patches.back();
        item.hash.size=Disk(path).size;item.files.size=item.layout.size=fileCount;
        if (patches.size()>1) item.verify.size=source->size;
        if ((header->unitcode&2) && (header->dsi_flags&1)) item.dsi.size=source->size;
        auto archive=std::make_unique<mz_zip_archive>();*archive={};
        require(mz_zip_reader_init_file(archive.get(),path.c_str(),0),"Cannot open XZP archive");
        for (unsigned int index=0;index<mz_zip_reader_get_num_files(archive.get());index++) {
            mz_zip_archive_file_stat info;
            require(mz_zip_reader_file_stat(archive.get(),index,&info),"Cannot read XZP directory");
            if (info.m_is_directory) continue;
            std::string name(info.m_full_filename,info.m_full_filename_size);
            if (std::any_of(name.begin(),name.end(),[](unsigned char byte){return byte>=128;}))
                name=convert(name,info.m_filename_utf8?"UTF-8":"CP437");
            name=normalize(name);
            if (name=="readme.md" || name=="readme.txt") continue;
            item.extraction[name].size=info.m_uncomp_size;
            if (name.compare(0,7,"xdelta/")==0) {
                auto file=name.substr(7);auto found=sizes.find(file);
                item.application[file].size=found==sizes.end()?info.m_uncomp_size:found->second;
                cache.size+=item.application[file].size;
            } else if (name.compare(0,14,"preprocessing/")==0) item.application["game preprocessing"].size=source->size;
            else if (name!="md5.txt" && name!="metadata.json") {
                item.application[name].size=info.m_uncomp_size;cache.size+=info.m_uncomp_size;
            }
        }
        require(mz_zip_reader_end(archive.get()),"Cannot close XZP archive");
    }
    ready=true;
}

void PreparationPlan::finishPatch() {
    auto& item=patches[patch];
    for (auto* tasks : {&item.extraction,&item.application})
        for (auto& task : *tasks) task.second.size=task.second.completed;
    for (auto* task : {&item.files,&item.layout,&item.verify,&item.dsi}) task->size=task->completed;
}

void PreparationPlan::update(const char* stage,uint32_t completed,uint32_t total,const char* detail) {
    if (patches.empty()) return;
    std::string name(stage);
    if (name=="Reading Game Card") {cardRead=true;card.update(completed,total);}
    else if (name=="Reading patch") {
        if (!cardRead) card.size=0;
        if (!completed) hash=nextHash++;
        patches.at(hash).hash.update(completed,total);
    } else if (name=="Patch sequence") {
        if (completed>1) finishPatch();
        patch=completed-1;dsi=false;windowName.clear();windowBase=0;
    } else if (name=="Using prepared cache") {
        for (patch=0;patch<patches.size();patch++) finishPatch();
        patch=patches.size()-1;cache={};
    } else if (name=="Extracting patch file") patches[patch].extraction[normalize(detail)].update(completed,total);
    else if (name=="Applying replacement") {
        auto& task=patches[patch].application[normalize(detail)];task.size=total;task.update(completed,total);
    } else if (name=="Applying patch section") {
        if (windowName!=detail) {
            windowName=detail;windowBase=0;windowCompleted=windowTotal=0;
            windowWork=&patches[patch].application[normalize(detail)];
        }
        else if (!completed && windowCompleted) windowBase+=windowTotal;
        windowCompleted=completed;windowTotal=total;
        auto& task=*windowWork;
        task.size=std::max(task.size,windowBase+total);task.completed=windowBase+completed;
    } else if (name=="Preparing game files") {
        auto& task=patches[patch].files;task.size=total;task.update(completed,total);
    } else if (name=="Arranging game files") {
        if (total && completed==total) patches[patch].layout.completed=std::min(patches[patch].layout.size,patches[patch].layout.completed+1);
    } else if (name=="Preparing DSi files") dsi=true;
    else if (name=="Checking game data") {
        auto& task=dsi?patches[patch].dsi:patches[patch].verify;task.size=total;task.update(completed,total);
    } else if (name=="Writing cache") {finishPatch();cache.update(completed,total);}
    else if (name=="Checking cache") {check.size=uint64_t(total)*sizeof(Extent);check.update(completed,total);}
    else if (name=="Preparing launch files") launch.update(completed,total);
    else if (name=="Patch cache reused" || name=="Patch cache prepared") {
        finishPatch();cache.size=cache.completed;check.size=check.completed;
        launch.completed=launch.size;finish.completed=finish.size;
    }
}

PreparationWork PreparationPlan::work() const {
    PreparationWork result;
    if (!ready) return result;
    auto add=[&](const PreparationWork& item){result.completed+=item.completed;result.size+=item.size;};
    for (const auto* task : {&card,&cache,&check,&launch,&finish}) add(*task);
    for (const auto& item : patches) {
        for (const auto* task : {&item.hash,&item.files,&item.layout,&item.verify,&item.dsi}) add(*task);
        for (const auto* tasks : {&item.extraction,&item.application}) for (const auto& task : *tasks) add(task.second);
    }
    return result;
}
}
