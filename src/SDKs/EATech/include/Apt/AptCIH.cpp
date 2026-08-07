// ===========================================================================
// EATech Apt -- AptCIH core.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x824C7C / dtor 0x804E68 / RegisterReferences 0x7E9C78 /
//   DestroyGCPointers 0x80553C / PreDestroy 0x7ECBAC + the state/flag/link
//   accessors (0x7DF0xx..0x7FB648).
//
// The packed flag words use the console rotate+mask idiom; reconstructed here as
// the equivalent clear-range/set bit-ops on the whole word. Scope is the node
// core; the behavioural methods are follow-ons.
//
// AptCIH::`scalar deleting destructor' @0x82AE7340 -- DROPPED (compiler-generated
// thunk, like every sibling Apt class's deleting destructor). The X360 body is the
// canonical MSVC pattern { ~AptCIH(); if (flags & 1) AptCIH::operator delete(this,
// 0x28); return this; } -- where 0x28 == sizeof(AptCIH) (the GC value pool block).
// Both halves it composes already exist: the virtual ~AptCIH() (below) and
// AptCIH::operator delete (AptCIHBehaviour.cpp). The compiler re-synthesises this
// thunk from the virtual destructor + operator delete, so it is not hand-written.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"  // movie-clip play-head (mnGotoFrame/mnClipActionFlags/mnLastActionFrame/mDisplayList)
#include "SDKs/EATech/include/Apt/AptRenderItem.h"               // mpRenderItem->mpCharacter (the embedded AptMovie)
#include "SDKs/EATech/include/Apt/AptMovie.h"                    // doFrameControls / queueFrameActions / DoTemporaryFrameControls + AptValue_toInteger

#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // child-list recursion (AptDisplayList::tick)
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"         // AsState()->RegisterReferences (GC mark of the child list)
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"        // AptCurrentRenderTreeManager + Update_ItemRemoved (DestroyGCPointers)
#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"        // scratch state for the multi-frame skip path
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"        // gAptActionInterpreter operand stack (_gotoAndX)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"          // AptString::Create (the `_name` property getter)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"          // AptGetAnimationAtLevel (GetRootAnimation's CIHNone path)
#include "SDKs/EATech/include/Apt/AptDefine.h"     // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <new>   // placement new (the scratch AptPseudoDisplayList over pool memory)
#include <cstring>   // std::strcmp (the `_name` property-name compare)

// Shared AS/runtime singletons (storage HOMED in AptGlobals.cpp; declared per-TU
// by their console-name equivalents, the established Apt convention):
//   gAptActionInterpreter (console &dword_8324E760) -- the AS interpreter context
//     (its operand stack is mpStack[mnStackTop]); declared in AptActionInterpreter.h.
//   gnAptActionFrameId    (console dword_8324E514)  -- the running AS-action frame id
//     queueClipEvents stamps onto each queued clip event.
//   gbAptRecorderGate     (console byte_82F733F7)   -- the input-recorder/replay gate:
//     when shut (deterministic replay) the play-head must NOT auto-advance.
//   gpUndefinedValue      (console off_8324D814)    -- the shared AS `undefined` value.
extern int gnAptActionFrameId;       // console dword_8324E514
extern unsigned char gbAptRecorderGate;   // console byte_82F733F7
extern AptValue* gpUndefinedValue;   // console off_8324D814

// The AS interpreter singleton (console &dword_8324E760; storage HOMED in
// AptGlobals.cpp): the operand stack _gotoAndX reads the goto target from.
// Declared per-TU like its siblings (AptCIHNativeFunctionHelper.cpp / AptMovie.cpp).
extern AptActionInterpreter gAptActionInterpreter;   // &dword_8324E760

// AptCIH::queueClipEvents (canonical X360/PS3 sig (int, unsigned int, int); a3 the
// UNSIGNED frame id) is HOMED in AptCIHBehaviour.cpp and called directly as a member
// (AptCIH.h is included above); the free-function shim is retired.

// AptDisplayList::mergeState @0x82B0B438 (HOMED in AptDisplayList.cpp) is called
// directly as a member (AptDisplayList.h is included above); the free-function
// forwarder is retired.

