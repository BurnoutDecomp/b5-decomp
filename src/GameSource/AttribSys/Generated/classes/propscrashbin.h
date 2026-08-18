#pragma once

// Attrib::Gen::propscrashbin -- generated AttribSys class: the PROP crash-sound bin
// schema (which crash-sound bin a prop collision falls into, and the collision sample
// ids to pick from once it does). Derives from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The four functions this TU owns are the
// only propscrashbin symbols the X360 ledger carries, and -- unlike the accessor API
// of every other generated class committed here, which is inlined away -- they are
// REAL out-of-line ACCESSORS: BrnSound::Logic::Collision::CollisionStateManager::
// GetRandomSampleID<Attrib::Gen::propscrashbin> @0x82703A28 takes their ADDRESSES and
// hands them to CrashBinUtils<propscrashbin>::GetSampleIds @0x8268FE90, so the compiler
// had to emit them.
//
//   Attrib::Gen::propscrashbin::mCollisionsLarge    @ 0x82682F28
//   Attrib::Gen::propscrashbin::mCollisionsMedium   @ 0x82682F88
//   Attrib::Gen::propscrashbin::mCollisionsSmall    @ 0x82682FE8
//   Attrib::Gen::propscrashbin::mNumCollisionsSmall @ 0x82683058
//
// Each one opens with `lwz r30, 4(r3)` -- Attrib::Instance::mpAttributeData, the
// resolved LAYOUT BLOCK -- and then indexes it at fixed byte offsets. Every offset in
// this file is therefore a DATA-FORMAT offset into that serialised block: it is fixed by
// the AttribSys data, NOT by a host C++ struct, so it does NOT widen on the x64 host.
// (The one thing that does widen is mpAttributeData itself -- console instance+0x04,
// host instance+0x08 -- which is why the block is always reached through
// Instance::GetLayoutPointer() and never by poking the instance.)
//
// ---------------------------------------------------------------------------------
// THE LAYOUT BLOCK (0x190 bytes)
//
// Member NAMES, TYPES and ORDER come from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../propscrashbin.h, `struct _LayoutStruct` at :248-275
// and the accessor declarations at :74-241). The DWARF order is the generated codegen's
// layout order: descending alignment class, and reverse-alphabetical (case-insensitive)
// within each class -- the same rule boostparamsasset.h:138-152 records for its own
// block.
//
// ASM-PINNED offsets (read straight out of the X360 bodies):
//   +0x38  mMaterialB           `ld r11, 0x38(r11)`  BinLookupCache::Build @0x826A87E8
//   +0x40  mMaterialA           `ld r10, 0x40(r11)`  BinLookupCache::Build @0x826A87DC
//   +0x48  _Array_mCollisionsSmall  header   `addi r3, r30, 0x48`   @0x82683004
//   +0x50  mCollisionsSmall[0]                `addi r11, r31, 0x14`  @0x82683014 (*4)
//   +0xA0  _Array_mCollisionsMedium header    `addi r3, r30, 0xA0`   @0x82682FA4
//   +0xA8  mCollisionsMedium[0]               `addi r11, r31, 0x2A`  @0x82682FB4 (*4)
//   +0xF8  _Array_mCollisionsLarge  header    `addi r3, r30, 0xF8`   @0x82682F44
//   +0x100 mCollisionsLarge[0]                `addi r11, r31, 0x40`  @0x82682F54 (*4)
//   +0x160 mNumCollisionsSmall                `addi r3, r11, 0x160`  @0x8268305C
//   +0x164 mNumCollisionsMedium               `addi r3, r11, 0x164`  @0x8268304C  (see ICF note)
//   +0x168 mNumCollisionsLarge                `addi r3, r11, 0x168`  @0x82682F1C  (see ICF note)
//   size 0x190                                `li r3, 0x190`         BinLookupCache::Build @0x826A87CC
//
// CHAINED offsets (not individually read by any recovered body; derived by walking the
// DWARF member order between the pins above -- the walk closes on all three pins with no
// slack, so the assignment is unique):
//   +0x000 Volumes                          RwVector3 (16B, rw::math::vpu::Vector3)
//   +0x010 Pitch                            RwVector3
//   +0x020 IntensityThreshold               RwVector3
//   +0x030 mSpliceBankAsset                 Text (8B -- the codegen sorted it into the
//                                           8-byte alignment class, next to the UInt64s)
//   [+0x038 mMaterialB, +0x040 mMaterialA -- pinned above; the 16/8-byte run ends here
//    exactly at the pinned +0x48 array header, which is the first closure]
//   +0x050 mCollisionsSmall  Int32[20]  (0x50B) -> next header at 0xA0   (second closure)
//   +0x0A8 mCollisionsMedium Int32[20]  (0x50B) -> next header at 0xF8
//   +0x100 mCollisionsLarge  Int32[20]  (0x50B) -> scalars start at 0x150
//   +0x150 Priority                         Float
//   +0x154 PhysicsImpulseNormalization_MIN  Float
//   +0x158 PhysicsImpulseNormalization_MAX  Float
//   +0x15C mOrientation                     UInt32
//   [+0x160/+0x164/+0x168 mNumCollisionsSmall/Medium/Large -- pinned above; the four
//    4-byte scalars above them land exactly on +0x160, which is the third closure]
//   +0x16C MixerSlider   AttribSys::Enums::eCollisionMixerSliders
//   +0x170 mImpactTime                      UInt32
//   +0x174 mGameModes                       UInt32
//   +0x178 mFatalityFlag                    UInt32
//   +0x17C mCameras                         UInt32
//   +0x180 mAction                          UInt32
//   +0x184 DistanceFactor_Min               Float
//   +0x188 DistanceFactor_Max               Float
//   = 0x18C used, rounded up to the pinned 0x190 block size.
//
// ---------------------------------------------------------------------------------
// ICF NOTE (why only ONE of the three mNumCollisions* accessors is a propscrashbin
// symbol). propscrashbin and crashbin share this scalar run byte-for-byte, so the three
// mNumCollisions* bodies are three instructions with NO relocations and are literally
// identical between the two classes -- the linker folded each pair and kept one name:
//     mNumCollisionsSmall  survived as propscrashbin's  @0x82683058
//     mNumCollisionsMedium survived as crashbin's       @0x82683048
//     mNumCollisionsLarge  survived as crashbin's       @0x82682F18
// GetRandomSampleID<propscrashbin> @0x82703AD4/@0x82703AE8 takes the addresses of the
// crashbin-named survivors for the Medium/Large cases, which is exactly what folding
// looks like at a call site. The mCollisions* array accessors are NOT folded because
// their `bl Attrib__Private__GetLength` / `bl Attrib__DefaultDataArea` are PC-relative,
// so the two classes' copies differ in encoded bytes -- and indeed both copies are
// present (crashbin::mCollisionsMedium @0x82682E58 vs propscrashbin's @0x82682F88).
//
// ---------------------------------------------------------------------------------
// NOT RECONSTRUCTED HERE (parked, deliberately):
//  * The CONSTRUCTOR. `Attrib::Gen::propscrashbin::propscrashbin` is a real X360 symbol
//    at 0x826972D8 (named in the xrefs_from of GetRandomSampleID<propscrashbin>, which
//    calls it at 0x82703A74), but it is absent from the per-address JSON export set and
//    from the ledger, so its body is not readable here. It is DECLARED below -- the call
//    site pins the signature (r3=this, r4 = a 64-bit `ld r4, 0x90(r31)` collection key,
//    r5 = 0 owner), matching DWARF propscrashbin.h:23 `propscrashbin(Attribute::Key,
//    uint32_t)` -- but NOT defined. Do not synthesise a body from the crashbin sibling:
//    crashbin's ctor @0x82697108 is the (Collection*, owner) overload and does something
//    different (it asserts the class instead of resolving a collection by key).
//  * The other 23 generated attributes. DWARF declares a full accessor set (and a
//    Set_/TAttrib/out-param form for each), but the X360 build attests only the members
//    below, so per the DWARF-gating rule they are left out rather than homed off a
//    derived offset. Their offsets are recorded in the map above for whoever needs one.
//  * The Attrib::Hash::propscrashbin::* attribute keys. The DWARF carries only their LOW
//    words (e.g. mNumCollisionsSmall = 2283809110) and the schema vault holding the full
//    doublewords is not extracted in this tree. Nothing here needs them -- every reader
//    goes through the layout block by offset. Do not fabricate the high words.
#include "types.hpp"                                                          // s32 / u8 / u32 / u64
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_private.h"   // Attrib::Private (array-length header)

