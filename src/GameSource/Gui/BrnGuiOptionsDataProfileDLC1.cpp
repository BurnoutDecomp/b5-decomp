#include "GameSource/Gui/BrnGuiOptionsDataProfileDLC1.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log::gpDebugPrint, Message::gxMessageFilterFlags

namespace BrnGui
{
// Outlined from BrnGui::GuiCache::Construct @0x82505860 (asm 0x825060EC..0x82506110):
// the cache's live DLC1 options block is initialised to {version 1, reserved 0, flags 0}.
// Identical store set to ValidateProfile's uninitialised-sentinel reset arm below, which
// is why the X360 emits the sequence twice rather than calling a shared helper.
void OptionsDataProfileDLC1::Construct()
{
    miVersion  = KI_VERSION_CURRENT;
    miReserved = 0;
    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
    {
        maFlags[liIndex] = 0;
    }
}

// 0x824F0C38  BrnGui::OptionsDataProfileDLC1::ValidateProfile
// Semantic parity with the X360 body:
//   liVersion = miVersion;                                  // 0(r31)
//   if (liVersion == KI_VERSION_UNINITIALISED) {
//       if (filter & 1) gpDebugPrint << "... uninitialised ...\n";
//       miReserved = 0; miVersion = 1; clear maFlags[0..7];
//       return true;
//   }
//   if (liVersion == 1) return true;
//   if (filter & 1) gpDebugPrint << "... mismatch, expected " << 1 << ", got "
//                                << liVersion << "\n";
//   return false;
bool OptionsDataProfileDLC1::ValidateProfile()
{
    using namespace CgsDev;

    const s32 liVersion = miVersion;

    if (liVersion == KI_VERSION_UNINITIALISED)
    {
        if ((Message::gxMessageFilterFlags & 1) != 0)
        {
            *Log::gpDebugPrint
                << "Options Data Profile DLC1 is uninitialised. Non-upgraded profile?\n";
        }

        miReserved = 0;
        miVersion  = KI_VERSION_CURRENT;
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            maFlags[liIndex] = 0;
        }
        return true;
    }

    if (liVersion == KI_VERSION_CURRENT)
    {
        return true;
    }

    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        *Log::gpDebugPrint
            << "Options Data Profile version mismatch, expected "
            << KI_VERSION_CURRENT
            << ", got "
            << liVersion
            << "\n";
    }

    return false;
}
}
