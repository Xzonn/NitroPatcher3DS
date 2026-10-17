#include "core.h"

#include <cstring>

extern "C" {
#include "xdelta3.h"
#include "miniz.h"
}

namespace np3d {

struct DeltaOutput {
    SourcePtr source;
    std::shared_ptr<Map> target;
    uint32_t adler = 1;
    std::string label;
    std::vector<uint8_t> buffer = std::vector<uint8_t>(BufferSize);
};

static int emit(xd3_stream* stream, const xd3_hinst* instruction, const uint8_t* data) {
    auto& state = *static_cast<DeltaOutput*>(stream->opaque);
    require(instruction->size <= 0x40000000 && instruction->addr <= 0x40000000,
        "VCDIFF instruction exceeds ROM size limit");
    uint32_t length = instruction->size;
    uint32_t start = state.target->size;
    if (instruction->type == XD3_ADD) state.target->literal(data, length);
    else if (instruction->type == XD3_RUN) state.target->append(Kind::Fill, *data, length);
    else {
        uint32_t address = instruction->addr;
        if (address < stream->dec_cpylen) {
            require(stream->dec_cpyoff <= 0x40000000, "VCDIFF source offset exceeds ROM size limit");
            SourcePtr input = (stream->dec_win_ind & 2) ? state.target : state.source;
            require(stream->dec_cpyoff <= input->size && stream->dec_cpylen <= input->size - stream->dec_cpyoff,
                "VCDIFF source window exceeds source file");
            uint32_t take = std::min<uint32_t>(length, stream->dec_cpylen - address);
            state.target->copy(input, stream->dec_cpyoff + address, take);
            length -= take;
            address = 0;
        } else address -= stream->dec_cpylen;
        if (length) {
            uint32_t offset = stream->dec_winstart + address;
            require(offset < state.target->size, "VCDIFF target copy references unwritten data");
            uint32_t distance = state.target->size - offset;
            while (length) {
                uint32_t take = std::min(length, distance);
                state.target->copy(state.target, offset, take);
                offset += take;
                length -= take;
            }
        }
    }
    for (uint32_t offset = start; offset < state.target->size;) {
        uint32_t take = std::min(BufferSize, state.target->size - offset);
        state.target->read(offset, state.buffer.data(), take);
        state.adler = mz_adler32(state.adler, state.buffer.data(), take);
        offset += take;
        progress("Applying patch section", offset - stream->dec_winstart, stream->dec_tgtlen, true, state.label.c_str());
    }
    return 0;
}

static uint32_t checksum(xd3_stream* stream) {
    return static_cast<DeltaOutput*>(stream->opaque)->adler;
}

std::shared_ptr<Map> applyDelta(Workspace& work, SourcePtr source, const std::string& path, const std::string& label) {
    logPreparation("DELTA begin name=" + label + " path=" + path + " sourceSize=" + std::to_string(source->size));
    DeltaOutput output;
    output.source = source;
    output.label = label;
    output.target = work.map();
    Disk input(path);
    xd3_stream stream = {};
    xd3_config config;
    xd3_init_config(&config, 0);
    config.opaque = &output;
    require(xd3_config_stream(&stream, &config) == 0, "Cannot initialize xdelta3");
    stream.external_emit = emit;
    stream.external_adler32 = checksum;
    auto reject = [&](const char* message) -> std::shared_ptr<Map> {
        work.error = std::string("Cannot decode VCDIFF: ") + message + " | " + label;
        xd3_free_stream(&stream);
        return nullptr;
    };
    xd3_source original = {};
    original.blksize = BufferSize;
    require(xd3_set_source_and_size(&stream, &original, source->size) == 0, "Cannot configure VCDIFF source");
    std::vector<uint8_t> buffer(BufferSize);
    uint32_t offset = 0;
    while (true) {
        logPreparation("DELTA decode begin inputOffset=" + std::to_string(offset));
        int status = xd3_decode_input(&stream);
        logPreparation("DELTA decode done status=" + std::to_string(status) + " outputSize=" + std::to_string(output.target->size));
        if (status == XD3_INPUT) {
            if (offset == input.size) {
                if (xd3_close_stream(&stream) != 0) return reject(stream.msg ? stream.msg : "Incomplete VCDIFF stream");
                break;
            }
            uint32_t take = std::min(BufferSize, input.size - offset);
            logPreparation("DELTA read begin offset=" + std::to_string(offset) + " bytes=" + std::to_string(take));
            input.read(offset, buffer.data(), take);
            logPreparation("DELTA read done offset=" + std::to_string(offset));
            offset += take;
            xd3_avail_input(&stream, buffer.data(), take);
            if (offset == input.size) xd3_set_flags(&stream, stream.flags | XD3_FLUSH);
        } else if (status == XD3_OUTPUT) {
            xd3_consume_output(&stream);
        } else if (status == XD3_GOTHEADER || status == XD3_WINSTART) {
            output.adler = 1;
            progress("Applying patch section", 0, stream.dec_tgtlen, true, label.c_str());
        } else if (status != XD3_WINFINISH) {
            return reject(stream.msg ? stream.msg : "invalid stream");
        }
    }
    if (!stream.dec_window_count) return reject("VCDIFF contains no target windows");
    xd3_free_stream(&stream);
    output.target->finish();
    logPreparation("DELTA done name=" + label + " outputSize=" + std::to_string(output.target->size));
    return output.target;
}

}