// ---------------------------------------------------------------------------
// AptGetClipMovie -- the AptMovie timeline embedded inside a sprite/animation
// AptCharacter record. The X360 reaches it as `mpRenderItem->mpCharacter + 0x10`;
// the same reinterpret AptCharacterAnimation.cpp / AptCIHMembers.cpp use.
// Centralised here so the three play-head methods name it once.
// FLAG PC-platform leaf: serialised .apt blob offset access -- the character table
// is native-8 "1:7:8" FILE data (relocated live by Fixup), not a modelled C++
// member; the widened header lands the embedded body at char+0x20
// (KU_AptEmbeddedMovieOff, AptCIH.h -- console char+0x10). We ship only the 8-byte
// format, so this is the single native-64 offset.
static AptMovie* AptGetClipMovie(const AptCharacterSpriteInstBase* pInst)
{
    AptCharacter* pCharacter = pInst->mpRenderItem->mpCharacter;
    return reinterpret_cast<AptMovie*>(reinterpret_cast<char*>(pCharacter) + KU_AptEmbeddedMovieOff);
}

// The pre-destroy notify shim (HOMED in AptCIHBehaviour.cpp; a PC host-callback
// boundary -- dispatches the host-installable gpAptCIHPreDestroyHook slot, console
// dword_8324E8A0, faithfully null until a host installs it).
void AptCIH_PreDestroyHook(AptCIH* pCIH);

// The zombie-vector reap (XB1 sub_140830A40; X360 name AptUpdateZombieVector) --
// HOMED in AptGC.cpp (the real gpAptZombieVector reap; AptInit builds the vector).
extern void* AptUpdateZombieVector(char bClear);

// ctor @0x824C7C
AptCIH::AptCIH(AptCharacter* pCharacter, AptCIH* pParent)
    : AptValueGC(AptVFT_CharacterInstHandle, CO_CIH)
    // mInstanceName default-constructs to the shared empty string.
{
    // x64 ctor sub_140825520: mFlagsA masked (`and [rbx+18h],0FE0001D0h` -- the
    // deterministic bits then applied), mFlagsB createdOnFrame inited to -1 via
    // `or [rbx+1Ch],3FFFh` (LOW 14 bits). Zero-initialised here, then the
    // deterministic bits applied.
    mFlagsA = 0;
    mFlagsB = 0x3FFFu;

    mpDisplayListParent = pParent;
    if (pParent)
        pParent->AddRef();

    setGCRoot(1);
    SetAllowDelayedDeletion(false);

    mpDisplayListPrevious = nullptr;
    mpDisplayListNext     = nullptr;
    mpAssetString         = nullptr;

    mpCharacterInst = AptCharacterInst::CreateCharacterInst(pCharacter);

    // Finish exactly as the console ctor (@0x82B00638, tail 0x82B006F0..0x82B00778):
    // dirty the node keyed on the character type.
    //   SetDirtyState(bDirty, bDirty)  -- bDirty when the character is a sprite(5) /
    //     button(4) / morph(8) / animation(9) / custom-control(16), or is null. Both
    //     the dirty and propagate args carry the SAME value (asm sets r4==r5). This
    //     sets the tick-dirty bit 6 and pushes it up the display-list parent chain, so
    //     a freshly-placed sprite/animation child is BORN dirty and ticks on its own
    //     placement frame (SetDirtyState's internal shape/text/None gate still applies).
    //   SetGeneralizedProcessDirtyState(bGenDirty) -- bGenDirty when the character is a
    //     text(2) / button(4) / sprite(5); sets bit 7 (self) + propagates bit 8 up.
    // This is the real mechanism that composes nested content -- NOT a later render-tree
    // propagation pass. (Replaces the deferred hard-set stand-in, which both hard-set
    // the self bit unconditionally and omitted the tick-dirty bit fresh children need.)
    const int32_t nCharType = pCharacter ? pCharacter->mnType : -1;
    const bool bDirty = (pCharacter == nullptr) ||
        nCharType == 5 || nCharType == 4 || nCharType == 8 ||
        nCharType == 9 || nCharType == 16;
    SetDirtyState(bDirty, bDirty);

    const bool bGenDirty = (pCharacter != nullptr) &&
        (nCharType == 2 || nCharType == 4 || nCharType == 5);
    SetGeneralizedProcessDirtyState(bGenDirty);
}

// dtor @0x804E68 -- free the lazily-allocated asset string; mInstanceName is
// released by its member destructor. (Parent + char inst are released by
// DestroyGCPointers, which the GC runs first.)
AptCIH::~AptCIH()
{
    if (mpAssetString)
    {
        gpNonGCPoolManager->Deallocate(mpAssetString, 4);
        mpAssetString = nullptr;
    }
}

