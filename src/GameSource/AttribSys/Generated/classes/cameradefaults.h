#pragma once

// Attrib::Gen::cameradefaults -- generated AttribSys class (default camera parameters
// used by BrnDirector::DirectorResourceManager::Prepare). The generated accessor /
// `using Instance::...` API is inlined away at the call site, so the constructor is the
// only cameradefaults function in the X360 ledger (same minimal-recon convention as the
// sibling generated classes shotgroup / iceanim / surfacelist / debrisparams). Derives
// from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::cameradefaults::cameradefaults @ 0x82208770
//
//   * the ctor resolves the cameradefaults class collection via
//     FindCollection(1595961137 /* 0x5F206F31 */, owner), chains Instance(Collection,
//     owner) over it, then (unlike the class-checked siblings) gives the instance a
//     default data area (0x38 bytes) if construction left it without one -- no
//     AssertOnClassCheck is emitted in this ctor's asm.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class cameradefaults : private Instance
    {
    public:
        // The cameradefaults class key, LOW word (0x5F206F31 == 1595961137).
        static const int KI_CAMERADEFAULTS_CLASS = 1595961137;

        // The FULL 64-bit class key Attrib::FindCollection resolves against. RECOVERED
        // 2026-07-31 from the ctor @0x82208770: `lis 0x5F20 / ori 0x6F31 / lis 0x095B /
        // ori 0x375E / insrdi r3,r11,32,0` -> 0x095B375E_5F206F31. (This one was not
        // previously recorded anywhere in the tree.)
        static const u64 KU_CAMERADEFAULTS_CLASS_KEY = 0x095B375E5F206F31ULL;

        // Construct over the cameradefaults collection named by luCollectionKey. lpOwner
        // is the optional owning object threaded through to Attrib::Instance.
        //
        // ⚠️ ARITY FIXED 2026-07-31. The X360 symbol is (this, key, owner) -- r5 is the
        // owner and r4 (the key) is never written by the ctor, so it passes straight
        // through to FindCollection as the collection key, exactly like shotgroup. This
        // declaration used to take only the owner, which meant the caller's key was
        // dropped and the owner pointer was handed to FindCollection in its place.
        // ⚠️ WIDENED to u64 2026-07-31. Attrib::StringToKey returns the FULL 64-bit
        // hash (@0x82805828 tail-calls the lookup8 hash and returns r3 whole), and
        // Attrib::FindCollection hashes the collection key as a whole doubleword. The
        // 32-bit Attrib::Key spelling here truncated it, so every name-keyed collection
        // resolve missed -- the identical defect the CLASS key had until c27208fc.
        // ⚠️ THE KEY IS NO LONGER DEFAULTED -- see the ELEMENT ctor below.
        explicit cameradefaults(u64 luCollectionKey, void* lpOwner = nullptr);

        // ⭐ THE ELEMENT / DEFAULT ctor -- X360 `sub_827DC8C8`, the ONE non-shotgroup slot
        // build in DirectorResourceManager::DirectorResourceManager @0x827DEB98 (offset
        // 0x5D8 == byte 1496 == mCameraDefaults), called with (collection = 0, owner = 0).
        // Read from the IDA database: Instance::Instance, an AssertOnClassCheck against
        // 0x095B375E_5F206F31, then DefaultDataArea(0x38) if the instance has no data
        // area -- the last of which is what distinguishes it from its shotgroup sibling
        // sub_827DC838, and is independent confirmation of the whole 65-slot ordering.
        //
        // ⚠️ The key ctor lost its default argument for the same reason shotgroup's did:
        // this class is held BY VALUE inside DirectorResourceManager, which is reached
        // from the file-scope static gGameModule, so default construction runs PRE-MAIN
        // and must not call Attrib::FindCollection.
        explicit cameradefaults(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // The base validity test, re-exported past the PRIVATE inheritance (same shape as
        // shotgroup's). DirectorResourceManager's PC bank diagnostic reads it; the console
        // itself never asserts this slot (mCameraDefaults is one of the three
        // Prepare builds and never validates).
        using Instance::IsValid;

        // ⭐ Re-exported 2026-08-02 for BoostShakeController::Update, which is the console's
        // one reader of this instance's attribute data (`lwz r11, 4(r6)` then 0x18/0x1C/0x20/
        // 0x24). Same re-export the sibling cameraexternalbehaviour already carries, and the
        // ONLY sound way to reach that slot on x64: it is Instance +0x04 on console and +0x08
        // here, so any modelled head over it is an offset poke.
        using Instance::GetLayoutPointer;
    };

    // X360 ctor @0x82208770: Collection = FindCollection(0x095B375E_5F206F31, r4); chain
    // the Instance ctor over it; then give the instance a default data area (0x38 bytes)
    // if it has none (lwz r11,4(r31) / cmplwi / bne-skip / DefaultDataArea(0x38) /
    // stw r11,4(r31)). No class-check assert appears in this ctor's asm (unlike the
    // shotgroup/iceanim/surfacelist/debrisparams siblings).
    inline cameradefaults::cameradefaults(u64 luCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_CAMERADEFAULTS_CLASS_KEY, luCollectionKey), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x38u);
    }

    // X360 element ctor `sub_827DC8C8`. Same DefaultDataArea(0x38) tail as the key ctor,
    // plus the class check the key ctor's asm does not carry. `GetClass() != 0` keeps a
    // null-collection default construction quiet.
    inline cameradefaults::cameradefaults(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        if (GetClass() != KI_CAMERADEFAULTS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_CAMERADEFAULTS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x38u);
    }
}
}
