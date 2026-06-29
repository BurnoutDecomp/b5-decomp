// ===========================================================================
// EATech Apt -- AptCIH behavioural follow-ons (batch 1).   DECOMPILED from the
// X360 ARTIST.XEX. The node core (ctor/dtor/GC virtuals/state-flag-link/name
// accessors + the type predicates + SetDirtyState) lives in AptCIH.cpp; this file
// carries the standalone behavioural methods as they are reconstructed + verified.
//   GetFirstChild 0x82ADC938 / ContainsNativeHashVirtual 0x82AD74B8 /
//   HasEventMember 0x82AD74E0 / ForceCleanNativeHash 0x82AF2338 /
//   GetAnimationInst 0x82B7B358 / GetCosAngle 0x82AD7448.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // GetTypeTag / mpProperties
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"  // mDisplayList (sprite-base child list)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"   // GetAnimationInst downcast
#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // mpHead / AptDisplayListNode::mpFirst
#include "SDKs/EATech/include/Apt/AptNativeHash.h"               // mnEventHandlerMask / mp__Proto__ / DestroyGCPointers
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"            // AptMatrix a/b/c
#include "SDKs/EATech/include/Apt/AptDefine.h"                   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                      // DOGMA_PoolManager

#include <cmath>   // sqrtf

// ---------------------------------------------------------------------------
// GetFirstChild @0x82ADC938 -- the first placed child of this node. Only the
// sprite-base character instances (movie-clip type 5 / animation type 9) carry a
// child display list (AptCharacterSpriteInstBase::mDisplayList, a subtype member
// outside the AptCharacterInst base); every other character type has no children,
// so the result is null. For a sprite-base node the child list's head node is read
// (lazily pool-allocated, so it can be null), and its first listed AptCIH returned.
// The X360 reads mpCharacterInst directly (no null guard -- only called on placed
// nodes that own an instance).
// ---------------------------------------------------------------------------
AptCIH* AptCIH::GetFirstChild() const
{
    const uint32_t nType = mpCharacterInst->GetTypeTag();
    const bool bSpriteBase = (nType == 5 || nType == 9);   // IsSpriteInstBase
    if (!bSpriteBase)
        return nullptr;

    const AptCharacterSpriteInstBase* pSpriteInst =
        static_cast<const AptCharacterSpriteInstBase*>(mpCharacterInst);

    const AptDisplayListNode* pHead = pSpriteInst->mDisplayList.mpHead;
    if (!pHead)
        return nullptr;   // head node not yet pool-allocated (empty list)

    return pHead->mpFirst;
}

// ---------------------------------------------------------------------------
// ContainsNativeHashVirtual @0x82AD74B8 -- AptValue vtbl[3] override. The X360
// body inlines GetNativeHash (read mpCharacterInst, null-guard, read its
// mpProperties) and coerces the result to a boolean. Reuse GetNativeHash so the
// null-guard stays in one place.
// ---------------------------------------------------------------------------
bool AptCIH::ContainsNativeHashVirtual() const
{
    return GetNativeHash() != nullptr;
}

// ---------------------------------------------------------------------------
// HasEventMember @0x82AD74E0 -- does this value (or anything up its __proto__
// chain) carry an AS event handler in nEventMask? A tight prototype-chain walk:
// each link's native hash is reached through GetNativeHashVirtual() (AptValue
// slot 2); a plain value returns null (chain end), an AptValueWithHash/AptObject/
// AptCIH returns its property table. Returns the masked int (not a bool) -- the
// callers (HasMouseEvent / objectMemberSet) use it as a truthiness test.
// ---------------------------------------------------------------------------
int AptCIH::HasEventMember(int nEventMask)
{
    for (AptValue* pValue = this; pValue != nullptr; )
    {
        AptNativeHash* pHash = pValue->GetNativeHashVirtual();
        if (!pHash)
            return 0;

        const int nMatched = pHash->mnEventHandlerMask & nEventMask;
        if (nMatched != 0)
            return nMatched;

        pValue = pHash->mp__Proto__;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// ForceCleanNativeHash @0x82AF2338 -- tear down + free the character instance's
// per-instance property hash, then clear the slot. The X360 reads the char inst
// (mpCharacterInst), grabs its property hash (mpProperties), and -- if present --
// runs DestroyGCPointers() (release every held value/key + free the bucket table)
// followed by the scalar-deleting destructor (~AptNativeHash + free the storage),
// then nulls mpProperties. The pseudocode's `int`/`return result` is a Hex-Rays
// artifact (r3/this flows through the deleting destructor's return). AptNativeHash
// is pool-allocated (gpNonGCPoolManager), so the scalar-deleting destructor is the
// explicit dtor + Deallocate; ~AptNativeHash is a no-op once DestroyGCPointers has
// run, preserving the X360's two-step order.
// ---------------------------------------------------------------------------
void AptCIH::ForceCleanNativeHash()
{
    AptCharacterInst* pCharacterInst = GetCharacterInst();
    AptNativeHash* pHash = pCharacterInst->mpProperties;
    if (pHash)
    {
        pHash->DestroyGCPointers();
        pHash->~AptNativeHash();
        gpNonGCPoolManager->Deallocate(pHash, sizeof(AptNativeHash));
        pCharacterInst->mpProperties = nullptr;
    }
}

// ---------------------------------------------------------------------------
// GetAnimationInst @0x82B7B358 -- mpCharacterInst narrowed to the concrete
// animation subtype. The X360 is a bare `lwz r3, 0x20(r3); blr` (mpCharacterInst
// at dword [8]); the caller has already confirmed IsAnimationInst (type tag 9),
// so no guard is emitted. The downcast is well-formed (AptCharacterAnimationInst
// derives, through AptCharacterSpriteInstBase, from AptCharacterInst).
// ---------------------------------------------------------------------------
AptCharacterAnimationInst* AptCIH::GetAnimationInst() const
{
    return static_cast<AptCharacterAnimationInst*>(mpCharacterInst);
}

// ---------------------------------------------------------------------------
// GetCosAngle @0x82AD7448 -- extract cos(theta) from an affine transform's first
// column (a, b). Static helper (the X360 takes the matrix pointer in r3, not a CIH
// `this`). When the rotation terms b and c are both exactly zero there is no
// rotation, so cos(theta) = 1; otherwise normalise the first column:
// a / sqrt(a*a + b*b). All ops are single-precision in the asm.
// ---------------------------------------------------------------------------
float AptCIH::GetCosAngle(const AptMatrix* pMatrix)
{
    if (pMatrix->b == 0.0f && pMatrix->c == 0.0f)
        return 1.0f;

    return pMatrix->a / sqrtf(pMatrix->a * pMatrix->a + pMatrix->b * pMatrix->b);
}
