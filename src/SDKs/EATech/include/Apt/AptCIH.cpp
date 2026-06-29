// ===========================================================================
// EATech Apt -- AptCIH core.   DECOMPILED from the PS3 EXTERNAL ELF.
//   ctor 0x824C7C / dtor 0x804E68 / RegisterReferences 0x7E9C78 /
//   DestroyGCPointers 0x80553C / PreDestroy 0x7ECBAC + the state/flag/link
//   accessors (0x7DF0xx..0x7FB648).
//
// The packed flag words use the console rotate+mask idiom; reconstructed here as
// the equivalent clear-range/set bit-ops on the whole word. Scope is the node
// core; the behavioural methods are follow-ons.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"     // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"

// FLAG (homed elsewhere): the optional pre-destroy notify hook (console
// dword_1059C6D0); null until installed.
void AptCIH_PreDestroyHook(AptCIH* pCIH);

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
}

bool AptCIH::IsInCtor() const { return ((mFlagsA >> 27) & 1u) != 0; }
void AptCIH::SetInCtor(uint32_t b)
{
    mFlagsA = (mFlagsA & ~0x08000000u) | ((b << 27) & 0x08000000u);
}

// ---- packed state / flags (mFlagsB) ---------------------------------------
int  AptCIH::GetCreatedOnFrame() const { return static_cast<int>(mFlagsB >> 18); }
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
