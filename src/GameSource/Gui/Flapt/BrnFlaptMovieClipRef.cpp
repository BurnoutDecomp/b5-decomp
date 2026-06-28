#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"

#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"  // BrnFlapt::MovieClipInstance
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"       // BrnFlapt::TextFieldRef
#include "GameShared/GameClasses/Containers/CgsHash.h"        // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

// ============================================================================
// BrnFlapt::MovieClipRef member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// This TU (GameSource/Gui/Flapt/BrnFlaptMovieClipRef.cpp) bodies the navigation,
// named-lookup, timeline-playback and trigger-callback accessors. Every one is a
// thin forwarder onto the referenced live MovieClipInstance (held in the Ref's
// first slot, mpMovieClipInst), guarded by the X360's null-pointer asserts. The
// string-keyed lookups hash the NUL-terminated name first (length excludes the
// terminator, matching the inlined strlen the X360 emits) and pass the hash on.
//
// sret returns are modeled, per this module's house style (see FileRef /
// FlaptFileInstance), as an explicit lpOutRef first parameter that is written by
// the instance method and returned. The X360-baked source file/line cites are
// discarded per project convention; CGS_ASSERT supplies __FILE__/__LINE__.
//
// NOTE: the transform-mutating accessors (SetPosition / SetPositionY / SetColour
// / SetColourScale / SetSizeScale / SetRotation / GetPosition) are NOT in this
// file -- they are blocked on the unreconstructed CgsGraphics::Im2dTransform and
// the rw Vector2/Vector4/VecFloat vector-math types (see the header note and the
// TU block reason).
// ============================================================================

namespace BrnFlapt
{

// Inline strlen the X360 folds into each lookup: count up to (not including) the
// NUL terminator, which is the byte count CgsHash::CalculateHash expects.
static int FlaptNameLength(const char* lpcName)
{
    int liLength = 0;
    while (lpcName[liLength] != '\0')
    {
        ++liLength;
    }
    return liLength;
}

// ---- FindChildMovieClip @ 0x8246C740 -------------------------------------
// Assert the handle and the name, hash the name, then forward to the instance,
// which writes the matched child's MovieClipRef into lpOutRef and (via this Ref)
// returns it.
MovieClipRef* MovieClipRef::FindChildMovieClip(MovieClipRef* lpOutRef,
                                               const char* lpcName) const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(lpcName != 0, "lpcName");

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName),
                                                       FlaptNameLength(lpcName));

    mpMovieClipInst->FindChildMovieClip(luHash, lpOutRef, lpcName);
    return lpOutRef;
}

// ---- FindChildMovieClipOnFrame @ 0x8246C8B0 ------------------------------
// As FindChildMovieClip, but the instance restricts the search to the children
// placed on its current frame.
MovieClipRef* MovieClipRef::FindChildMovieClipOnFrame(MovieClipRef* lpOutRef,
                                                      const char* lpcName) const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(lpcName != 0, "lpcName");

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName),
                                                       FlaptNameLength(lpcName));

    mpMovieClipInst->FindChildMovieClipOnFrame(luHash, lpOutRef, lpcName);
    return lpOutRef;
}

// ---- FindChildTextField @ 0x8246C7F8 -------------------------------------
// Assert the handle and the name, hash the name, then forward to the instance,
// which writes the named child text field's TextFieldRef into lpOutRef.
TextFieldRef* MovieClipRef::FindChildTextField(TextFieldRef* lpOutRef,
                                               const char* lpcName) const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(lpcName != 0, "lpcName");

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName),
                                                       FlaptNameLength(lpcName));

    mpMovieClipInst->FindChildTextField(luHash, lpOutRef, lpcName);
    return lpOutRef;
}

// ---- TryFindChildComponentRecursively @ 0x8246C968 -----------------------
// Assert the handle, the out buffer and the name (asm order: mpMovieClipInst,
// lpOutMovieClipRef, lpcName), hash the name, then forward; the instance
// performs the recursive descent and returns whether it found a match.
bool MovieClipRef::TryFindChildComponentRecursively(const char* lpcName,
                                                    MovieClipRef* lpOutMovieClipRef) const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(lpOutMovieClipRef != 0, "lpOutMovieClipRef");
    CGS_ASSERT(lpcName != 0, "lpcName");

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcName),
                                                       FlaptNameLength(lpcName));

    return mpMovieClipInst->TryFindChildComponentRecursively(luHash,
                                                             lpOutMovieClipRef,
                                                             lpcName);
}

// ---- GetParent @ 0x8246CA40 ----------------------------------------------
// Forward to the instance, which writes the parent clip's MovieClipRef into
// lpOutRef and (via this Ref) returns it. The X360 omits the handle assert here.
MovieClipRef* MovieClipRef::GetParent(MovieClipRef* lpOutRef) const
{
    mpMovieClipInst->GetParent(lpOutRef);
    return lpOutRef;
}

// ---- GotoAndPlayLabel @ 0x8246F388 ---------------------------------------
// Assert the handle, then forward the pre-hashed label and its debug name to the
// instance.
void MovieClipRef::GotoAndPlayLabel(u32 luLabelHash, const char* lpcDEBUGName) const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");

    mpMovieClipInst->GotoAndPlayLabel(luLabelHash, lpcDEBUGName);
}

// ---- GotoAndStopLabel @ 0x8246F498 ---------------------------------------
// Assert the handle and the label, hash the label, then forward to the instance
// (which receives both the hash and the original string).
void MovieClipRef::GotoAndStopLabel(const char* lpcLabel) const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(lpcLabel != 0, "lpcLabel");

    u32 luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcLabel),
                                                       FlaptNameLength(lpcLabel));

    mpMovieClipInst->GotoAndStopLabel(luHash, lpcLabel);
}

// ---- SetFrameTriggerCallback @ 0x8246CAB8 --------------------------------
// Assert the handle and the callback, then install it on the instance. The
// callback is declared opaquely (void*) in the header to keep the instance
// type out of the Ref's declaration home; bridge it to the typed signature here.
void MovieClipRef::SetFrameTriggerCallback(void* lpCallback, void* lpUserData) const
{
    CGS_ASSERT(mpMovieClipInst != 0, "mpMovieClipInst");
    CGS_ASSERT(lpCallback != 0, "lpCallback");

    mpMovieClipInst->SetFrameTriggerCallback(
        reinterpret_cast<MovieClipInstance::FrameTriggerCallback>(lpCallback),
        lpUserData);
}

// ---- GetTriggerParameters @ 0x8246CAB0 -----------------------------------
// Tail-call forward to the instance; the X360 emits no assert here.
const TriggerParameters* MovieClipRef::GetTriggerParameters()
{
    return mpMovieClipInst->GetTriggerParameters();
}

}