// RegisterReferences @0x7E9C78 -- GC mark: the parent link + the char inst's
// property hash.
void AptCIH::RegisterReferences()
{
    if (!AptValue::sReferenceRegistrationCb)
        return;   // GC registration callback not installed (the same gate every sibling RegisterReferences body uses)

    AptNativeHash* pCharProps = nullptr;
    if (mpCharacterInst)
        pCharProps = mpCharacterInst->mpProperties;

    if (mpDisplayListParent)
        AptValue::sReferenceRegistrationCb(this, &mpDisplayListParent, "Parent", 1);
    if (pCharProps)
        pCharProps->RegisterReferences(this);

    // Sprite/movie nodes (char type 5/9) also GC-mark their child display-list
    // state (AptDisplayListState::RegisterReferences @0x7E68FC, HOMED in
    // AptDisplayListState.cpp) -- the child links keep placed children reachable.
    if (mpCharacterInst)
    {
        const uint32_t nType = mpCharacterInst->GetTypeTag();
        if (nType == 5u || nType == 9u)
        {
            AptDisplayListState* pChildren =
                static_cast<AptCharacterSpriteInstBase*>(mpCharacterInst)->mDisplayList.AsState();
            if (pChildren)
                pChildren->RegisterReferences(this);
        }
    }
}

// DestroyGCPointers @0x80553C -- release the parent + tear down the char inst.
void AptCIH::DestroyGCPointers()
{
    AptCharacterInst* pCI = mpCharacterInst;
    AptCIH* pParent = mpDisplayListParent;
    mpCharacterInst = nullptr;

    if (pParent)
        pParent->Release();
    mpDisplayListParent = nullptr;

    if (pCI)
    {
        // The console first unhooks the inst's render item from the render-tree
        // manager (Update_ItemRemoved) and destroys the inst's property hash --
        // the same retire sequence ClearCIH's immediate-free path runs
        // (AptCIHBehaviour.cpp). The char inst's own destructor then releases
        // its render item.
        if (pCI->mpRenderItem != nullptr)
        {
            if (AptRenderTreeManager* pManager = AptCurrentRenderTreeManager())
                pManager->Update_ItemRemoved(pCI->GetRenderItemWritable(), gnCurrUpdateTick);
        }
        if (pCI->mpProperties != nullptr)
        {
            pCI->mpProperties->DestroyGCPointers();
            pCI->mpProperties->~AptNativeHash();
            gpNonGCPoolManager->Deallocate(pCI->mpProperties, sizeof(AptNativeHash));
            pCI->mpProperties = nullptr;
        }
        pCI->~AptCharacterInst();
        gpNonGCPoolManager->Deallocate(pCI, sizeof(AptCharacterInst));
    }
}

// PreDestroy @0x7ECBAC
void AptCIH::PreDestroy()
{
    AptCIH_PreDestroyHook(this);
}

// ---- delegated visual read ------------------------------------------------
int16_t AptCIH::GetDepth() const { return mpCharacterInst->GetDepth(); }

// ---- packed state / flags (mFlagsA) ---------------------------------------
// x64 GetCIHState @0x140838580: `shr eax,1; and eax,3` -- bits 1-2.
uint32_t AptCIH::GetCIHState() const { return (mFlagsA >> 1) & 3u; }
void AptCIH::SetCIHState(uint32_t eState)
{
    // x64 SetCIHState @0x140840FD0: `and [rcx+18h],0FFFFFFF9h; add edx,edx`.
    mFlagsA = (mFlagsA & ~0x6u) | ((eState << 1) & 0x6u);
}

// x64 GetZombieCount @0x140839E80: `shl eax,7; sar eax,10h` -- SIGNED 16-bit
// field in bits 9-24 (mask 0x01FFFE00).
int16_t AptCIH::GetZombieCount() const { return static_cast<int16_t>(static_cast<uint16_t>(mFlagsA >> 9)); }
void AptCIH::IncZombieCount()
{
    // x64 IncZombieCount @0x14083A630: mask 0x1FFFE00, +0x200.
    const int16_t n = static_cast<int16_t>(GetZombieCount() + 1);
    mFlagsA = (mFlagsA & ~0x01FFFE00u) | ((static_cast<uint32_t>(static_cast<uint16_t>(n)) << 9) & 0x01FFFE00u);
}
void AptCIH::DecZombieCount()
{
    const int16_t n = static_cast<int16_t>(GetZombieCount() - 1);
    mFlagsA = (mFlagsA & ~0x01FFFE00u) | ((static_cast<uint32_t>(static_cast<uint16_t>(n)) << 9) & 0x01FFFE00u);
    // XB1 sub_140835B50 (the arbiter's DecZombieCount): when the count reaches
    // zero, reap the dead zombies immediately -- AptUpdateZombieVector(0)
    // (HOMED in AptGC.cpp; AptInit builds gpAptZombieVector).
    if ((mFlagsA & 0x01FFFE00u) == 0u)
        AptUpdateZombieVector(0);
}

