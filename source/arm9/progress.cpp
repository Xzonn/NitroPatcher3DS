#include "progress.h"
#include "ui.h"
#include "../patch/core.h"
#include "../patch/preparation_progress.h"
#include "resources.h"
#include <cstring>

namespace {
std::string lastStage, lastDetail;
uint32_t lastCompleted, lastTotal;
uint64_t started, rendered;
std::string fileDetail;
ui::PreparationProgress preparation;
np3d::PreparationPlan plan;

void showProgress(const char* stage, uint32_t completed, uint32_t total, bool bytes, const char* detail) {
    uint64_t now = np3d::preparationMilliseconds();
    bool files = std::strcmp(stage,"Preparing game files")==0;
    bool nested = std::strcmp(stage,"Extracting patch file")==0 || std::strcmp(stage,"Applying patch section")==0
        || std::strcmp(stage,"Applying replacement")==0;
    if (files) {
        bool reset = !preparation.fileCount || total!=preparation.fileCount || completed==0;
        if (reset || fileDetail!=detail) preparation.fileNumber=std::min(completed+1,total);
        preparation.fileCount=total;fileDetail=detail;
    } else {
        if (!nested) {preparation.fileNumber=preparation.fileCount=0;fileDetail.clear();}
    }
    plan.update(stage,completed,total,detail);
    bool changed = lastStage != stage || lastDetail != detail || lastTotal != total || completed < lastCompleted;
    if (!changed && now-rendered<100 && (completed!=total || completed==lastCompleted)) return;
    auto work=plan.work();
    preparation.completed=work.completed;preparation.total=work.size;
    preparation.remaining=work.remaining(now-started);
    preparation.bytes=bytes;
    lastStage=stage;lastDetail=detail;lastCompleted=completed;lastTotal=total;rendered=now;
    ui::progress(stage,completed,total,detail,now-started,preparation);
}
}
void beginPatchProgress() {
    lastStage.clear();lastDetail.clear();lastCompleted=lastTotal=0;
    fileDetail.clear();preparation={};plan={};
    started=rendered=np3d::preparationMilliseconds();
    ui::preparing();np3d::setProgressHandler(showProgress);
}
void planPatchProgress(np3d::SourcePtr card,const std::vector<std::string>& paths) {
    plan.initialize(card,paths,np3d::resourceSize("bootstrap.nds"));
}
void finishPatchProgress(bool reused) {
    np3d::progress(reused?"Patch cache reused":"Patch cache prepared",1,1);
    np3d::setProgressHandler(nullptr);
}
