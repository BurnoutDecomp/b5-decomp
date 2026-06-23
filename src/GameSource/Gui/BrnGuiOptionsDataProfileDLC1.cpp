#include "GameSource/Gui/BrnGuiOptionsDataProfileDLC1.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log::gpDebugPrint, Message::gxMessageFilterFlags

namespace BrnGui
{
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
