#ifndef CGS_LOG_CHANNEL_OUTPUT_H
#define CGS_LOG_CHANNEL_OUTPUT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase (base)

// CgsDev::Log::LogChannelOutput - a StrStreamBase whose char* sink writes the text to the engine
// console/unified log, optionally prefixed by a channel tag. Reconstructed from the DecFIGS DWARF
// (Development/Log/CgsLogChannelOutput.h) + the X360 ARTIST Append body @ 0x8229FB20. Layout
// (DWARF): StrStreamBase (vtable@0 + mePrintMode@4 = 8 bytes) then miChannel @ +0x08 (-1 ==
// unchannelled). The default ctor (inlined at embed sites, e.g. BrnGame::AutoTestManager @
// 0x827DB4C0) runs StrStreamBase() then stores -1 into miChannel before patching the derived vtable.
namespace CgsDev
{
namespace Log
{
struct LogChannelOutput : public StrStreamBase
{
    // CgsLogChannelOutput.h:53 (DWARF) -- the user-channel ids (the unchannelled default is -1).
    enum EUserChannel
    {
        FIRST_CHANNEL = 3,
        CHANNEL0      = 3,
        CHANNEL1      = 4,
        CHANNEL2      = 5,
        CHANNEL3      = 6,
        CHANNEL4      = 7,
        CHANNEL5      = 8,
        CHANNEL6      = 9,
        CHANNEL7      = 10,
        CHANNEL8      = 11,
        CHANNEL9      = 12,
        CHANNEL10     = 13,
        CHANNEL11     = 14,
        CHANNEL12     = 15,
        LAST_CHANNEL  = 15,
    };

    // CgsLogChannelOutput.h:76 (DWARF). Unchannelled (-1) until SetChannel.
    LogChannelOutput() : miChannel(-1) {}

    // CgsLogChannelOutput.h:84 (DWARF).
    void SetChannel(EUserChannel leChannel) { miChannel = static_cast<s32>(leChannel); }

    // Concrete sink so the StrStreamBase pure-virtual is satisfied; forwards to Append.
    StrStreamBase& operator<<(const char* lpcText) override
    {
        Append(lpcText);
        return *this;
    }

protected:
    // X360 0x8229FB20 (CgsLogChannelOutput.h:118 DWARF). The char* sink: write the text to the
    // unified log, channel-prefixed when miChannel != -1.
    virtual void Append(const char* lpcText);

private:
    s32 miChannel;   // [+0x08] channel id, -1 == unchannelled -- CgsLogChannelOutput.h:97
};
}
}
#endif
