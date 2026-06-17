#pragma once

#include "types.hpp"

// CgsDev::StrStreamBase - the engine's formatting text stream. Values are streamed in with
// operator<<; the scalar/format overloads render the value into a temp buffer (printf-style,
// via the KAC_* formats) and forward it to the virtual char* sink, which each concrete stream
// implements (StrStream -> a fixed char buffer; the debug-log stream -> debug output). The
// current PrintMode controls integer formatting (decimal / hex / hex-once). Reconstructed from
// the DecFIGS DWARF (Development/CgsStrStream.h/.cpp); the X360 lays the sink at vtable+4 and
// the call sites do `stream << "text" << value`.
namespace CgsDev
{
    enum PrintMode
    {
        E_PRINTMODE_DECIMAL = 0,
        E_PRINTMODE_HEX     = 1,
        E_PRINTMODE_HEXONCE = 2,
    };

    struct StrStreamBase
    {
        StrStreamBase();
        virtual ~StrStreamBase() {}

        // The sink: each concrete stream writes the string somewhere (buffer / debug output).
        virtual StrStreamBase& operator<<(const char* lpcText) = 0;

        // Formatting overloads (render then forward to the sink).
        StrStreamBase& operator<<(s32 liValue);
        StrStreamBase& operator<<(u32 luValue);
        StrStreamBase& operator<<(u64 luValue);
        StrStreamBase& operator<<(f32 lfValue);
        StrStreamBase& operator<<(void* lpValue);
        StrStreamBase& operator<<(PrintMode leMode);

    protected:
        void Append64IntDecimal(u64 luValue);
        void Append64IntHex(u64 luValue);

        PrintMode mePrintMode;
    };

    // CgsDev::StrStream - a StrStreamBase that appends into a caller-supplied fixed char buffer
    // (used to build assert/log messages on the stack).
    struct StrStream : public StrStreamBase
    {
        StrStream(char* lpcBuffer, s32 liBufferSize);

        StrStreamBase& operator<<(const char* lpcText) override;
        char* GetBuffer() { return mpcBuffer; }

        // Clear the caller-supplied buffer back to the empty string (X360 CgsDev::StrStream::Reset,
        // called between successive label builds in the debug HUD; the X360 `stb 0,0(buffer)`).
        void Reset() { if (mpcBuffer && miBufferSize > 0) mpcBuffer[0] = '\0'; }

    private:
        char* mpcBuffer;
        s32   miBufferSize;
    };

    // CgsDev::SimpleStrStream - a StrStreamBase that owns an INLINE fixed char buffer (256 bytes),
    // used to build short debug strings on the stack/in a member without a caller-supplied buffer
    // (e.g. BrnGameState::StuntManagerDebugComponent::maStrStreams[3], BrnAI::BuzzBy::DrawBuzzTimer).
    // Distinct vtable from StrStreamBase (X360 off_82014B00) - it overrides the char* sink to append
    // into macCharBuffer. Object size = 264B (0x108): StrStreamBase (vtable@0 + mePrintMode@4 = 8B) +
    // macCharBuffer[256]@+0x08, matching the X360 ctor stride. KI_BUFFER_SIZE / accessors + the
    // scalar overloads (int32_t @969, float32_t @1025) are from DWARF CgsStrStream.h.
    struct SimpleStrStream : public StrStreamBase
    {
        SimpleStrStream();

        StrStreamBase& operator<<(const char* lpcText) override;
        StrStreamBase& operator<<(s32 liValue);
        StrStreamBase& operator<<(f32 lfValue);

        char* GetBuffer() { return macCharBuffer; }

        // Clear the inline buffer back to the empty string (X360 CgsDev::SimpleStrStream::Reset,
        // called between the three score cells of each debug-HUD table row).
        void Reset() { macCharBuffer[0] = '\0'; }

    private:
        static const s32 KI_BUFFER_SIZE = 256;   // DWARF CgsStrStream.h:306
        char macCharBuffer[KI_BUFFER_SIZE];
    };
}