namespace Attrib
{
namespace Gen
{
    // DWARF propscrashbin.h:14 renders the base without an access specifier; modelled
    // `private Instance` like every committed generated sibling (crashbin, iceanim,
    // propscrashbinlist, boostparamsasset). The DWARF's own GetBase() accessors at
    // :28-29 are the tell -- a publicly derived class would not need them.
    class propscrashbin : private Instance
    {
    public:
        // The FULL 64-bit class key. Staged as a doubleword by BinLookupCache::Build
        // @0x826A8794-0x826A87A8 (`lis r11,-0x1601 / ori r3,r11,0x326C` = 0xE9FF326C,
        // then `lis 0x4154 / ori 0xBD6D` inserted into the high half by `insrdi`) and
        // handed to Attrib::FindCollection as the class key. Its low word matches the
        // DWARF Attrib::ClassName::propscrashbin = 3925815916 == 0xE9FF326C.
        static const u64 KU_PROPSCRASHBIN_CLASS_KEY = 0x4154BD6DE9FF326CULL;

        // The generated class-key accessor (DWARF propscrashbin.h:16), same spelling the
        // physicsvehiclehandling / boostparamsasset siblings carry.
        static u64 ClassKey() { return KU_PROPSCRASHBIN_CLASS_KEY; }

