#ifndef CGS_SOUND_PCM_TRACE_H
#define CGS_SOUND_PCM_TRACE_H
// FLAG PC-platform witness: opt-in, read-only source/decoded-PCM evidence.
// BRN_SOUND_PCM_TRACE names a local trace file. No sound/control data is changed.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace CgsSound { namespace PcmTrace {
inline FILE* File()
{
    static FILE* file = []() -> FILE* {
        const char* path = std::getenv("BRN_SOUND_PCM_TRACE");
        return path && *path ? std::fopen(path, "w") : nullptr;
    }();
    return file;
}
inline void Log(const char* format, ...)
{
    if (FILE* file = File()) {
        va_list args; va_start(args, format);
        std::vfprintf(file, format, args); va_end(args);
        std::fflush(file);
    }
}
struct Entry { const void* player; const void* sample; unsigned voice; bool emitted; };
inline Entry* Entries() { static Entry entries[256] = {}; return entries; }
inline void Register(const void* player, const void* sample, unsigned voice)
{
    if (!File()) return;
    static unsigned next = 0;
    Entry* entry = nullptr;
    for (unsigned i = 0; i < 256; ++i)
        if (Entries()[i].player == player) { entry = Entries() + i; break; }
    if (!entry) entry = Entries() + (next++ % 256);
    *entry = {player, sample, voice, false};
    Log("aems-player player=%p sample=%p voice=%u\n", player, sample, voice);
}
inline void Measure(const void* player, const float* pcm, unsigned count,
                    unsigned channels, unsigned stride)
{
    if (!File() || !count) return;
    for (unsigned i = 0; i < 256; ++i) {
        Entry& entry = Entries()[i];
        if (entry.player != player || entry.emitted) continue;
        double power = 0.0; float peak = 0.0f;
        for (unsigned ch = 0; ch < channels; ++ch)
            for (unsigned s = 0; s < count; ++s) {
                const float value = pcm[ch * stride + s];
                power += static_cast<double>(value) * value;
                if (std::fabs(value) > peak) peak = std::fabs(value);
            }
        if (peak > 0.0f) {
            entry.emitted = true;
            Log("aems-pcm player=%p sample=%p voice=%u frames=%u channels=%u rms=%.9g peak=%.9g\n",
                player, entry.sample, entry.voice, count, channels,
                std::sqrt(power / (count * channels)), peak);
        }
        return;
    }
}
// FLAG PC-platform witness: first nonzero output of a DSP effect, after processing.
inline void MeasureEffect(const char* name, const void* effect, const float* pcm,
                          unsigned count, unsigned channels, unsigned stride)
{
    if (!File() || !count || !channels) return;
    static const void* emitted[32] = {};
    static unsigned next = 0;
    for (const void* value : emitted) if (value == effect) return;
    double power = 0.0; float peak = 0.0f;
    for (unsigned ch = 0; ch < channels; ++ch)
        for (unsigned i = 0; i < count; ++i) {
            const float value = pcm[ch * stride + i];
            power += static_cast<double>(value) * value;
            if (std::fabs(value) > peak) peak = std::fabs(value);
        }
    if (peak > 0.0f) {
        emitted[next++ % 32] = effect;
        Log("effect-pcm name=%s effect=%p frames=%u channels=%u rms=%.9g peak=%.9g\n",
            name, effect, count, channels, std::sqrt(power / (count * channels)), peak);
    }
}

}}
#endif
