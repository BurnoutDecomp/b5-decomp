// Compile anchor for the BrnCrashNavOptions.h HEADER TU (the three X360-emitted
// header-inlines live in the header; this TU pulls it through the compile gate) +
// the static load-table definitions (they belong to BrnCrashNavOptions.cpp when
// that TU lands -- move them there; values XEX-recovered from .data @0x82F26F50).

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavOptions.h"

namespace BrnGui
{
    // X360 .data @0x82F26F50: { muId = 0x8D, meType = 4 }; count @0x82F26F58 = 1.
    const CgsGui::sResourceTuple CrashNavOptions::maResourcesToLoad[1] =
    {
        { 0x8Du, static_cast<CgsGui::ResourceRequestTypes>(4) },
    };
    const u32 CrashNavOptions::muNumResourcesToLoad = 1;

    namespace
    {
        // Embed check: the slice types stay complete/instantiable as committed
        // headers evolve (never executed).
        void BrnCrashNavOptions_EmbedCheck(CrashNavOptions* lpState,
                                           const CgsGui::sResourceTuple** lppTuples,
                                           u32* lpuCount)
        {
            lpState->GetResourcesToLoad(lppTuples, lpuCount);
            lpState->SetSettingsFromProfile();
        }
        // Anchor the check so it is compiled (and only that).
        void (*gpBrnCrashNavOptionsEmbedCheck)(CrashNavOptions*,
                                               const CgsGui::sResourceTuple**, u32*) =
            &BrnCrashNavOptions_EmbedCheck;
    }
}