// x64 IsInCtor @0x14083B140 / SetInCtor @0x1408417C0: bit 4 (mask 0x10).
bool AptCIH::IsInCtor() const { return (mFlagsA & 0x10u) != 0; }
void AptCIH::SetInCtor(uint32_t b)
{
    mFlagsA = (mFlagsA & ~0x10u) | (b ? 0x10u : 0u);
}

// ---- packed state / flags (mFlagsB) ---------------------------------------
// x64 GetCreatedOnFrame @0x1408387E0: `shl eax,12h; sar eax,12h` -- SIGNED
// 14-bit field in the LOW bits (mask 0x3FFF); SetCreatedOnFrame @0x140841220
// clears with `and [rcx+1Ch],0FFFFC000h`.
int  AptCIH::GetCreatedOnFrame() const { return (static_cast<int32_t>(mFlagsB << 18)) >> 18; }
void AptCIH::SetCreatedOnFrame(int nFrame)
{
    mFlagsB = (mFlagsB & ~0x3FFFu) | (static_cast<uint32_t>(nFrame) & 0x3FFFu);
}

// ---------------------------------------------------------------------------
// Character-type predicates -- each reads the character instance's type tag
// (mTypeFlags bits 26..31). The X360 bodies dereference mpCharacterInst directly
// (no null guard -- they are only called on placed nodes that own an instance).
// IsNone instead tests this value's own AptValue vtable index.
// ---------------------------------------------------------------------------
bool AptCIH::IsShapeInst() const         { return mpCharacterInst->GetTypeTag() == 1; }   // @0x82AD5A30
bool AptCIH::IsDynamicTextInst() const   { return mpCharacterInst->GetTypeTag() == 2; }   // @0x82AD5A50
bool AptCIH::IsButtonInst() const        { return mpCharacterInst->GetTypeTag() == 4; }   // @0x82AD5A10
bool AptCIH::IsMorphInst() const         { return mpCharacterInst->GetTypeTag() == 8; }   // @0x82AD5A90
bool AptCIH::IsAnimationInst() const     { return mpCharacterInst->GetTypeTag() == 9; }   // @0x82AD5AB0
bool AptCIH::IsLevelInst() const         { return mpCharacterInst->GetTypeTag() == 15; }  // @0x82AD5AD0
bool AptCIH::IsCustomControlInst() const { return mpCharacterInst->GetTypeTag() == 16; }  // @0x82AD5B08

// A sprite inst is a movie-clip (5) or its custom-control variant (16). @0x82AD59B0
bool AptCIH::IsSpriteInst() const
{
    const uint32_t nType = mpCharacterInst->GetTypeTag();
    return nType == 5 || nType == 16;
}

// A sprite-base inst is a movie-clip (5) or an imported animation (9). @0x82AD59E0
bool AptCIH::IsSpriteInstBase() const
{
    const uint32_t nType = mpCharacterInst->GetTypeTag();
    return nType == 5 || nType == 9;
}

// IsNone @0x82AD5AF0 -- this value is the empty placeholder (AptCIHNone, AptValue
// vtable index 37), rather than testing a character instance.
bool AptCIH::IsNone() const { return static_cast<uint32_t>(getVtblIndex()) == 37u; }

// ---------------------------------------------------------------------------
// Delegated mask / property reads (through the character instance + its render
// item). GetNativeHash null-guards the char inst (as the X360 does); IsMask/HasMask
// dereference it directly (only valid on placed nodes that own an instance).
// ---------------------------------------------------------------------------
AptNativeHash* AptCIH::GetNativeHash() const   // @0x82AD5B28
{
    return mpCharacterInst ? mpCharacterInst->mpProperties : nullptr;
}

