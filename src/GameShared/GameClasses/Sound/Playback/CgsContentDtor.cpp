// ============================================================================
// CgsContentDtor.cpp -- CgsSound::Playback::Content out-of-line destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82692A70
//   (CgsSound::Playback::Content::`vector deleting destructor')
//     bl  Content::~Content(this);        // base source dtor (out-of-line symbol)
//     if (a2 & 1) operator delete(this);  // (vector-)deleting tail (host delete)
//
// The compiler synthesises the deleting thunk from the out-of-line class dtor, so
// only the source-level ~Content() body is hand-written here. That body is the two
// load-count / ref-count asserts (moved out of the header so this TU emits the
// single Content::~Content symbol the deleting dtor and subclass dtors call).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "rw/rwcore_structs.h"

namespace CgsSound
{
namespace Playback
{
    void* Content::operator new(size_t luClientSize, Factory& arFactory,
                                const ContentSpec& /*akrContentSpec*/)
    {
        rw::BaseResourceDescriptors<5> lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size =
            static_cast<u32>(luClientSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (u32 luIndex = 1; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size = 0;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }

        rw::Resource lResource = arFactory.GetEnvironment().GetAllocator()->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), "Content");
        return lResource.m_baseResources[0];
    }

    void Content::operator delete(void* lpMemory, Factory& arFactory,
                                  const ContentSpec& /*akrContentSpec*/)
    {
        if (lpMemory)
            arFactory.GetEnvironment().GetAllocator()->Free(lpMemory);
    }

    Content::~Content()
    {
        // CgsContent.h:360. The checked byte is the low seven bits of
        // mu8ContentState (+0x1E), not the adjacent load-count halfword.
        CGS_ASSERT((mu8ContentState & 0x7Fu) == E_CONTENT_STATE_UNLOADED,
                   "Destroying content while it's still loaded. This is a Bad thing.");

        CGS_ASSERT(mu32RefCount == 0, "0 == mu32RefCount");
    }

    void Content::SetContentState(int liState)
    {
        CGS_ASSERT((liState & E_CONTENT_STATE_CHANGED) == 0,
                   "!(leContentState & E_CONTENT_STATE_CHANGED)");
        if ((mu8ContentState & 0x7Fu) != liState)
            mu8ContentState = static_cast<u8>((liState & 0x7F) |
                                               E_CONTENT_STATE_CHANGED);
    }

    void Content::OnAttach(Voice& arVoice, Slot& arSlot)
    {
        if (mu16LoadCount != 0 || DoLoad())
            ++mu16LoadCount;

        if (GetContentState() == E_CONTENT_STATE_LOADED)
            arSlot.HandleAttach(arVoice);
        else
            arSlot.PendingAttach();
    }
}
}
