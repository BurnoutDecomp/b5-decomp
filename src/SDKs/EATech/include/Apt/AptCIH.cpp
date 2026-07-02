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
#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"        // scratch state for the multi-frame skip path
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"        // gAptActionInterpreter operand stack (_gotoAndX)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"          // AptGetAnimationAtLevel (GetRootAnimation's CIHNone path)
#include "SDKs/EATech/include/Apt/AptDefine.h"     // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <new>   // placement new (the scratch AptPseudoDisplayList over pool memory)

// FLAG (un-homed AS/runtime singletons -- declared verbatim by their console name
// equivalents; bodies/definitions land with their owning TUs):
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

// FLAG (un-homed AS interpreter singleton; console &dword_8324E760): the operand
// stack _gotoAndX reads the goto target from. Declared per-TU like its siblings
// (AptCIHNativeFunctionHelper.cpp / AptMovie.cpp).
extern AptActionInterpreter gAptActionInterpreter;   // &dword_8324E760

// FLAG (homed by AptCIH's event/AS-action follow-on TU; console AptCIH::queueClipEvents
// @0x82AD...): queue this node's clip-event handlers (nEventMask) for deferred AS
// execution, stamping them with nFrameId. A free-function shim (matches the
// AptCIH_queueClipEvents AptAnimationTarget.cpp already declares).
// Canonical sig: X360/PS3 AptCIH::queueClipEvents(int, unsigned int, int) -- a3 (the
// frame id) is UNSIGNED; reconciled across all three declaring TUs.
AptValue* AptCIH_queueClipEvents(AptValue* pCIH, int nEventMask, unsigned int nFrameId, int bDeferred);

// FLAG (un-homed AptDisplayList behavioural follow-on; console AptDisplayList::mergeState
// @0x82B0B438 -- explicitly BLOCKED in AptDisplayList.h's note): merge a rebuilt scratch
// pseudo display list into this live child list. Declared as a free function taking the
// child list so the home file can name it without editing AptDisplayList.h.
void* AptDisplayList_mergeState(AptDisplayList* pList, AptPseudoDisplayList* pScratch,
                                void* pProperties, char bForward);

// ---------------------------------------------------------------------------
// AptCIH_GetClipMovie -- the AptMovie timeline embedded inside a sprite/animation
// AptCharacter subclass. The X360 reaches it as `mpRenderItem->mpCharacter + 0x10`;
// the project models that un-homed subclass field with the same FLAGged reinterpret
// AptCharacterAnimation.cpp uses (the AptCharacter base header stops before it).
// Centralised here so the three play-head methods name it once.
// FLAG (un-homed sprite/animation AptCharacter subclass): the embedded AptMovie is a
// fixed member the AptCharacter base type does not yet model.
// FLAG (x64 fork): the console reaches it at char+0x10; the 8-byte GUIAPT64 "1:7:8"
// layout widens the AptCharacter header so it lands at char+0x20 (KU_AptEmbeddedMovieOff,
// AptCIH.h). We ship only the 8-byte format, so this is the single native-64 offset.
static AptMovie* AptCIH_GetClipMovie(const AptCharacterSpriteInstBase* pInst)
{
    AptCharacter* pCharacter = pInst->mpRenderItem->mpCharacter;
    return reinterpret_cast<AptMovie*>(reinterpret_cast<char*>(pCharacter) + KU_AptEmbeddedMovieOff);
}

// FLAG (homed elsewhere): the optional pre-destroy notify hook (console
// dword_1059C6D0); null until installed.
void AptCIH_PreDestroyHook(AptCIH* pCIH);

// The zombie-vector reap (XB1 sub_140830A40; X360 name AptUpdateZombieVector) --
// currently the AptRenderLinkStubs no-op until the zombie vector homes.
extern void* AptUpdateZombieVector(char bClear);