        // Layout-block size: BinLookupCache::Build's `li r3, 0x190` fallback
        // @0x826A87CC, which is this class's DefaultDataArea size.
        static const u32 KU_LAYOUT_SIZE = 0x190u;

        // ---- the X360 layout-block offsets (DATA-format; they do NOT widen) ---------
        static const u32 KU_OFFSET_MATERIAL_B          = 0x38u;
        static const u32 KU_OFFSET_MATERIAL_A          = 0x40u;
        static const u32 KU_OFFSET_ARRAY_SMALL_HEADER  = 0x48u;
        static const u32 KU_OFFSET_ARRAY_SMALL         = 0x50u;
        static const u32 KU_OFFSET_ARRAY_MEDIUM_HEADER = 0xA0u;
        static const u32 KU_OFFSET_ARRAY_MEDIUM        = 0xA8u;
        static const u32 KU_OFFSET_ARRAY_LARGE_HEADER  = 0xF8u;
        static const u32 KU_OFFSET_ARRAY_LARGE         = 0x100u;
        static const u32 KU_OFFSET_NUM_SMALL           = 0x160u;
        static const u32 KU_OFFSET_NUM_MEDIUM          = 0x164u;
        static const u32 KU_OFFSET_NUM_LARGE           = 0x168u;

        // DECLARATION ONLY -- see the "NOT RECONSTRUCTED HERE" note in the banner.
        // Build the instance over the propscrashbin collection named by luCollectionKey
        // (GetRandomSampleID<propscrashbin> @0x82703A6C passes the OutputCollision's
        // 64-bit key from +0x90 -- `ld r4,0x90(r31)`, a 64-bit load). DWARF declares FIVE
        // ctors (propscrashbin.h:19-23): (const Collection*, uint32_t) :19, copy :20,
        // (const Instance&) :21, (const RefSpec&, uint32_t) :22 and this by-key one
        // (Attribute::Key, uint32_t) :23. No default argument here: a default would be
        // AMBIGUOUS against the (const Collection*, uint32_t) overload at :19 -- which is
        // the one crashbin's ctor @0x82697108 implements. DWARF spells the owner
        // `uint32_t`; the tree normalises it to `void*` like every generated sibling
        // (Instance(Collection*, void* lpOwner)). NOTE: `using Instance::IsValid;` makes
        // bin.IsValid() CALLABLE (all GetRandomSampleID @0x82703A78 does) but
        // &propscrashbin::IsValid cannot be formed through the private base (C2247).
        propscrashbin(u64 luCollectionKey, void* lpOwner);

