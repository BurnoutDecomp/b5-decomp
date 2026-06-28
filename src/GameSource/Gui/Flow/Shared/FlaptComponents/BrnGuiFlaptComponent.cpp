// ===================================================================================
// BrnGui::BrnFlaptComponent -- shared apt-component base
//   class:BrnGui::BrnFlaptComponent
//
//   Prepare @ 0x8240E670
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Non-static virtual member (asm: r3 =
// this; the composite component key is built into a stack buffer; the bound clip's
// MovieClipRef is written into mAptRef @ this+0x04/+0x08).
//
// The X360 builds the lookup key as "<parent>_<name>" when a parent prefix is
// supplied (CgsCore::SnPrintf into a 128-byte stack buffer, hard NUL-terminated at
// index 127), otherwise it looks the bare name up. It then resolves the component
// out of the supplied FileRef, binds the returned MovieClipRef into mAptRef, asserts
// the bound clip instance is valid, and resets that clip's timeline.
//
// The X360 dev-asserts (lacName != NULL, mpMovieClipInst) are emitted as the
// Begin/Fire/End assert triplet; per the module house style (see the BrnFlaptFileRef
// exemplar) they fold into CGS_ASSERT(cond,"msg"), discarding the baked file/line in
// favour of __FILE__/__LINE__. The r3 the asm tail-returns from ResetTimeline is a
// tail-call artifact; logically Prepare returns void (matching the DecFIGS subclass
// override shapes), so the reset is the final statement.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"

#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"             // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"   // BrnFlapt::MovieClipInstance::ResetTimeline
#include "GameShared/GameClasses/Core/CgsStringUtils.h"       // CgsCore::SnPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

namespace BrnGui
{
    // Longest composite component key the X360 formats on the stack (127 usable
    // chars + the hard NUL terminator it writes at index 127).
    static const s32 KI_COMPONENT_PATH_LENGTH = 128;

    // @ 0x8240E670 -- resolve, bind and reset the named apt component.
    void BrnFlaptComponent::Prepare(const char* lacName,
                                    const BrnFlapt::FileRef& lFile,
                                    const char* lacParentName)
    {
        CGS_ASSERT(lacName != 0, "lacName != NULL");

        const char* lpcComponentName = lacName;

        char lacComponentPath[KI_COMPONENT_PATH_LENGTH];
        if (lacParentName != 0)
        {
            // Composite key "<parent>_<name>".
            CgsCore::SnPrintf(lacComponentPath, KI_COMPONENT_PATH_LENGTH - 1,
                              "%s_%s", lacParentName, lacName);
            lacComponentPath[KI_COMPONENT_PATH_LENGTH - 1] = '\0';
            lpcComponentName = lacComponentPath;
        }

        // Bind the resolved component's MovieClipRef directly into mAptRef (the
        // X360 went via a stack temp then copied both handle words; that is exactly
        // a MovieClipRef write into mAptRef).
        lFile.FindComponent(&mAptRef, lpcComponentName);

        CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mpMovieClipInst");

        mAptRef.mpMovieClipInst->ResetTimeline();
    }
}
