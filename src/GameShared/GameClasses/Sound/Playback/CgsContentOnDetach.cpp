// ============================================================================
// CgsContentOnDetach.cpp -- CgsSound::Playback::Content::OnDetach @ 0x826A2458.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Drop a load reference when the content
// is detached from a voice slot. When this is the last load reference (count == 1)
// the unload is only committed if DoUnload() succeeds; for any other count the
// decrement is unconditional. Then hand off to the slot's detach latch.
//
// Emitted in its own TU (rather than the committed CgsObject.cpp, whose self-
// contained local `struct Factory` would collide with the coherent Content/Voice
// homes): it includes CgsVoice.h for the Voice/Slot collaborators.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"   // Voice, Slot

namespace CgsSound
{
namespace Playback
{
    void Content::OnDetach(Voice& arVoice, Slot& arSlot)
    {
        // ARTIST assertion at 0x826A2484.
        CGS_ASSERT_AT(mu16LoadCount,
            "mu16LoadCount",
            "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsObject.cpp", 104);

        const u16 lu16LoadCount = mu16LoadCount;
        if (lu16LoadCount != 1)
        {
            mu16LoadCount = static_cast<u16>(lu16LoadCount - 1);
        }
        else if (DoUnload())
        {
            --mu16LoadCount;
        }

        arSlot.HandleDetach(arVoice);   // X360: Slot::HandleDetach(lSlot, lVoice)
    }
}
}