// (objectMemberLookup @0x82B0DF70 / objectMemberSet @0x82B09E58 -- the built-in
// member recognizers -- are homed in AptCIHMembers.cpp.)
bool AptCIH::IsMask() const  { return mpCharacterInst->GetRenderItem()->GetIsMask(); }   // @0x82AD5BA0
bool AptCIH::HasMask() const { return mpCharacterInst->GetRenderItem()->GetHasMask(); }  // @0x82AD5BB8

// ---------------------------------------------------------------------------
// GetRootAnimation @PS3 0x820F3C -- the nearest enclosing ANIMATION node. The empty
// CIHNone placeholder (tag 0x25) resolves to the lazily-created level-0 root; any
// other node walks its display-list parent chain ([7] mpDisplayListParent) until the
// character-inst type tag (x64: mTypeFlags & 0x3F) is 9 (animation) or 15.
// ---------------------------------------------------------------------------
AptCIH* AptCIH::GetRootAnimation()
{
    if (getVtblIndex() == AptVFT_CIHNone)          // (this[1] & 0x7F) == 0x25
        return AptGetAnimationAtLevel(0);

    AptCIH* pNode = this;
    uint32_t uTag = pNode->mpCharacterInst->GetTypeTag();   // *(this[8]+8) >> 26
    while (uTag != 9u && uTag != 15u)
    {
        pNode = pNode->mpDisplayListParent;                 // this[7]
        uTag  = pNode->mpCharacterInst->GetTypeTag();
    }
    return pNode;
}

// ---------------------------------------------------------------------------
// SetDirtyState @0x82AD76B8 -- set/clear the per-node dirty bit (x64 mFlagsA bit 6,
// mask 0x40 -- x64 GetDirtyState @0x140838870 `shr eax,6`, XB1 ctor 0x140825520
// propagates `|= 0x40`; the console `oris 0x200` = 0x02000000 was the same field's
// X360 big-endian position). Clearing, or a non-renderable character type (shape 1 /
// dynamic-text 2 / static-text 10) or the empty AptCIHNone placeholder, leaves it
// clear. When a renderable node is dirtied with bPropagate, the dirty bit is pushed
// up the display-list parent chain, stopping at the first already-dirty ancestor.
// ---------------------------------------------------------------------------
void AptCIH::SetDirtyState(bool bDirty, bool bPropagate)
{
    if (!bDirty)
    {
        mFlagsA &= ~0x40u;
        return;
    }

    const uint32_t nType = mpCharacterInst->GetTypeTag();
    if (nType == 1 || nType == 2 || nType == 10 || IsNone())
    {
        mFlagsA &= ~0x40u;
        return;
    }

    mFlagsA |= 0x40u;
    if (bPropagate)
    {
        for (AptCIH* pAncestor = mpDisplayListParent; pAncestor; )
        {
            if ((pAncestor->mFlagsA & 0x40u) != 0)
                break;
            AptCIH* pNext = pAncestor->mpDisplayListParent;
            pAncestor->mFlagsA |= 0x40u;
            pAncestor = pNext;
        }
    }
}

// ===========================================================================
// Movie-clip play-head -- the per-frame timeline driver (tick / jumpToFrame /
// _gotoAndX). DECOMPILED from the X360 ARTIST.XEX, verified against the asm.
//
// The play-head state lives in the node's AptCharacterSpriteInstBase (the sprite/
// movie-clip + animation char instances): the asm reaches it as mpCharacterInst's
// dwords [4]/[5]/[8] and its embedded child list [7]. The named members are:
//   mnGotoFrame       (+0x10, dword[4]) -- the LIVE current play-head frame (the
//                     header's earlier "-1 pending goto sentinel" label was refined:
//                     these three functions PROVE +0x10 is the active current-frame
//                     counter the timeline driver reads/advances).
//   mnClipActionFlags (x64 +0x24) -- x64 bit layout: bit 25 (0x2000000, bIsPlaying) =
//                     "needs a frame action this tick", bit 24 (0x1000000, bJustLoaded)
//                     = "auto-play / freshly placed". (The LOW 24 bits are the AS
//                     clip-event mask, untouched here. X360 kept these reversed:
//                     state bits 6/7, mask high-24.)
//   mnLastActionFrame (+0x20, dword[8]) -- the frame id stamped for queueFrameActions.
//   mDisplayList      (+0x1C, dword[7]) -- the clip's child display list.
// ===========================================================================

