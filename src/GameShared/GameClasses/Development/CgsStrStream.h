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

    private:
        char* mpcBuffer;
        s32   miBufferSize;
    };
}
