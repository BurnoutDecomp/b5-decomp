// ===========================================================================
// EATech Apt -- AptCharacterSpriteInstBase bodies.   Reconstructed from the X360
// ARTIST.XEX pseudocode/asm (the authoritative spine; no Feb-2007 / DecFIGS
// source exists for this class):
//     ctor                 @ 0x82AFF820
//     ~AptCharacterSpriteInstBase @ 0x82AFF910
//     PreDestroy           @ 0x82AFE9E8
//     HasClipAction        @ 0x82AD50B8
//     SetClipAction        @ 0x82AD5080
//     RemoveClipAction     @ 0x82AD5098
//     SetCreatedDynamic    @ 0x82AE4438
//     `vector deleting destructor' @ 0x82B00110  (compiler thunk -- dropped;
//                                                 ~AptCharacterSpriteInstBase below
//                                                 is the real destructor body)
//
// The shared base for the sprite / movie-clip character instances (character type
// 5). It extends AptCharacterInst with the movie-clip's frame/clip-event state and
// its embedded child display list.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // base ctor/dtor + GetRenderItemWritable
#include "SDKs/EATech/include/Apt/AptRenderItem.h"      // mFlags (SetCreatedDynamic)
#include "SDKs/EATech/include/Apt/AptDisplayList.h"      // embedded display list
#include "SDKs/EATech/include/Apt/AptNativeHash.h"       // the per-instance property hash
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // GetNativeHashVirtual (proto wiring)
#include "SDKs/EATech/include/Apt/AptGlobal.h"           // gpAptGlobalFallback (off_8324E380)
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"        // gpAptPseudoDataPool (off_8324D808)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"  // EAStringC (the class-name key)
#include "SDKs/EATech/Apt/DogmaAllocator.h"               // DOGMA_PoolManager::Allocate

#include <new>   // placement new (the in-place AptNativeHash construct)

// ---------------------------------------------------------------------------
// The class-name key the ctor looks up in the _global fallback scope to find the
// sprite/movie-clip class whose prototype seeds this instance's __proto__. X360
// dword_8324E640 -- an .rdata EAStringC literal; HOMED in AptGlobals.cpp
// ("MovieClip"). (The neighbouring dword_8324E580/0x8324E698 are the "prototype"
// key bodies -- see AptNativeHash.h -- this is a distinct adjacent key.)
// ---------------------------------------------------------------------------
extern const EAStringC gAptSpriteClassKey;   // X360 dword_8324E640 ("MovieClip", AptGlobals.cpp)

// ---------------------------------------------------------------------------
// ctor @0x82AFF820
//   AptCharacterInst::AptCharacterInst(this, pCharacter);   // base ctor
//   *this = off_82145FD8;                                    // vtable (automatic)
//   AptDisplayList::AptDisplayList(this + 7);                // embedded list @+0x1C
//   this->mnGotoFrame       = -1;                            // +0x10
//   this->mnClipActionFlags = 0xC0 | (garbage & 0xF);        // +0x14 (rlwimi 3<<6)
//   this->mnCurrentFrame    = 0;                             // +0x18
//   this->mnLastActionFrame = 0;                             // +0x20
//   v3 = Allocate(off_8324D808, 20); if(v3){ AptNativeHash(v3, 8); }  // mpProperties
//   this->mpProperties = v3;
//   v5 = Lookup(&gpAptGlobalFallback->mHash, &dword_8324E640);
//   v6 = v5->GetNativeHashVirtual();          // vtbl[2] (offset +8)
//   this->mpProperties->Set__Proto__(v6->mpPrototype);   // *(v6+12)
//
// The X360 read-modify-writes mnClipActionFlags's low 4 bits from uninitialised
// memory while inserting 3<<6 into the upper 28; on a fresh object the field is
// indeterminate, so the faithful initialisation is the constant the source wrote:
// the low byte's state flags seeded to 0xC0 (bits 6,7), the clip-event mask (the
// >>8 region) left clear. The display list is constructed as the embedded member;
// the property hash + its __proto__ are the body's work.
// ---------------------------------------------------------------------------
AptCharacterSpriteInstBase::AptCharacterSpriteInstBase(AptCharacter* pCharacter)
    : AptCharacterInst(pCharacter)
    // mDisplayList default-constructed here (X360: AptDisplayList::AptDisplayList(this+7))
{
    mnGotoFrame         = -1;
    // x64 ctor 0x140825A00: `(v & 0xF3000000) | 0x03000000` -- bJustLoaded (bit 24)
    // + bIsPlaying (bit 25) set, clip-event mask (low 24) clear. (The old 0xC0 seed
    // was the X360 big-endian allocation order.)
    mnClipActionFlags   = 0x03000000u;
    mpClipEventHandlers = nullptr; // x64 +0x28 (no registered clip-event handlers yet)
    mnLastActionFrame   = 0;

    // Per-instance property hash (AptNativeHash, capacity 8) from the shared Apt
    // pool. The X360 folds AptNativeHash::AptNativeHash(8) inline into the pooled
    // 20-byte block; reconstructed as a placement-new of the same ctor. (Guarded
    // for the null-pool case exactly as the asm guards the Allocate result.)
    AptNativeHash* pProperties = nullptr;
    if (void* pMem = gpAptPseudoDataPool->Allocate(sizeof(AptNativeHash)))   // x64: 0x28 bytes (console 20)
        pProperties = ::new (pMem) AptNativeHash(8);
    mpProperties = pProperties;

    // Wire this hash's __proto__ to the registered sprite class's prototype: find the class value in
    // the _global fallback scope by its class-name key, take its native property hash, and copy that
    // hash's "prototype" slot into our __proto__.
    //
    // NULL-SAFE (host): the X360 derefs gpAptGlobalFallback unguarded because the sprite class is
    // registered before any sprite instance is built. That registration IS live (AptInit.cpp
    // registers the MovieClip/Object/String classes into gpAptGlobalFallback at Apt init), so the
    // __proto__ wiring runs on the normal path; the guards only cover an inst built before Apt init
    // (then the instance is built structurally without the AS prototype). Also requires mpProperties
    // (null if the pool alloc above failed).
    if (gpAptGlobalFallback != nullptr && mpProperties != nullptr)
    {
        AptValue* pClassValue = gpAptGlobalFallback->Lookup(gAptSpriteClassKey);
        AptNativeHash* pClassHash = (pClassValue != nullptr) ? pClassValue->GetNativeHashVirtual() : nullptr;
        if (pClassHash != nullptr)
            mpProperties->Set__Proto__(pClassHash->mpPrototype);
    }
}