// jumpToFrame @0x82B0BD50 -- seek the clip's play-head to nFrame.
int AptCIH::jumpToFrame(int nFrame)
{
    // No-op on the empty AptCIHNone placeholder (vtbl index 37).
    if (getVtblIndex() == AptVFT_CIHNone)
        return 0;

    AptCharacterSpriteInstBase* pInst =
        static_cast<AptCharacterSpriteInstBase*>(mpCharacterInst);
    if (nFrame < 0)
        return 0;

    AptMovie* pMovie = AptGetClipMovie(pInst);
    if (nFrame >= pMovie->mnFrameCount)
        return 0;

    int nResult = 0;
    const int nCurrent = pInst->mnGotoFrame;
    if (nFrame != nCurrent)
    {
        if (nFrame == nCurrent + 1)
        {
            // Single step forward: just replay the one frame's commands.
            pInst->mnGotoFrame = nFrame;
            pMovie->doFrameControls(&pInst->mDisplayList, this, nFrame);
        }
        else
        {
            // Arbitrary jump: rebuild the intervening display-list state into a
            // scratch pseudo list, replaying every skipped frame, then merge it.
            //
            // LIVE (2026-07-02, after five staged rounds): the replay pipeline is
            // native-8 through six fixed layers (the AptPlaceObjectInfo_t re-lay,
            // the snapshot ctor, mergeState's source head, the node/scratch pool
            // sizes, AddToDisplayList/ReplaceDisplyListItem's record id + embedded
            // -anim chain, the dispatcher's depth word), and the removal chain the
            // merge exercises was made faithful to the arbiter by the 2026-07-02
            // deletion-chain audit: AddToDelayReleaseList opens with the vtbl[0]
            // AddRef PIN (was mislabeled PreDestroy), removeItem does NOT release
            // (the fabricated "balance" drop was removed), ClearCIH's slot-5 tail
            // is SetHasClass(0) (was a fabricated Release), and the Release zero-
            // path teardown is ForceDelete (X360 vtbl+0x2C / XB1 +88). Boot-
            // verified: removals free at ATDRL's final Release only, 0 asserts.
            void* pProperties = pInst->mpProperties;   // dword[3] (the AS property hash)

            void* pScratchMem = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoDisplayList));   // [c: 8]
            AptPseudoDisplayList* pScratch = nullptr;
            if (pScratchMem)
                pScratch = new (pScratchMem) AptPseudoDisplayList(this);

            const char bForward = (pInst->mnGotoFrame < nFrame) ? 1 : 0;

            // A freshly-placed clip (bJustLoaded, x64 bit 24) restarts from frame 0.
            if ((pInst->mnClipActionFlags & 0x1000000u) != 0u)
                pInst->mnGotoFrame = 0;
            // Never replay forward FROM a frame already at/after the target.
            if (pInst->mnGotoFrame >= nFrame)
                pInst->mnGotoFrame = 0;

            // Replay every frame from the current play-head up to and including
            // nFrame into the scratch list (stopping early if it would run past the
            // clip's end). The play-head (mnGotoFrame) is the loop variable.
            while (pInst->mnGotoFrame <= nFrame)
            {
                if (pInst->mnGotoFrame >= AptGetClipMovie(pInst)->mnFrameCount)
                    break;
                // The X360 call site (sub_82B0BE60) only re-fills r3 (the AptMovie)
                // and r4 (the scratch list); r5/r6 still hold the current replay
                // frame + trailing args from the loop, so they are passed here
                // explicitly for the AptMovie::DoTemporaryFrameControls signature.
                pMovie->DoTemporaryFrameControls(pScratch, pInst->mnGotoFrame, 0, nullptr);
                ++pInst->mnGotoFrame;
            }

            pInst->mnGotoFrame = nFrame;
            pInst->mDisplayList.mergeState(reinterpret_cast<void**>(pScratch),
                                           static_cast<AptNativeHash*>(pProperties), bForward);

            if (pScratch)
            {
                pScratch->~AptPseudoDisplayList();
                gpAptPseudoDataPool->Deallocate(pScratch, sizeof(AptPseudoDisplayList));   // [c: 8]
            }
        }

        pInst->mnLastActionFrame = pInst->mnGotoFrame;
        // x64 narrowing (resolved): the X360 returns r3 -- the queueFrameActions
        // result pointer -- as the int result the callers ignore. Carried as a
        // non-zero "ran" marker so no 8->4-byte pointer truncation exists on x64.
        nResult = pMovie->queueFrameActions(this, pInst->mnGotoFrame) != nullptr;
    }
    return nResult;
}

