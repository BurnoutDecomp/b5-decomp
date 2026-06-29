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

        // 0x82817720 - render a printf-style format into a 256-byte stack buffer and forward the
        // result to the virtual char* sink (the X360 StrStreamBase::AppendFormat). The X360 asserts
        // the formatted length did not exceed the buffer (CgsStringUtils.h copy guard); we keep that
        // bounds intent with the CGS_ASSERT machinery. The vararg pass-through forwards to vsnprintf.
        void AppendFormat(const char* lpcFormat, ...);

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

        // Un-hide the inherited scalar formatting overloads (s32/u32/u64/f32/void*/PrintMode):
        // overriding operator<<(const char*) below would otherwise hide every base-class overload
        // by name. The X360 streams the teleport-location ordinal as an int through this path
        // (StrStreamBase::AppendFormat) when building the "Reset Player Car" location labels.
        using StrStreamBase::operator<<;

        StrStreamBase& operator<<(const char* lpcText) override;

        // 0x82815E80 - the buffer sink: append text to mpcBuffer without overflowing it (X360
        // CgsDev::StrStream::Append - walk to the NUL then strncat with the remaining room
        // miBufferSize - used). operator<<(const char*) forwards here; exposed by name because the
        // X360 streams the assert/HUD label text through this method.
        void Append(const char* lpcText);

        char* GetBuffer() { return mpcBuffer; }

        // 0x82815E68 - clear the caller-supplied buffer back to the empty string and reset the
        // formatting mode to decimal (X360 CgsDev::StrStream::Reset: `*(this+4)=0` resets
        // mePrintMode, `*(this+8)` is the buffer whose first byte is zeroed). Called between
        // successive label builds in the debug HUD.
        void Reset()
        {
            mePrintMode = E_PRINTMODE_DECIMAL;
            if (mpcBuffer && miBufferSize > 0)
                mpcBuffer[0] = '\0';
        }

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

        // 0x82815EB8 - clear the inline buffer to the empty string and reset the formatting mode
        // to decimal (X360 CgsDev::SimpleStrStream::Reset: `*(this+4)=0` mePrintMode,
        // `*(this+8)=0` macCharBuffer[0]). Called between the three score cells of each debug-HUD
        // table row.
        void Reset()
        {
            mePrintMode = E_PRINTMODE_DECIMAL;
            macCharBuffer[0] = '\0';
        }

    private:
        static const s32 KI_BUFFER_SIZE = 256;   // DWARF CgsStrStream.h:306
        char macCharBuffer[KI_BUFFER_SIZE];
    };
}
