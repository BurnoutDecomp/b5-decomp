// ============================================================================
// CgsVoice.cpp -- CgsSound::Playback::Slot control surface.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Slot::Attach        @ 0x826C77F8
//   Slot::Play          @ 0x82693428
//   Slot::Stop          @ 0x826934F8
//   Slot::HandleAttach  @ 0x826935D0
//   Slot::HandleDetach  @ 0x82693690
//   Slot::Release       @ 0x826C0810
//
// A Slot binds a Content to a Voice and drives its type-specific play/stop/attach
// behaviour through the pluggable ISlotImplementation. Every operation asserts the
// impl is present, and (when content is bound) that the handle owns a Content.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"

namespace CgsSound
{
namespace Playback
{

// @ 0x826C77F8. Bind ahContent iff its ContentClass matches this slot's authored
// mpContentClass (Entity mName + mTypeName word-for-word). The incoming
// Handle<Content> arrives by value; its ref is released on every return path by the
// Handle destructor.
bool Slot::Attach(Voice& arVoice, Handle<Content> ahContent)
{
    if (ahContent)                              // non-empty incoming handle
    {
        Content& lrContent = *ahContent;        // *a3 (non-null here)

        // content.mContentSpec -> ContentType -> ContentClass (the class the incoming
        // content belongs to).
        const ContentType&  lkrType  = lrContent.GetContentSpec().GetContentType();
        const ContentClass& lkrClass = lkrType.GetContentClass();

        // Inlined ContentClass::operator== : Entity mName + mTypeName match against
        // the class this slot was authored to accept.
        const u32* lpuThis  = reinterpret_cast<const u32*>(mpContentClass);
        const u32* lpuOther = reinterpret_cast<const u32*>(&lkrClass);
        bool lbMatch = (lpuThis[0] == lpuOther[0]) && (lpuThis[1] == lpuOther[1]);

        if (lbMatch)
        {
            Detach(arVoice);          // drop any content currently bound
            mhContent = ahContent;    // Handle::operator= into mhContent
            mhContent->OnAttach(arVoice, *this);
            return true;
        }
    }

    return false;
}

// @ 0x82693428.
bool Slot::Play(PlayerVoice& arVoice, u32 au32Param)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (!mu8Playing)
    {
        CGS_ASSERT(mhContent, "mpObject");
        if (mpImpl->DoPlay(*this, arVoice, *mhContent, au32Param))
            mu8Playing = 1;
    }
    return mu8Playing != 0;
}

// @ 0x826934F8.
bool Slot::Stop(PlayerVoice& arVoice)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (mu8Playing)
    {
        CGS_ASSERT(mhContent, "mpObject");
        if (mpImpl->DoStop(*this, arVoice, *mhContent))
            mu8Playing = 0;
    }
    return mu8Playing == 0;
}

// @ 0x826935D0.
void Slot::HandleAttach(Voice& arVoice)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (mu8Attach != 2)
    {
        CGS_ASSERT(mhContent, "mpObject");
        mpImpl->DoPostAttach(*this, arVoice, *mhContent);
        mu8Attach = 2;
    }
}

// @ 0x82693690.
void Slot::HandleDetach(Voice& arVoice)
{
    CGS_ASSERT(mpImpl, "mpImpl");

    if (mu8Attach == 2)
    {
        CGS_ASSERT(mhContent, "mpObject");
        mpImpl->DoPreDetach(*this, arVoice, *mhContent);
        mu8Attach = 0;
    }
}

// @ 0x826C0810. Detach current content, then hand this slot's impl to the Voice's
// slot disposer as a zero-padded 20-byte request. The disposer is reached off the
// Voice through raw offsets (voice.mFactory @+8 -> +0xC -> +0x30); those Voice header
// words are DEFERRED to the Voice keystone, so the walk stays offset-faithful.
void Slot::Release(Voice& arVoice)
{
    Detach(arVoice);

    CGS_ASSERT(mpImpl, "mpImpl");

    u8*   lpu8Voice = reinterpret_cast<u8*>(&arVoice);
    void* lpFactory = *reinterpret_cast<void**>(lpu8Voice + 8);
    void* lpModule  = *reinterpret_cast<void**>(reinterpret_cast<u8*>(lpFactory) + 0xC);
    ISlotDisposer* lpDisposer =
        *reinterpret_cast<ISlotDisposer**>(reinterpret_cast<u8*>(lpModule) + 0x30);

    // 20-byte request: word[0] = mpImpl, words[1..4] = 0.
    SlotDisposeRequest lRequest = {};
    lRequest.mpImpl = mpImpl;

    lpDisposer->DisposeSlot(&lRequest);   // disposer virtual at vtable byte 0x14.
}

} // namespace Playback
} // namespace CgsSound