// tick @0x82B0BED8 -- advance the movie-clip node one frame.
//
// FLAG PC-platform leaf (host tick gating): the console tick dereferences the char
// inst (a1+32), its render item (v6[1]), the render item's character and the embedded
// clip movie (char + embed) WITHOUT null guards -- everything is always live in the
// shipped game. On our partial bring-up the render-tree / AS scope is not fully stood up
// yet, so this faithful body is NOT ticked at boot: the host driver holds it off
// (BrnGuiAptRuntime UpdateRuntime: lbTickReady=false) until the converter delivers
// a uniformly-64-bit bundle. The body below is the single faithful X360 decompile.

int AptCIH::tick()
{
    // Only a dirtied node ticks (the dirty bit, x64 mFlagsA bit 6 -- XB1 tick
    // sub_14085D620 `(*(a1+24) & 0x40) == 0`; the console 0x02000000 was the same
    // field's X360 big-endian position).
    if ((mFlagsA & 0x40u) == 0)
        return 0;

    AptCharacterSpriteInstBase* pInst =
        static_cast<AptCharacterSpriteInstBase*>(mpCharacterInst);   // v6 = *(a1+32)


    // Only sprite(5)/animation(9) clips have a play-head (v6[2] >> 26 == 5 or 9).
    const uint32_t nType = pInst->GetTypeTag();
    if (nType != 5 && nType != 9)
        return (mFlagsA >> 6) & 1u;   // LABEL_44 (dirty bit; x64 bit 6)

    const uint32_t nFlags = pInst->mnClipActionFlags;   // v11 = v6[5]
    pInst->mnLastActionFrame = 0;                        // v6[8] = 0

    const bool bNeedsAction = (nFlags & 0x2000000u) != 0;   // bIsPlaying (x64 bit 25; X360 bit6)
    const bool bFreshPlaced = (nFlags & 0x1000000u) != 0;   // bJustLoaded (x64 bit 24; X360 bit7)

    AptMovie* const pClipMovie = AptGetClipMovie(pInst);   // *(v6[1]+4) + embed

    // ---- (1) auto-advance the play-head -----------------------------------
    // The play-head steps this frame when the clip needs an action (v12), or it is
    // auto-playing (bit7) with an open recorder gate. A non-fresh stopped clip and a
    // fresh clip in shut-gate deterministic replay both hold their frame (LABEL_18/19).
    const bool bStep = bNeedsAction || (bFreshPlaced && gbAptRecorderGate != 0);
    if (bStep)
    {
        AptRenderItem* pRenderItem = pInst->mpRenderItem;   // v14 = v6[1]
        // A stopped render item (mFlags bit 0x08000000; console *(v14+24) bit27) holds
        // frame 0; else step to the next frame.
        if ((pRenderItem->mFlags & 0x10u) != 0)   // x64 bit 4 (X360 bit27)
            pInst->mnGotoFrame = 0;
        else
            ++pInst->mnGotoFrame;

        const int nFrame = pInst->mnGotoFrame;             // v15 = v6[4]
        const int nFrameCount = pClipMovie->mnFrameCount;
        if (nFrame == 1 && nFrameCount == 1)
        {
            // A single-frame clip never plays past frame 0; skip both doFrameControls
            // and queueFrameActions, jumping to the enterFrame stage (LABEL_27).
            pInst->mnGotoFrame = 0;
            goto label_27;
        }
        if (nFrame == nFrameCount)
        {
            // Wrapped past the end: loop back to the start, then skip to the enterFrame
            // stage (LABEL_27) -- NOT through queueFrameActions.
            // FLAG (deliberate divergence, 2nd attempt 2026-07-04): the faithful console/
            // XB1 wrap is jumpToFrame(0) (XB1 tick sub_14085D620 `call sub_1408501F0(0)`,
            // replay + mergeState + frame-0 action re-queue). RESTORING it against the
            // REAL native-8 drive bundles kills the process reproducibly a few ticks into
            // composition (silent death mid-mkitem, no WER record -- 2x reproduced), so
            // the wrap-replay chain still has an un-diagnosed defect on real-bundle data
            // (suspects: the pseudo-list replay over clips whose imports are unresolved
            // stubs, or the delay-release chain re-entered from mergeState during tick).
            // Until that is pinned, the plain play-head reset stays: the next tick's
            // frame-0 doFrameControls re-composes the loop content (same on-screen
            // result for a LOOP; the replay-merge only matters for a mid-timeline SEEK).
            pInst->mnGotoFrame = 0;
            goto label_27;
        }
        // Normal frame: fall through to the shared doFrameControls (LABEL_18/19).
    }

    // LABEL_18/19: run this frame's place/remove commands when the clip has a pending
    // action (v12), or it is auto-playing (bit7) with an open recorder gate.
    if (bNeedsAction || (bFreshPlaced && gbAptRecorderGate != 0))
    {
        pClipMovie->doFrameControls(&pInst->mDisplayList, this, pInst->mnGotoFrame);
    }

    // ---- (2) queue this frame's actions -----------------------------------
    // Normal fall-through ONLY (queueFrameActions @0x82B0C034). The single-frame and
    // end-wrap paths jump PAST this block to the enterFrame stage (label_27 below).
    {
        const uint32_t nFlags2 = pInst->mnClipActionFlags;   // v16 = v6[5]
        if (((nFlags2 & 0x2000000u) != 0) || (((nFlags2 & 0x1000000u) != 0) && gbAptRecorderGate != 0))
        {
            // Negate the stamped frame while the actions are queued (the X360's
            // running-frame marker), then restore it.
            pInst->mnLastActionFrame = -pInst->mnGotoFrame;   // v6[8] = -v6[4]
            pClipMovie->queueFrameActions(this, pInst->mnGotoFrame);
            pInst->mnLastActionFrame = pInst->mnGotoFrame;    // v6[8] = v6[4]
        }
    }

label_27:
    // ---- (3)+(4) clip events (enterFrame / construct-load) ----------------
    // (3) enterFrame clip event: a non-fresh clip (or the 0x24 custom-control family)
    // with an onEnterFrame handler (clip-event mask bit 0x200 or a __proto__ event
    // member) queues its enterFrame handlers.
    if ((((pInst->mnClipActionFlags & 0x1000000u) == 0) ||   // not freshly placed (bJustLoaded)
         ((pInst->mTypeFlags & 0x3Fu) == 9u)) &&   // animation; x64 low-6-bit tag
        (((pInst->mnClipActionFlags & 0x2u) != 0) || HasEventMember(2)))   // enterFrame; x64 low-24 mask
    {
        queueClipEvents(2, gnAptActionFrameId, 1);
    }
    // (4) construct/load clip event on first placement; then clear the freshly-placed bit.
    if ((pInst->mnClipActionFlags & 0x1000000u) != 0)
    {
        if (((pInst->mnClipActionFlags & 0x1u) != 0) || HasEventMember(1))   // load; x64 low-24 mask
            queueClipEvents(1, gnAptActionFrameId, 1);
        pInst->mnClipActionFlags &= ~0x1000000u;
    }

    // ---- (5) recurse the child display list -------------------------------
    const int nChildTick = pInst->mDisplayList.tick(-1, 0);

    // ---- (6) recompute the dirty bit --------------------------------------
    // The node stays dirty (forces the x64 bit-6 dirty flag) when it carries an
    // enterFrame handler; otherwise it inherits the child-list tick result --
    // unless it is a playing multi-frame clip (bIsPlaying set, frame count != 1),
    // which leaves the dirty bit untouched.
    if (((pInst->mnClipActionFlags & 0x2u) != 0) || HasEventMember(2))
    {
        mFlagsA |= 0x40u;
    }
    else
    {
        if ((pInst->mnClipActionFlags & 0x2000000u) != 0 &&
            AptGetClipMovie(pInst)->mnFrameCount != 1)
        {
            return (mFlagsA >> 6) & 1u;
        }
        mFlagsA = (mFlagsA & ~0x40u) |
                  ((static_cast<uint32_t>(nChildTick) << 6) & 0x40u);
    }

    return (mFlagsA >> 6) & 1u;   // dirty bit; x64 bit 6
}

// (AptCIH::_gotoAndX @0x82B0D2F0 -- the AS gotoAndPlay/gotoAndStop core -- is the
// static member homed in AptCIHNativeFunctionHelper.cpp, the sole caller family's
// TU. The duplicate body this TU carried was RETIRED 2026-07-10: two homes for one
// console function drifted -- one had the play-arm SetDirtyState right while the
// live one had it inverted onto the stop arm, freezing every gotoAndPlay'd
// transition on a settled node.)
