#include <cassert>
#include <vector>
#include <bsp/audio_codec_transition.h>

struct Codec {
    bool inputOpen = true;
    bool outputOpen = true;
    int step = 0;
    int failAt = -1;
    bool failCleanup = false;
    bool failed = false;
    std::vector<int> steps;
    int call(int action) {
        steps.push_back(action);
        const bool fail = step++ == failAt || (failed && failCleanup);
        if (fail) failed = true;
        return fail ? -7 : 0;
    }
};
static int closeOutput(void* context) {
    auto& codec = *static_cast<Codec*>(context);
    const int result = codec.call(0);
    if (!result) codec.outputOpen = false;
    return result;
}
static int closeInput(void* context) {
    auto& codec = *static_cast<Codec*>(context);
    const int result = codec.call(1);
    if (!result) codec.inputOpen = false;
    return result;
}
static int disable(void* context) { return static_cast<Codec*>(context)->call(2); }
static int formatInput(void* context, const audio_codec_formats_t* formats) {
    auto& codec = *static_cast<Codec*>(context);
    assert(!codec.inputOpen && !codec.outputOpen);
    assert(formats->input_channels == 4 && formats->input_bits == 16);
    return codec.call(3);
}
static int openInput(void* context, const audio_codec_formats_t*) {
    auto& codec = *static_cast<Codec*>(context);
    assert(!codec.inputOpen && !codec.outputOpen);
    const int result = codec.call(4);
    if (!result) codec.inputOpen = true;
    return result;
}
static int formatOutput(void* context, const audio_codec_formats_t*) {
    auto& codec = *static_cast<Codec*>(context);
    assert(codec.inputOpen && !codec.outputOpen);
    return codec.call(5);
}
static int openOutput(void* context, const audio_codec_formats_t*) {
    auto& codec = *static_cast<Codec*>(context);
    assert(codec.inputOpen && !codec.outputOpen);
    const int result = codec.call(6);
    if (!result) codec.outputOpen = true;
    return result;
}
static int enable(void* context) { return static_cast<Codec*>(context)->call(7); }

int main()
{
    for (auto formats : {audio_codec_playback_formats(44100, 16, 2), audio_codec_record_formats(48000, 16, 4)}) {
        for (int fault = -1; fault < 8; ++fault) {
            Codec codec;
            codec.failAt = fault;
            const audio_codec_transition_ops_t ops {&codec, closeOutput, closeInput, disable,
                                                    formatInput, openInput, formatOutput, openOutput, enable};
            const auto result = audio_codec_transition(&formats, &ops);
            assert(result.ready == (fault == -1));
            assert((result.error == 0) == result.ready);
            assert(result.cleanup_succeeded);
            if (fault == -1) assert(codec.inputOpen && codec.outputOpen);
            else assert(!codec.inputOpen && !codec.outputOpen);
            assert(codec.steps[0] == 0 && codec.steps[1] == 1);
        }
    }
    Codec codec;
    codec.failAt = 4;
    codec.failCleanup = true;
    const auto formats = audio_codec_playback_formats(44100, 16, 2);
    const audio_codec_transition_ops_t ops {&codec, closeOutput, closeInput, disable,
                                            formatInput, openInput, formatOutput, openOutput, enable};
    const auto result = audio_codec_transition(&formats, &ops);
    assert(!result.ready && !result.cleanup_succeeded && result.error != 0);
    return 0;
}
