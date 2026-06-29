#ifndef CGS_LOG_COMBINED_H
#define CGS_LOG_COMBINED_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase (base + child type)

// CgsDev::Log::LogCombined - a composite log sink: it holds up to KI_MAX_COMBINED_STREAMS child
// StrStreamBase pointers and fans every Append() out to each non-null child. Being a StrStreamBase
// itself, a LogCombined can be nested inside another combined sink. Reconstructed from the DecFIGS
// DWARF (Development/Log/CgsLogCombined.h) + the X360 ARTIST bodies AddStream @ 0x82817660 /
// Append @ 0x82817698.
//
// Layout (DWARF): StrStreamBase (vtable@0 + mePrintMode@4 = 8 bytes) then mapStreams[4] @ +0x08.
// The default ctor (inlined at embed sites, e.g. BrnGame::AutoTestManager @ 0x827DB4C0) runs
// StrStreamBase() then zeroes the four mapStreams slots (stw of 0 at +0x08/+0x0C/+0x10/+0x14)
// before patching in LogCombined's own vtable.
namespace CgsDev
{
namespace Log
{
    // CgsLogCombined.h:63 (DWARF) -- the fixed number of child stream slots.
    const s32 KI_MAX_COMBINED_STREAMS = 4;

    struct LogCombined : public StrStreamBase
    {
        // CgsLogCombined.h:87 (DWARF). All four child slots start empty.
        LogCombined()
        {
            for (s32 li = 0; li < KI_MAX_COMBINED_STREAMS; ++li)
                mapStreams[li] = nullptr;
        }

        // X360 0x82817660 (CgsLogCombined.cpp:49). Store lpStream into the first empty slot; once
        // all four are taken the request is silently dropped.
        void AddStream(StrStreamBase* lpStream);

        // CgsLogCombined.cpp:74. Drop lpStream from the slot it occupies (if present).
        void RemoveStream(StrStreamBase* lpStream);

        // CgsLogCombined.cpp:123. Clear all child slots.
        void RemoveAllStreams();

        // Concrete sink so the StrStreamBase pure-virtual is satisfied; forwards to Append.
        StrStreamBase& operator<<(const char* lpcText) override
        {
            Append(lpcText);
            return *this;
        }

    protected:
        // X360 0x82817698 (CgsLogCombined.cpp:99). The char* sink: forward the text to every
        // non-null child stream (null text becomes "<NULLSTRING>").
        virtual void Append(const char* lpcText);

    private:
        StrStreamBase* mapStreams[KI_MAX_COMBINED_STREAMS];   // [+0x08] child fan-out slots
    };
}
}
#endif