        // ------------------------------------------------------------------------
        // THE FOUR LEDGER FUNCTIONS.
        //
        // Return type is `const Int32&` verbatim from DWARF propscrashbin.h:111 / :119 /
        // :127 / :191, and the asm agrees: each body leaves an ADDRESS in r3, never a
        // loaded value. The consumer CrashBinUtils<CrashBin>::GetSampleIds
        // (BrnCollisionStateManager.h/.cpp) takes them as POINTERS TO MEMBER --
        // `const int& (CrashBin::*)() const` / `(CrashBin::*)(unsigned int) const` (DWARF
        // BrnCollisionStateManager.h:529/:530 `{ __pfn, __delta }`) -- and invokes them
        // through the bin (`mr r3,r28 ; mtctr ; bctrl` @0x8268FF24 / @0x8268FF68). A
        // 2026-08-18 verify found the consumer spelled them as free-function pointers;
        // fixed in the same change.
        //
        // Each array accessor bounds-checks the index against the LIVE element count in
        // the 8-byte Attrib::Private header that fronts the array, and on overflow
        // returns the shared 4-byte zero default block -- Attrib::DefaultDataArea(4),
        // `li r3, 4` in all three bodies -- instead of reading past the end. The
        // schema's declared capacity is Int32[20] per array, but GetLength() is what
        // decides, so the capacity is never spelled in the code (same as the asm).
        // ------------------------------------------------------------------------

        // The four bodies are defined OUT-OF-CLASS below (they are real out-of-line
        // functions in the X360 image, not an inlined-away API).

        const s32& mCollisionsSmall(u32 luIndex) const;   // @0x82682FE8
        const s32& mCollisionsMedium(u32 luIndex) const;  // @0x82682F88
        const s32& mCollisionsLarge(u32 luIndex) const;   // @0x82682F28
        const s32& mNumCollisionsSmall() const;           // @0x82683058

        // ------------------------------------------------------------------------
        // ICF-FOLDED TWINS (see the ICF note in the banner). These two are declared by
        // the generated class (DWARF propscrashbin.h:184 / :177) and the propscrashbin
        // code path really does call them -- but the body the X360 image carries is
        // shared with crashbin and is named under crashbin, so they are not propscrashbin
        // ledger functions. The slot offsets are read from those very bodies
        // (@0x8268304C / @0x82682F1C), i.e. from the code the propscrashbin path invokes.
        // ------------------------------------------------------------------------
        const s32& mNumCollisionsMedium() const { return ScalarAt(KU_OFFSET_NUM_MEDIUM); }
        const s32& mNumCollisionsLarge()  const { return ScalarAt(KU_OFFSET_NUM_LARGE); }

        // ------------------------------------------------------------------------
        // GROWN for BrnSound::Logic::Collision::BinLookupCache::Build<propscrashbinlist,
        // propscrashbin> @0x826A8710. That leaf reads these two qwords out of every
        // resolved bin's layout block with `ld r10,0x40(r11)` / `ld r11,0x38(r11)` and
        // stores them into its cache entry (0x40 first, into entry+0x00; 0x38 second,
        // into entry+0x08) -- i.e. the generated accessors DWARF declares at
        // propscrashbin.h:163 / :170, inlined at the call site. The bin's material pair
        // is what the cache is a lookup on.
        // ------------------------------------------------------------------------
        const u64& mMaterialA() const
        {
            return *reinterpret_cast<const u64*>(LayoutBytes() + KU_OFFSET_MATERIAL_A);
        }
        const u64& mMaterialB() const
        {
            return *reinterpret_cast<const u64*>(LayoutBytes() + KU_OFFSET_MATERIAL_B);
        }