// ctor @0x824C7C
AptCIH::AptCIH(AptCharacter* pCharacter, AptCIH* pParent)
    : AptValueGC(AptVFT_CharacterInstHandle, CO_CIH)
    // mInstanceName default-constructs to the shared empty string.
{
    // FLAG: the console masks pool garbage into mFlagsA/mFlagsB (0xB80007F /
    // |0xFFFC0000); zero-initialised on PC, then the deterministic bits applied.
    mFlagsA = 0;
    mFlagsB = 0xFFFC0000u;

    mpDisplayListParent = pParent;
    if (pParent)
        pParent->AddRef();

    setGCRoot(1);
    SetAllowDelayedDeletion(false);

    mpDisplayListPrevious = nullptr;
    mpDisplayListNext     = nullptr;
    mpAssetString         = nullptr;

    mpCharacterInst = AptCharacterInst::CreateCharacterInst(pCharacter);

    // FLAG: the console finishes by calling SetDirtyState /
    // SetGeneralizedProcessDirtyState keyed on the character type (which read the
    // char-inst flags + propagate up the parent chain). Simplified here to the
    // local generalized-process-dirty bit; the propagation is deferred with the
    // render-tree update path.
    mFlagsA |= 0x01000000u;
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
        return;   // GC not up (FLAG)

    AptNativeHash* pCharProps = nullptr;
    if (mpCharacterInst)
        pCharProps = mpCharacterInst->mpProperties;

    if (mpDisplayListParent)
        AptValue::sReferenceRegistrationCb(this, &mpDisplayListParent, "Parent", 1);
    if (pCharProps)
        pCharProps->RegisterReferences(this);

    // FLAG: sprite/movie nodes (char type 5/9) also mark their display-list
    // state (AptDisplayListState::RegisterReferences) -- deferred with the
    // display-list subsystem.
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
        // FLAG: the console first removes the char inst's render item from the
        // render-tree manager (Update_ItemRemoved) + destroys the inst's property
        // hash; deferred with the render-tree manager. The char inst's own
        // destructor releases its render item.
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
uint32_t AptCIH::GetCIHState() const { return (mFlagsA >> 29) & 3u; }
void AptCIH::SetCIHState(uint32_t eState)
{
    mFlagsA = (mFlagsA & ~0x60000000u) | ((eState << 29) & 0x60000000u);
}

int16_t AptCIH::GetZombieCount() const { return static_cast<int16_t>(mFlagsA >> 7); }
void AptCIH::IncZombieCount()
{
    const int16_t n = static_cast<int16_t>((mFlagsA >> 7) + 1);
    mFlagsA = (mFlagsA & ~0x007FFF80u) | ((static_cast<uint32_t>(n) << 7) & 0x007FFF80u);
}
void AptCIH::DecZombieCount()
{
    const int16_t n = static_cast<int16_t>((mFlagsA >> 7) - 1);
    mFlagsA = (mFlagsA & ~0x007FFF80u) | ((static_cast<uint32_t>(n) << 7) & 0x007FFF80u);
    // XB1 sub_140835B50 (the arbiter's DecZombieCount): when the count reaches
    // zero, reap the dead zombies immediately -- AptUpdateZombieVector(0).
    // (The reap body is the staged zombie-vector tier; the current link-stub is
    // a no-op, faithful while the vector itself is un-homed.)
    if ((mFlagsA & 0x007FFF80u) == 0u)
        AptUpdateZombieVector(0);
}

bool AptCIH::IsInCtor() const { return ((mFlagsA >> 27) & 1u) != 0; }
void AptCIH::SetInCtor(uint32_t b)
{
    mFlagsA = (mFlagsA & ~0x08000000u) | ((b << 27) & 0x08000000u);
}

// ---- packed state / flags (mFlagsB) ---------------------------------------
int  AptCIH::GetCreatedOnFrame() const { return static_cast<int32_t>(mFlagsB) >> 18; }   // srawi: signed/arithmetic shift (sign-extends the 14-bit field)
void AptCIH::SetCreatedOnFrame(int nFrame)
{
    mFlagsB = (mFlagsB & 0x0003FFFFu) | (static_cast<uint32_t>(nFrame) << 18);
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
bool AptCIH::IsMask() const  { return mpCharacterInst->GetRenderItem()->GetIsMask(); }   // @0x82AD5BA0
bool AptCIH::HasMask() const { return mpCharacterInst->GetRenderItem()->GetHasMask(); }  // @0x82AD5BB8

// ---------------------------------------------------------------------------
// GetRootAnimation @PS3 0x820F3C -- the nearest enclosing ANIMATION node. The empty
// CIHNone placeholder (tag 0x25) resolves to the lazily-created level-0 root; any
// other node walks its display-list parent chain ([7] mpDisplayListParent) until the
// character-inst type tag ([8]->mTypeFlags >> 26) is 9 (animation) or 15.
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
// SetDirtyState @0x82AD76B8 -- set/clear the per-node dirty bit (mFlagsA bit 25).
// Clearing, or a non-renderable character type (shape 1 / dynamic-text 2 /
// static-text 10) or the empty AptCIHNone placeholder, leaves it clear. When a
// renderable node is dirtied with bPropagate, the dirty bit is pushed up the
// display-list parent chain, stopping at the first already-dirty ancestor.
// ---------------------------------------------------------------------------
void AptCIH::SetDirtyState(bool bDirty, bool bPropagate)
{
    if (!bDirty)
    {
        mFlagsA &= ~0x02000000u;
        return;
    }

    const uint32_t nType = mpCharacterInst->GetTypeTag();
    if (nType == 1 || nType == 2 || nType == 10 || IsNone())
    {
        mFlagsA &= ~0x02000000u;
        return;
    }

    mFlagsA |= 0x02000000u;
    if (bPropagate)
    {
        for (AptCIH* pAncestor = mpDisplayListParent; pAncestor; )
        {
            if ((pAncestor->mFlagsA & 0x02000000u) != 0)
                break;
            AptCIH* pNext = pAncestor->mpDisplayListParent;
            pAncestor->mFlagsA |= 0x02000000u;
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
//   mnGotoFrame       (+0x10, dword[4]) -- the LIVE current play-head frame. FLAG:
//                     the header's role label (a -1 "pending goto" sentinel) is
//                     refined here; these three functions PROVE +0x10 is the active
//                     current-frame counter the timeline driver reads/advances.
//   mnClipActionFlags (+0x14, dword[5]) -- the low 8 bits are sprite STATE flags:
//                     bit6 (0x40) = "needs a frame action this tick", bit7 (0x80) =
//                     "auto-play / freshly placed". (The high 24 bits are the AS
//                     clip-event mask, untouched here.)
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

    AptMovie* pMovie = AptCIH_GetClipMovie(pInst);
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

            // A freshly-placed clip (state bit7) restarts from frame 0.
            if ((pInst->mnClipActionFlags & 0x80u) == 0x80u)
                pInst->mnGotoFrame = 0;
            // Never replay forward FROM a frame already at/after the target.
            if (pInst->mnGotoFrame >= nFrame)
                pInst->mnGotoFrame = 0;

            // Replay every frame from the current play-head up to and including
            // nFrame into the scratch list (stopping early if it would run past the
            // clip's end). The play-head (mnGotoFrame) is the loop variable.
            while (pInst->mnGotoFrame <= nFrame)
            {
                if (pInst->mnGotoFrame >= AptCIH_GetClipMovie(pInst)->mnFrameCount)
                    break;
                // FLAG: the X360 call site (sub_82B0BE60) only fills r3 (the AptMovie)
                // and r4 (the scratch list); the frame index + trailing args are not
                // re-loaded into r5/r6 at the call. Passed faithfully as the current
                // replay frame for the AptMovie::DoTemporaryFrameControls signature.
                pMovie->DoTemporaryFrameControls(pScratch, pInst->mnGotoFrame, 0, nullptr);
                ++pInst->mnGotoFrame;
            }

            pInst->mnGotoFrame = nFrame;
            AptDisplayList_mergeState(&pInst->mDisplayList, pScratch, pProperties, bForward);

            if (pScratch)
            {
                pScratch->~AptPseudoDisplayList();
                gpAptPseudoDataPool->Deallocate(pScratch, sizeof(AptPseudoDisplayList));   // [c: 8]
            }
        }

        pInst->mnLastActionFrame = pInst->mnGotoFrame;
        // FLAG (x64 narrowing): the X360 returns r3 -- the queueFrameActions result
        // pointer -- as the int result the callers ignore. Carried as a non-zero
        // "ran" marker to avoid an 8->4-byte pointer truncation on x64.
        nResult = pMovie->queueFrameActions(this, pInst->mnGotoFrame) != nullptr;
    }
    return nResult;
}

// tick @0x82B0BED8 -- advance the movie-clip node one frame.
//
// FLAG (x64 bring-up -- deferred, not gated): the console tick dereferences the char
// inst (a1+32), its render item (v6[1]), the render item's character and the embedded
// clip movie (char + embed) WITHOUT null guards -- everything is always live in the
// shipped game. On our partial bring-up the render-tree / AS scope is not fully stood up
// yet, so this faithful body is NOT ticked at boot: the host driver holds it off
// (BrnAptRuntimeBringUp AptRuntimeUpdate: lbTickReady=false) until the converter delivers
// a uniformly-64-bit bundle. The body below is the single faithful X360 decompile.

int AptCIH::tick()
{
    // Only a dirtied node ticks (mFlagsA bit25).
    if ((mFlagsA & 0x02000000u) == 0)
        return 0;

    AptCharacterSpriteInstBase* pInst =
        static_cast<AptCharacterSpriteInstBase*>(mpCharacterInst);   // v6 = *(a1+32)


    // Only sprite(5)/animation(9) clips have a play-head (v6[2] >> 26 == 5 or 9).
    const uint32_t nType = pInst->GetTypeTag();
    if (nType != 5 && nType != 9)
        return (mFlagsA >> 25) & 1u;   // LABEL_44

    const uint32_t nFlags = pInst->mnClipActionFlags;   // v11 = v6[5]
    pInst->mnLastActionFrame = 0;                        // v6[8] = 0

    const bool bNeedsAction = ((nFlags >> 6) & 1u) != 0;   // v12 = (v11 >> 6) & 1  (state bit6 0x40)
    const bool bFreshPlaced = (nFlags & 0x80u) != 0;       // state bit7 (0x80)

    AptMovie* const pClipMovie = AptCIH_GetClipMovie(pInst);   // *(v6[1]+4) + embed

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
        if (((pRenderItem->mFlags >> 27) & 1u) != 0)
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
            // FLAG (un-homed native-8 leaf -- honest boundary): the console loops via
            // jumpToFrame(0), whose arbitrary-jump path replays the intervening frames through
            // AptMovie::DoTemporaryFrameControls + the AptPseudoDisplayList temporary-frame skip
            // resolver. That subsystem still reads command records at CONSOLE (4-byte) offsets and
            // is NOT ported to the native-8 GUIAPT layout (verified via crash dump: DoTemporary
            // FrameControls -> CmdPtr reads a straddled/misaligned pointer 0x180400d000007ff6 -> AV).
            // Until it is homed (its own follow-on), a multi-frame nested clip that reaches its end
            // would AV on the wrap. So the wrap RESETS the play-head to frame 0 directly (a plain
            // loop) instead of the arbitrary-jump replay: the next tick's frame-0 doFrameControls
            // re-composes the clip's frame-0 content, which is the same on-screen result for a
            // looping clip (the replay-merge only matters for a mid-timeline SEEK, not a loop-to-0).
            // (The console additionally re-queues the label-0 actions via jumpToFrame's queueFrame
            // Actions tail; that is AS-VM work which is deferred here, so the plain reset is faithful
            // to the "un-run action queue" state.) Then fall to the enterFrame stage (label_27).
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
        if (((nFlags2 & 0x40u) != 0) || (((nFlags2 & 0x80u) != 0) && gbAptRecorderGate != 0))
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
    if ((((pInst->mnClipActionFlags & 0x80u) == 0) ||
         ((pInst->mTypeFlags & 0xFC000000u) == 0x24000000u)) &&
        (((pInst->mnClipActionFlags & 0x200u) != 0) || HasEventMember(2)))
    {
        AptCIH_queueClipEvents(this, 2, gnAptActionFrameId, 1);
    }
    // (4) construct/load clip event on first placement; then clear the freshly-placed bit.
    if ((pInst->mnClipActionFlags & 0x80u) != 0)
    {
        if (((pInst->mnClipActionFlags & 0x100u) != 0) || HasEventMember(1))
            AptCIH_queueClipEvents(this, 1, gnAptActionFrameId, 1);
        pInst->mnClipActionFlags &= ~0x80u;
    }

    // ---- (5) recurse the child display list -------------------------------
    const int nChildTick = pInst->mDisplayList.tick(-1, 0);

    // ---- (6) recompute the dirty bit --------------------------------------
    // The node stays dirty (forces bit25) when it carries an enterFrame handler;
    // otherwise it inherits the child-list tick result -- unless it is a
    // pending-action multi-frame clip (state bit6 set, frame count != 1), which
    // leaves the dirty bit untouched.
    if (((pInst->mnClipActionFlags & 0x200u) != 0) || HasEventMember(2))
    {
        mFlagsA |= 0x02000000u;
    }
    else
    {
        if ((pInst->mnClipActionFlags & 0x40u) != 0 &&
            AptCIH_GetClipMovie(pInst)->mnFrameCount != 1)
        {
            return (mFlagsA >> 25) & 1u;
        }
        mFlagsA = (mFlagsA & 0xFDFFFFFFu) |
                  ((static_cast<uint32_t>(nChildTick) << 25) & 0x02000000u);
    }

    return (mFlagsA >> 25) & 1u;
}

// _gotoAndX @0x82B0D2F0 -- the AS gotoAndPlay/gotoAndStop core (static: the CIH is
// passed in r3, the arg count in r4, the play flag in r5).
void* AptCIH::_gotoAndX(int nArgCount, unsigned int bPlay)
{
    if (nArgCount >= 1)
    {
        AptCharacterInst* pInst = mpCharacterInst;
        // The goto target is the top of the interpreter operand stack.
        AptValue* pTarget = gAptActionInterpreter.stackAt(0);

        // The custom-control 0x3C char family does not seek by this path.
        if ((pInst->mTypeFlags & 0xFC000000u) != 0x3C000000u)
        {
            int nFrame;
            // A defined string-value (vtbl 1) / string-object (vtbl 33) operand names
            // a frame LABEL; anything else is a numeric frame number. Read the value's
            // type/defined state through the named bitfield accessors (the x64-native
            // form of the console `(*(v6+4)<<25)>>25` / `(*(v6+4)>>27)&1`).
            const int nVtbl = static_cast<int>(pTarget->getVtblIndex());
            const bool bIsLabel = (nVtbl == AptVFT_StringValue || nVtbl == AptVFT_StringObject)
                                  && pTarget->getIsDefined();

            if (bIsLabel)
            {
                // FLAG (x64): the X360 hands AptNativeHash::Lookup the label value's
                // embedded EAStringC (console `v6 + 8`; for a register value it first
                // dereferences the register's stored value). AptValue::Get_ToString is
                // the blessed value-layer accessor for that name EAStringC (returning the
                // embedded string, or rendering the value into the scratch), used here in
                // place of the x64-shifted raw +8 / register-deref offsets.
                EAStringC strScratch;
                const EAStringC* pName = AptValue::Get_ToString(pTarget, &strScratch);

                AptMovie* pMovie = AptCIH_GetClipMovie(
                    static_cast<AptCharacterSpriteInstBase*>(pInst));
                AptValue* pLabelMatch = (pMovie->mpLabelHash && pName)
                    ? pMovie->mpLabelHash->Lookup(*pName)
                    : nullptr;
                nFrame = pLabelMatch ? (pLabelMatch->toInteger() + 1) : 0;   // (-1)+1 == 0 on miss
            }
            else
            {
                nFrame = pTarget->toInteger();
            }

            // The stored/label frame is 1-based; seek to the 0-based index.
            const int nSeek = nFrame - 1;
            if (nSeek >= 0)
            {
                jumpToFrame(nSeek);

                // Set/clear the clip's auto-play state bit (bit6 / 0x40 of the sprite
                // mnClipActionFlags) from bPlay, then -- when it is PLAYING (bPlay set)
                // -- re-dirty the node so it keeps ticking.
                const bool bWantPlay = (bPlay != 0);
                AptCharacterSpriteInstBase* pSprite =
                    static_cast<AptCharacterSpriteInstBase*>(mpCharacterInst);
                pSprite->mnClipActionFlags =
                    (pSprite->mnClipActionFlags & 0xFFFFFFBFu) | (bWantPlay ? 0x40u : 0u);
                if (bWantPlay)
                    SetDirtyState(true, true);
            }
        }
    }
    return gpUndefinedValue;
}
