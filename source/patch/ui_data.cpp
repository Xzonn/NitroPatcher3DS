#include "ui_data.h"
#include "miniz.h"
#include "cJSON.h"
#include "md4c.h"
extern "C" {
#include "entity.h"
}
#include "uni_algo/conv.h"
#include <cstring>
#include <cstdlib>
#include <unistd.h>

namespace np3d {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

std::string text(const cJSON* json, const char* key) {
    auto item = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsString(item) ? item->valuestring : "";
}

void newline(std::string& output) {
    if (!output.empty() && output.back() != '\n') output += '\n';
}
int leaveBlock(MD_BLOCKTYPE type, void*, void* opaque) {
    if (type != MD_BLOCK_DOC) newline(*static_cast<std::string*>(opaque));
    return 0;
}
int markdownText(MD_TEXTTYPE type, const char* data, MD_SIZE size, void* opaque) {
    auto& output = *static_cast<std::string*>(opaque);
    if (type == MD_TEXT_HTML) return 0;
    if (type == MD_TEXT_BR || type == MD_TEXT_SOFTBR) { newline(output); return 0; }
    if (type == MD_TEXT_ENTITY) {
        if (const ENTITY* entity = entity_lookup(data, size)) {
            std::u32string codepoints(1, entity->codepoints[0]);
            if (entity->codepoints[1]) codepoints += entity->codepoints[1];
            output += una::utf32to8(codepoints);
            return 0;
        }
    }
    output.append(data, size);
    return 0;
}
std::string plainMarkdown(const std::string& input) {
    std::string output;
    MD_PARSER parser = {};
    parser.enter_block = [](MD_BLOCKTYPE, void*, void*) { return 0; };
    parser.leave_block = leaveBlock;
    parser.enter_span = [](MD_SPANTYPE, void*, void*) { return 0; };
    parser.leave_span = [](MD_SPANTYPE, void*, void*) { return 0; };
    parser.text = markdownText;
    require(md_parse(input.data(), input.size(), &parser, &output) == 0, "Cannot read patch README");
    return output;
}

std::string entry(mz_zip_archive& archive, const char* name) {
    int index = mz_zip_reader_locate_file(&archive, name, nullptr, 0);
    if (index < 0) return "";
    auto iterator=mz_zip_reader_extract_iter_new(&archive,index,0);
    require(iterator,"Cannot extract patch information");
    std::string result;
    std::vector<char> buffer(BufferSize);
    while (true) {
        size_t size=mz_zip_reader_extract_iter_read(iterator,buffer.data(),buffer.size());
        require(iterator->status>=TINFL_STATUS_DONE,"Cannot decode patch information");
        if (!size) break;
        result.append(buffer.data(),size);
    }
    require(mz_zip_reader_extract_iter_free(iterator),"Cannot verify patch information");
    una::error error;
    una::strict::utf8to32u(result, error);
    require(!error, "Patch information is not UTF-8");
    return result;
}
}

PatchInfo inspectPatch(const std::string& path) {
    if (isDeltaPatch(path)) return {};
    auto archive = std::make_unique<mz_zip_archive>();
    *archive = {};
    require(mz_zip_reader_init_file(archive.get(), path.c_str(), 0), "Cannot open XZP archive");
    PatchInfo result;
    std::string metadata = entry(*archive, "metadata.json");
    if (!metadata.empty()) {
        Json json(cJSON_ParseWithLength(metadata.c_str(), metadata.size() + 1), cJSON_Delete);
        result.name = text(json.get(), "name");
        result.author = text(json.get(), "author");
        result.homepage = text(json.get(), "homepage");
        result.version = text(json.get(), "version");
        result.beta = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json.get(), "isBeta"));
    }
    if (mz_zip_reader_locate_file(archive.get(), "README.md", nullptr, 0) >= 0)
        result.readme = plainMarkdown(entry(*archive, "README.md"));
    else result.readme = entry(*archive, "README.txt");
    require(mz_zip_reader_end(archive.get()), "Cannot close XZP archive");
    return result;
}

Settings loadSettings(const std::string& root) {
    Settings settings;
    std::string path = root + "/config/settings.json";
    if (!exists(path)) return settings;
    auto input = std::make_shared<Disk>(path);
    auto bytes = readBytes(input, 0, input->size);
    bytes.push_back(0);
    Json json(cJSON_ParseWithLength(reinterpret_cast<const char*>(bytes.data()), bytes.size()), cJSON_Delete);
    require(cJSON_IsObject(json.get()), "Invalid settings.json");
    settings.language = text(json.get(), "language");
    auto games = cJSON_GetObjectItemCaseSensitive(json.get(), "games");
    for (auto game = games ? games->child : nullptr; game; game = game->next) {
        require(cJSON_IsArray(game), "Invalid game patch selection");
        for (auto item = game->child; item; item = item->next) {
            require(cJSON_IsString(item), "Invalid selected patch filename");
            settings.games[game->string].push_back(item->valuestring);
        }
    }
    return settings;
}

void saveSettings(const std::string& root, const Settings& settings) {
    makeDirectory(root + "/config");
    Json json(cJSON_CreateObject(), cJSON_Delete);
    require(bool(json), "Cannot allocate settings");
    cJSON_AddStringToObject(json.get(), "language", settings.language.c_str());
    auto games = cJSON_AddObjectToObject(json.get(), "games");
    for (const auto& game : settings.games) {
        auto array = cJSON_AddArrayToObject(games, game.first.c_str());
        for (const auto& patch : game.second) cJSON_AddItemToArray(array, cJSON_CreateString(patch.c_str()));
    }
    char* data = cJSON_PrintUnformatted(json.get());
    require(data, "Cannot serialize settings");
    std::string path = root + "/config/settings.json";
    { Disk output(path + ".new", "wb"); output.write(0, data, std::strlen(data)); output.sync(); }
    cJSON_free(data);
    if (exists(path)) require(unlink(path.c_str()) == 0, "Cannot replace settings.json");
    require(std::rename((path + ".new").c_str(), path.c_str()) == 0, "Cannot publish settings.json");
}
}
