#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t rate;
    uint32_t input_bits;
    uint32_t input_channels;
    uint32_t output_bits;
    uint32_t output_channels;
} audio_codec_formats_t;

static inline audio_codec_formats_t audio_codec_playback_formats(uint32_t rate, uint32_t bits, uint32_t channels)
{
    audio_codec_formats_t formats = {rate, 16, 4, bits, channels};
    return formats;
}
static inline audio_codec_formats_t audio_codec_record_formats(uint32_t rate, uint32_t bits, uint32_t channels)
{
    audio_codec_formats_t formats = {rate, bits, channels, 16, 2};
    return formats;
}

typedef struct {
    void* context;
    int (*close_output)(void*);
    int (*close_input)(void*);
    int (*disable)(void*);
    int (*format_input)(void*, const audio_codec_formats_t*);
    int (*open_input)(void*, const audio_codec_formats_t*);
    int (*format_output)(void*, const audio_codec_formats_t*);
    int (*open_output)(void*, const audio_codec_formats_t*);
    int (*enable)(void*);
} audio_codec_transition_ops_t;

typedef struct {
    int error;
    bool ready;
    bool cleanup_succeeded;
} audio_codec_transition_result_t;

// Caller owns the codec for this entire transaction. No direction is opened at
// a different shared clock until both old directions have closed and disabled.
static inline audio_codec_transition_result_t audio_codec_transition(
    const audio_codec_formats_t* formats, const audio_codec_transition_ops_t* ops)
{
    audio_codec_transition_result_t result = {0, false, true};
    void* ctx = ops->context;
    int out = ops->close_output(ctx);
    int in = ops->close_input(ctx);
    result.error = out ? out : in;
    if (!result.error) result.error = ops->disable(ctx);
    if (!result.error) result.error = ops->format_input(ctx, formats);
    if (!result.error) result.error = ops->open_input(ctx, formats);
    if (!result.error) result.error = ops->format_output(ctx, formats);
    if (!result.error) result.error = ops->open_output(ctx, formats);
    if (!result.error) result.error = ops->enable(ctx);
    result.ready = result.error == 0;
    if (!result.ready) {
        out = ops->close_output(ctx);
        in = ops->close_input(ctx);
        int disabled = ops->disable(ctx);
        result.cleanup_succeeded = out == 0 && in == 0 && disabled == 0;
    }
    return result;
}