        // The base's validity test, re-exported past the private inheritance (same
        // using-declaration precedent as boostparamsasset.h:213 / shotgroup.h). It is the
        // `lCrashBin.IsValid()` assert GetRandomSampleID<propscrashbin> fires at
        // @0x82703A78 (`lwz r11,0(instance) ; cmplwi r11,0` == mpCollection != 0).
        using Instance::IsValid;

    private:
        // The resolved layout block. Console `lwz rN, 4(this)`; on the host the same
        // member sits at instance+0x08, which is why it is reached through the accessor.
        const u8* LayoutBytes() const
        {
            return static_cast<const u8*>(GetLayoutPointer());
        }

        // A 4-byte scalar attribute slot.
        const s32& ScalarAt(u32 luOffset) const
        {
            return *reinterpret_cast<const s32*>(LayoutBytes() + luOffset);
        }

        // One element of an Int32 array attribute: bounds-check luIndex (unsigned --
        // the asm compares with `cmplw`) against the live count in the array's 8-byte
        // Attrib::Private header, then index the payload; on overflow hand back the
        // shared 4-byte default block, exactly as the three bodies do.
        const s32& ArrayElementAt(u32 luHeaderOffset, u32 luElementsOffset, u32 luIndex) const
        {
            const u8* lpLayout = LayoutBytes();
            const Private* lpHeader =
                reinterpret_cast<const Private*>(lpLayout + luHeaderOffset);
            if (luIndex >= lpHeader->GetLength())
                return *static_cast<const s32*>(DefaultDataArea(4u));
            return reinterpret_cast<const s32*>(lpLayout + luElementsOffset)[luIndex];
        }
    };

    // ----------------------------------------------------------------------------
    // THE FOUR LEDGER BODIES.
    //
    // Kept out-of-class (and `inline`, because this TU is header-keyed -- the ledger id
    // is propscrashbin.h and there is no propscrashbin.cpp) so they read as the separate
    // functions the X360 image actually emits. Same shape as the out-of-class ctor
    // definitions in the crashbin / iceanim / propscrashbinlist siblings.
    // ----------------------------------------------------------------------------

    // @0x82682FE8 -- sample id [luIndex] of the SMALL-collision bin list.
    inline const s32& propscrashbin::mCollisionsSmall(u32 luIndex) const
    {
        return ArrayElementAt(KU_OFFSET_ARRAY_SMALL_HEADER, KU_OFFSET_ARRAY_SMALL, luIndex);
    }

    // @0x82682F88 -- sample id [luIndex] of the MEDIUM-collision bin list.
    inline const s32& propscrashbin::mCollisionsMedium(u32 luIndex) const
    {
        return ArrayElementAt(KU_OFFSET_ARRAY_MEDIUM_HEADER, KU_OFFSET_ARRAY_MEDIUM, luIndex);
    }

    // @0x82682F28 -- sample id [luIndex] of the LARGE-collision bin list.
    inline const s32& propscrashbin::mCollisionsLarge(u32 luIndex) const
    {
        return ArrayElementAt(KU_OFFSET_ARRAY_LARGE_HEADER, KU_OFFSET_ARRAY_LARGE, luIndex);
    }

    // @0x82683058 -- how many of the SMALL list's sample ids are populated. The whole
    // body is `lwz r11,4(r3) ; addi r3,r11,0x160 ; blr`: no bounds check, no default
    // block, just the address of the scalar slot.
    inline const s32& propscrashbin::mNumCollisionsSmall() const
    {
        return ScalarAt(KU_OFFSET_NUM_SMALL);
    }
}
}
