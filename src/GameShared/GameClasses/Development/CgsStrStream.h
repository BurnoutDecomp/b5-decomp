#pragma once

#include "types.hpp"
#include "rw/core/stdc/stdc.h"

// CgsDev::StrStreamBase - the engine's formatting text stream. Values are streamed in with
// operator<<; the scalar/format overloads render the value into a temp buffer (printf-style,
// via the KAC_* formats) and forward it to the virtual char* sink, which each concrete stream
// implements (StrStream -> a fixed char buffer; the debug-log stream -> debug output). The
// current PrintMode controls integer formatting (decimal / hex / hex-once). Reconstructed from
// the DecFIGS DWARF (Development/CgsStrStream.h/.cpp); the X360 lays Reset and Append at vtable+0 and vtable+4 and
// the call sites do `stream << "text" << value`.
// DWARF CgsStrStream.cpp declares only AppendFormat (:91) and Matches (:68): every operator<<
// and both Append64Int helpers are defined in this header (h:519-693, h:812/:844).
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
        StrStreamBase() { StrStreamBase::Reset(); }
        virtual void Reset();

        // Text insertion replaces null with <NULLSTRING> and calls the virtual Append sink.
        StrStreamBase& operator<<(const char* lpcText)
        {
            if (lpcText)
                Append(lpcText);
            else
                Append("<NULLSTRING>");
            return *this;
        }

        // Formatting overloads (render then forward to the sink).
        // DWARF CgsStrStream.h:534 / :559: both are defined in the header. ARTIST holds out-of-line
        // copies (s32 0x821F0E50, u32 0x821F0EC8) that most sites call, and inlines them inside loops
        // (e.g. OutputQueueContents 0x82230CC8: a call before its loop, four inline copies in it): the
        // XDK's own decision for a plain inline member under the original if-form asserts (D12).
        StrStreamBase& operator<<(s32 liInt)
        {
            switch (meCurrentPrintMode)
            {
            case E_PRINTMODE_DECIMAL:
                AppendFormat(KAC_INTEGER, liInt);
                break;
            case E_PRINTMODE_HEXONCE:
            case E_PRINTMODE_HEX:
                AppendFormat(KAC_HEX, liInt);
                meCurrentPrintMode = meCurrentPrintMode == E_PRINTMODE_HEXONCE ? E_PRINTMODE_DECIMAL : meCurrentPrintMode;
                break;
            }
            return *this;
        }
        StrStreamBase& operator<<(u32 liUInt)
        {
            switch (meCurrentPrintMode)
            {
            case E_PRINTMODE_DECIMAL:
                AppendFormat(KAC_UNSIGNED, liUInt);
                break;
            case E_PRINTMODE_HEXONCE:
            case E_PRINTMODE_HEX:
                AppendFormat(KAC_HEX, liUInt);
                meCurrentPrintMode = meCurrentPrintMode == E_PRINTMODE_HEXONCE ? E_PRINTMODE_DECIMAL : meCurrentPrintMode;
                break;
            }
            return *this;
        }
        // DWARF CgsStrStream.h:584, union UFloatInt (h:598-600) / lFloatInt (h:602) in the hex arm.
        // ARTIST 0x821F0F40: KAC_FLOAT with the value promoted for the vararg call (stfd/ld r5),
        // KAC_HEX with the float's bits read back through the stack union (stfs/lwz r5).
        StrStreamBase& operator<<(f32 lfReal)
        {
            switch (meCurrentPrintMode)
            {
            case E_PRINTMODE_DECIMAL:
                AppendFormat(KAC_FLOAT, lfReal);
                break;
            case E_PRINTMODE_HEXONCE:
            case E_PRINTMODE_HEX:
                {
                    union UFloatInt
                    {
                        f32 mfReal;
                        s32 miInt;
                    };
                    UFloatInt lFloatInt;
                    lFloatInt.mfReal = lfReal;
                    AppendFormat(KAC_HEX, lFloatInt.miInt);
                    meCurrentPrintMode = meCurrentPrintMode == E_PRINTMODE_HEXONCE ? E_PRINTMODE_DECIMAL : meCurrentPrintMode;
                }
                break;
            }
            return *this;
        }
        // DWARF CgsStrStream.h:625, local leSavedPrintMode (h:627).
        StrStreamBase& operator<<(void* lpPointer)
        {
            const PrintMode leSavedPrintMode = meCurrentPrintMode;
            *this << E_PRINTMODE_HEX << reinterpret_cast<u32>(lpPointer) << leSavedPrintMode;
            return *this;
        }
        // DWARF CgsStrStream.h:635: defined in the header. ARTIST has no out-of-line copy and
        // stores the mode straight into meCurrentPrintMode at each site (e.g. RaceCarStreamer::
        // OnResourceUnloading 0x822A18F4 `stw r11,4(r31)`).
        StrStreamBase& operator<<(PrintMode lePrintMode)
        {
            meCurrentPrintMode = lePrintMode;
            return *this;
        }
        // DWARF CgsStrStream.h:668 (ARTIST copy 0x82203EE8: Append64IntDecimal inline, Append64IntHex
        // called at 0x821F1138).
        StrStreamBase& operator<<(u64 lu64Int)
        {
            switch (meCurrentPrintMode)
            {
            case E_PRINTMODE_DECIMAL:
                Append64IntDecimal(lu64Int);
                break;
            case E_PRINTMODE_HEXONCE:
            case E_PRINTMODE_HEX:
                Append64IntHex(lu64Int);
                meCurrentPrintMode = meCurrentPrintMode == E_PRINTMODE_HEXONCE ? E_PRINTMODE_DECIMAL : meCurrentPrintMode;
                break;
            }
            return *this;
        }
        // DWARF CgsStrStream.h:693. PS3 0x33823C hands KAC_TRUE / KAC_FALSE straight to the virtual
        // Append (vtable+4), without operator<<(const char*)'s null check.
        StrStreamBase& operator<<(bool lbValue)
        {
            if (lbValue)
                Append(KAC_TRUE);
            else
                Append(KAC_FALSE);
            return *this;
        }

        // 0x82817720 - render a printf-style format into a 256-byte stack buffer and forward the
        // result to the virtual char* sink (the X360 StrStreamBase::AppendFormat). The X360 asserts
        // the formatted length did not exceed the buffer (CgsStringUtils.h copy guard); we keep that
        // bounds intent with the CGS_ASSERT machinery. The vararg pass-through forwards to vsnprintf.
        void AppendFormat(const char* lpcFormat, ...);

    protected:
        explicit StrStreamBase(PrintMode leMode) : meCurrentPrintMode(leMode) {}
        virtual void Append(const char* lpcText) = 0;

        // DWARF CgsStrStream.h:812, local lacNumber[22] (h:822).
        void Append64IntDecimal(u64 lu64Int)
        {
            char lacNumber[22];
            rw::core::stdc::ConvertI64ToA(static_cast<s64>(lu64Int), lacNumber, 10);
            Append(lacNumber);
        }
        // DWARF CgsStrStream.h:844, locals lA / lB (h:847/:848); ARTIST copy 0x821F1138.
        void Append64IntHex(u64 lu64Int)
        {
            const u32 lA = static_cast<u32>(lu64Int >> 32);
            const u32 lB = static_cast<u32>(lu64Int);
            AppendFormat(KAC_PADDED_HEX, lA);
            AppendFormat(KAC_PADDED_HEX_NO_PREFIX, lB);
        }

        PrintMode meCurrentPrintMode;   // DWARF CgsStrStream.h:159

    private:
        // DWARF CgsStrStream.h:163-179; ARTIST .data 0x82F3193C..0x82F31974 in this order.
        static const char* KAC_INTEGER;
        static const char* KAC_UNSIGNED;
        static const char* KAC_HEX;
        static const char* KAC_PADDED_HEX;
        static const char* KAC_PADDED_HEX_NO_PREFIX;
        static const char* KAC_FLOAT;
        static const char* KAC_CHAR;
        static const char* KAC_TRUE;
        static const char* KAC_FALSE;
        static const char* KAC_VECTOR2;
        static const char* KAC_VECTOR3;
        static const char* KAC_VECTOR4;
        static const char* KAC_MATRIX33;
        static const char* KAC_MATRIX44;
        static const char* KAC_LF;
        static const s32 KI_FORMAT_BUFFER_SIZE;
    };

    // CgsDev::StrStream - a StrStreamBase that appends into a caller-supplied fixed char buffer
    // (used to build assert/log messages on the stack).
    struct StrStream : public StrStreamBase
    {
        StrStream(char* lpcBuffer, s32 liBufferSize)
            : mpcBuffer(lpcBuffer)
            , miBufferSize(liBufferSize)
        {
            lpcBuffer[0] = '\0';
        }

        // ARTIST's inline assert setup constructs the stream directly in decimal mode.
        StrStream(char* lpcBuffer, s32 liBufferSize, PrintMode leMode)
            : StrStreamBase(leMode)
            , mpcBuffer(lpcBuffer)
            , miBufferSize(liBufferSize)
        {
            lpcBuffer[0] = '\0';
        }

        char* GetBuffer() { return mpcBuffer; }

        // 0x82815E68 - clear the caller-supplied buffer back to the empty string and reset the
        // formatting mode to decimal (X360 CgsDev::StrStream::Reset: `*(this+4)=0` resets
        // meCurrentPrintMode, `*(this+8)` is the buffer whose first byte is zeroed). Called between
        // successive label builds in the debug HUD.
        virtual void Reset()
        {
            meCurrentPrintMode = E_PRINTMODE_DECIMAL;
            mpcBuffer[0] = '\0';
        }

    protected:
        // 0x82815E80 - the buffer sink: append text to mpcBuffer without overflowing it (X360
        // CgsDev::StrStream::Append - walk to the NUL then strncat with the remaining room
        // miBufferSize - used). operator<<(const char*) forwards here; exposed by name because the
        // X360 streams the assert/HUD label text through this method.
        virtual void Append(const char* lpcText);


    private:
        char* mpcBuffer;
        s32   miBufferSize;
    };

    // CgsDev::SimpleStrStream - a StrStreamBase that owns an INLINE fixed char buffer (256 bytes),
    // used to build short debug strings on the stack/in a member without a caller-supplied buffer
    // (e.g. BrnGameState::StuntManagerDebugComponent::maStrStreams[3], BrnAI::BuzzBy::DrawBuzzTimer).
    // Distinct vtable from StrStreamBase (X360 off_82014B00) - it overrides the char* sink to append
    // into macCharBuffer. Object size = 264B (0x108): StrStreamBase (vtable@0 + meCurrentPrintMode@4 = 8B) +
    // macCharBuffer[256]@+0x08, matching the X360 ctor stride. KI_BUFFER_SIZE / accessors + the
    // scalar overloads (int32_t @969, float32_t @1025) are from DWARF CgsStrStream.h.
    struct SimpleStrStream : public StrStreamBase
    {
        SimpleStrStream();

        StrStreamBase& operator<<(const char* lpcText);
        StrStreamBase& operator<<(s32 liValue);
        StrStreamBase& operator<<(f32 lfValue);

        char* GetBuffer() { return macCharBuffer; }

        // 0x82815EB8 - clear the inline buffer to the empty string and reset the formatting mode
        // to decimal (X360 CgsDev::SimpleStrStream::Reset: `*(this+4)=0` meCurrentPrintMode,
        // `*(this+8)=0` macCharBuffer[0]). Called between the three score cells of each debug-HUD
        // table row.
        virtual void Reset()
        {
            meCurrentPrintMode = E_PRINTMODE_DECIMAL;
            macCharBuffer[0] = '\0';
        }

    protected:
        virtual void Append(const char* lpcText);

    private:
        static const s32 KI_BUFFER_SIZE = 256;   // DWARF CgsStrStream.h:306
        char macCharBuffer[KI_BUFFER_SIZE];
    };
}