// ---------------------------------------------------------------------------
// ~AptCharacterSpriteInstBase @0x82AFF910
//   *this = off_82145FD8;                          // reset vtable
//   AptDisplayList::~AptDisplayList(this + 7);      // destroy the embedded list
//   return AptCharacterInst::~AptCharacterInst(this);
// Empty body: the embedded mDisplayList destructor and the AptCharacterInst base
// destructor are emitted by the compiler (member + base teardown), and the vtable
// reset is the codegen of entering the destructor. AptCharacterSpriteInstBase owns
// no further teardown of its own. The `vector deleting destructor' thunk
// @0x82B00110 is compiler-generated and intentionally not hand-written.
// ---------------------------------------------------------------------------
AptCharacterSpriteInstBase::~AptCharacterSpriteInstBase()
{
}

// ---------------------------------------------------------------------------
// PreDestroy @0x82AFE9E8
//   return AptDisplayList::PreDestroy(this + 28);   // the embedded list @+0x1C
// Forward to the child display list's PreDestroy (release the placed children
// before the GC tears the instance down).
// ---------------------------------------------------------------------------
void AptCharacterSpriteInstBase::PreDestroy()
{
    mDisplayList.PreDestroy();
}

// ---------------------------------------------------------------------------
// HasClipAction @0x82AD50B8
//   return (mnClipActionFlags >> 8) & nMask;        // srawi: arithmetic shift
// The clip-event-handler mask lives in the high 24 bits of the flags word.
// ---------------------------------------------------------------------------
int32_t AptCharacterSpriteInstBase::HasClipAction(int32_t nMask) const
{
    // x64 @0x140839F70: sign-extend the LOW 24-bit mask (`shl eax,8; sar eax,8`), then AND.
    return ((static_cast<int32_t>(mnClipActionFlags << 8)) >> 8) & nMask;
}

// ---------------------------------------------------------------------------
// SetClipAction @0x82AD5080
//   mnClipActionFlags |= (nMask << 8);              // set the masked clip-event bits
//   return this;
// (The X360 insrwi re-stamps the unchanged low byte -- a no-op confirmation that
// only the >>8 mask region changes.)
// ---------------------------------------------------------------------------
AptCharacterSpriteInstBase* AptCharacterSpriteInstBase::SetClipAction(int32_t nMask)
{
    // x64 @0x1408411B0: `and edx,0FFFFFFh / or [rcx+24h],edx` -- mask into the LOW 24 bits.
    mnClipActionFlags |= (static_cast<uint32_t>(nMask) & 0xFFFFFFu);
    return this;
}

// ---------------------------------------------------------------------------
// RemoveClipAction @0x82AD5098
//   mnClipActionFlags &= ((~nMask) << 8) | 0xFF;    // clear the masked clip-event bits
//   return this;
// The low byte (the sprite state flags) is preserved by the | 0xFF.
// ---------------------------------------------------------------------------
AptCharacterSpriteInstBase* AptCharacterSpriteInstBase::RemoveClipAction(int32_t nMask)
{
    // x64 @0x14083F260: `not edx / or edx,0FF000000h / and [rcx+24h],edx` -- clear LOW-24
    // bits, preserve the high state byte.
    mnClipActionFlags &= (~static_cast<uint32_t>(nMask)) | 0xFF000000u;
    return this;
}

// ---------------------------------------------------------------------------
// SetCreatedDynamic @0x82AE4438
//   AptRenderItem* pItem = AptCharacterInst::GetRenderItemWritable(this);
//   pItem->mFlags = (pItem->mFlags & ~0x08000000) | (bDynamic ? 0x08000000 : 0);
// Set the owned render item's "created dynamic" flag (bit 0x08000000) through the
// writable render item. (The X360 derives the bit via cntlzw(bDynamic) -- 32 when
// the byte is zero, otherwise <32 -- then complements it; the net is simply
// `bDynamic != 0`. Operated on the render item's named mFlags, not a raw offset.)
// ---------------------------------------------------------------------------
void AptCharacterSpriteInstBase::SetCreatedDynamic(bool bDynamic)
{
    AptRenderItem* pItem = GetRenderItemWritable();
    pItem->mFlags = (pItem->mFlags & ~0x08000000u) | (bDynamic ? 0x08000000u : 0u);
}
