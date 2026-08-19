#pragma once

// ============================================================================
// b5-decomp/src/SDKs/EATech/rwcollision/volume_debug_access.h
//
// Minimal typed view over an rw::collision::Volume for the debug renderers (the prop
// debug overlay's Draw) and for the two rwcollision dispatch inlines the game calls
// (GetBBox / GetBBoxDiag). rwcollision is NOT present in the PC libs (it must be
// decompiled from the console build), and the Volume payload is an external EATech
// *serialised* collision record (a fixed 96-byte blob -- see
// SDKs/EATech/rwcollision/volume.cpp), so its fields are addressed by their fixed byte
// offsets in the blob, which is the documented exception to the no-offset rule for
// serialised / SPU data.
//
// ---------------------------------------------------------------------------
// LAYOUT -- console offsets, all MEASURED unless marked otherwise. The canonical SDK
// declaration is in the leaked EARenderWare header
// references/Feb-2007/BrnEntityModuleUnity/EARenderWare/stable/platform/include/
// native/ps3-ppu-gcc/rwccore.h:1487-1614 (`class Volume`), and the DecFIGS DWARF
// prints the same member sequence at
// references/DecFIGS/dwarfdump/SDKs/EATech/include/cmn/rw/collision/volume.h:1304-1554:
//
//   transform  @ +0x00 (0)   Matrix44Affine, 64 bytes  (rwccore.h:1599 / volume.h:1525)
//   vTable     @ +0x40 (64)  Volume::VTable*           (rwccore.h:1600 / volume.h:1530)
//   <union>    @ +0x44 (68)  {aggregate|sphere|capsule|triangle|box|cylinder}SpecificData
//                            -- 12 bytes; the BOX arm is the half-extents x/y/z that the
//                            debug overlay reads, and the x lane doubles as the
//                            capsule/cylinder half-height  (volume.h:1534-1541)
//   radius     @ +0x50 (80)  float32_t                 (volume.h:1545)
//   groupID    @ +0x54 (84)  uint32_t                  (volume.h:1548)
//   surfaceID  @ +0x58 (88)  uint32_t                  (volume.h:1551)
//   m_flags    @ +0x5C (92)  uint32_t                  (volume.h:1554)  -> sizeof == 96
//
// The +0x40/+0x44/+0x50 offsets are independently pinned in the X360 asm by
// PropEntityDebugComponent::Draw @ 0x822A9770, and sizeof(Volume)==96 is pinned a second
// time by the 96-byte indexing stride of PropTypeData's volume run
// (PropManager::GetPropInertia @0x82612640: `slwi r11,r30,1 ; add r11,r30,r11 ;
// slwi r11,r11,5` == i*96) and static_asserted in
// SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h:201.
//
// ⚠️ HOST-vs-CONSOLE HAZARD -- RESOLVED 2026-08-18 (wave Q5 integration). The record must
// stay 96 bytes on the host (the serialised stride above), but the vTable slot at +0x40
// holds a POINTER on the console, which is 8 bytes on x64 and 4 on the console; a host
// pointer written into +0x40 would overlap +0x44..+0x47 -- the box half-extent X lane
// (and FixableVolume::FixUp used to do exactly that through a `void**`). THE HOST
// REPRESENTATION IS THEREFORE THE TYPE ENUM: +0x40 keeps the 4-byte on-disk VolumeType
// (1..6) for the whole lifetime of the record, FixUp/FixDown only validate it, and every
// reader (Volume::GetVTable() below, FixableVolume.cpp, the vendor narrow-phase TUs'
// TU-local descriptor views, the four *Volume::Initialize bodies) recovers the console's
// pointer as gVolumeVTable[type]. Derivation: scratchpad/waveQ5/rwc3.owner.md section 7.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Matrix44Affine

namespace rw
{
    namespace collision
    {
        // rw::collision::AABBox lives in vendor/renderware/collision/AABBox.hpp. It is
        // only NAMED here (a reference parameter and a return-by-pointer-to-member), which
        // does not need the definition -- deliberately, so that including this header does
        // not drag AABBox.hpp's math vocabulary into PropTypeData's many includers.
        class AABBox;

        // RenderWare boolean. Identical typedef to the ones already in
        // vendor/renderware/collision/AggregateVolume.hpp:46 and FeatureEdge.hpp:60 -- an
        // identical typedef repeated in the same namespace is legal, so co-inclusion with
        // those headers does not clash.
        typedef s32 RwBool;

        // The rwcollision volume primitive kinds. Values are the SDK's
        // rw::collision::VolumeType ids (DWARF volume.h:1238; rwccore.h enum
        // VOLUMETYPENULL..VOLUMETYPENUMINTERNALTYPES), MEASURED a second time out of the
        // shipped image: each per-type descriptor's leading word is its own id (see the
        // vTableArray dump in the VTable comment below).
        //
        // The Draw switch in PropEntityDebugComponent::Draw @0x822A9770 dispatches on
        // 1/2/4/5 only; its default branch asserts "Collision volume very confused about
        // what type it is".
        //
        // ⚠️ E_VOLUMETYPE_BBOX is this tree's spelling of the SDK's VOLUMETYPEBOX (4); the
        // name is kept because BrnPropEntityDebugComponent.cpp already switches on it.
        // ⚠️ The unrelated 128-byte placeholder enum in
        // vendor/renderware/collision/CollisionVolume.hpp:28-32 puts E_VOLUMETYPE_BOX at
        // 2 -- which is CAPSULE here. That is a separate pre-existing fork (see the class
        // comment below); the two headers are never co-included, and the values in THIS
        // enum are the measured ones.
        enum EVolumeType
        {
            E_VOLUMETYPE_SPHERE          = 1,   // descriptor 0x82F91740, leading word 1
            E_VOLUMETYPE_CAPSULE         = 2,   // descriptor 0x82F918C0, leading word 2
            E_VOLUMETYPE_TRIANGLE        = 3,   // descriptor 0x82F919A4, leading word 3
            E_VOLUMETYPE_BBOX            = 4,   // descriptor 0x82F9176C, leading word 4
            E_VOLUMETYPE_CYLINDER        = 5,   // descriptor 0x82F91894, leading word 5
            E_VOLUMETYPE_AGGREGATE       = 6,   // descriptor 0x82F919D0, leading word 6
            E_VOLUMETYPE_NUMINTERNALTYPES = 7   // == the 7-entry vTableArray in volume.cpp
        };

        // Typed accessor over the serialised 96-byte volume blob. The named getters read
        // the fixed-offset fields (documented serialised-data exception).
        //
        // ⚠️ THIS CLASS MUST NOT GAIN A VIRTUAL FUNCTION. sizeof(Volume) == 96 is
        // static_asserted (BrnPhysicsPropTypeData.h:201) and is the indexing stride of the
        // serialised volume run; a vptr would shift every field. The dispatch below is NOT
        // C++ virtual dispatch -- it goes through the rwcollision per-TYPE descriptor
        // pointer stored in the record at +0x40.
        //
        // ⚠️ FORK, REPORTED NOT RESOLVED: a second, unrelated `rw::collision::Volume`
        // exists at vendor/renderware/collision/CollisionVolume.hpp:36 (a 128-byte
        // placeholder image with its own EVolumeType). THIS class -- the 96-byte
        // serialised record -- is the one PropTypeData/PropPartTypeData hold
        // (BrnPhysicsPropTypeData.h:8/:101/:177) and the one the DWARF names as
        // `VolRef::Volume` in GetPropInertia/GetPartInertia's locals
        // (dwarfdump/.../BrnPropManager.cpp:2692/:2755, where volume.h:39 spells
        // `typedef rw::collision::Volume Volume`). The two are never co-included today.
        class Volume
        {
        public:
            // -----------------------------------------------------------------------
            // The rwcollision per-TYPE method descriptor.
            //
            // NOT a C++ vtable and NOT reached through a vptr: it is an ordinary data
            // member of the serialised record (`Volume::VTable* vTable` @ +0x40), and each
            // primitive kind has exactly ONE shared descriptor in .data. Declaration
            // copied from rwccore.h:1584-1595; the DWARF prints the identical member
            // sequence at volume.h:1507-1518 (typeID, getBBox, getBBoxDiag, getInterval,
            // getMaximumFeature, createGPInstance, lineSegIntersect, release, name, flags).
            //
            // MEASURED -- the six live descriptors read out of the shipped image with
            // headless IDA 9.3 on 2026-08-18 (BURNOUT_X360_ARTIST.XEX.i64), console word
            // offsets, one row per slot:
            //
            //   vTableArray[]  +0x00 +0x04 getBBox            +0x08 getBBoxDiag   +0x20 name
            //   1 0x82F91740     1   SphereVolume::GetBBox    SphereVolume::..    "SphereVolume"
            //   2 0x82F918C0     2   CapsuleVolume::GetBBox   CapsuleVolume::..   "CapsuleVolume"
            //   3 0x82F919A4     3   TriangleVolume::GetBBox  TriangleVolume::..  "TriangleVolume"
            //   4 0x82F9176C     4   BoxVolume::GetBBox       BoxVolume::..       "BoxVolume"
            //   5 0x82F91894     5   CylinderVolume::GetBBox  CylinderVolume::..  "CylinderVolume"
            //   6 0x82F919D0     6   AggregateVolume::GetBBox AggregateVolume::.. "AggregateVolume"
            //
            // FULL per-slot dump, re-measured 2026-08-18 (waveQ5 rwc3) on a private .i64
            // copy -- console word offsets, X360 addresses:
            //
            //  slot        [1] SPHERE  [2] CAPSULE [3] TRIANGLE [4] BOX    [5] CYLINDER [6] AGGREGATE
            //  +00 typeID   1           2           3            4          5            6
            //  +04 getBBox  82BA8020    82BAF9C8    82BBA038     82BA9FC8   82BAD490     82BBBA10
            //  +08 diag     82BA8580    82BAF6A8    82BBA110     82BA8890   82BAC7B8     82BBBB58
            //  +0C intvl    82BAC980    82BAC980    82BAC980     82BAC980   82BAC980     00000000
            //  +10 maxfeat  82BA81B0    82BAF808    82BBACD0     82BA87F0   82BAC988     82B0F1B8
            //  +14 creategp 82BA8100    82BAF6F0    82BBAA00     82BA92E8   82BAC7F8     82B0F1B8
            //  +18 lineseg  82BA82C8    82BAFCF8    82BBB970     82BA9478   82BAF688     00000000
            //  +1C release  82AD5078    82AD5078    82AD5078     82AD5078   82AD5078     82AD5078
            //  +20 name    "SphereVolume" "CapsuleVolume" "TriangleVolume" "BoxVolume"
            //                                           "CylinderVolume" "AggregateVolume"
            //  +24 flags    0           0           0            0          0            0
            //
            // The three shared/ICF-folded targets were disassembled, not assumed:
            //   0x82AD5078 = a lone `blr`            -- the empty function (411 xrefs; the
            //                                           exporter labels it STUB, gotcha 6)
            //   0x82BAC980 = `li r3,1 ; blr`         -- a folded "return 1" (54 xrefs). This
            //                                           is the getInterval slot for types
            //                                           1..5; IDA mislabels it
            //                                           `MassiveAdClient3::...MediaDownload`.
            //                                           DO NOT believe that name.
            //   0x82B0F1B8 = `li r3,0 ; blr`         -- a folded "return 0" (33 xrefs), shared
            //                                           by AGGREGATE's getMaximumFeature AND
            //                                           createGPInstance slots; IDA mislabels
            //                                           it `MassiveAdClient3::...`.
            // AGGREGATE's getInterval and lineSegIntersect slots are genuinely 0 in the image.
            //
            // ⚠️ CONSOLE-ONLY OFFSETS. The +0x04/+0x08/... word offsets above are X360
            // facts and appear here as documentation only: on the host every slot is an
            // 8-byte pointer-to-member, so the struct is laid out by NAME and its host
            // sizeof deliberately differs from the console's 40. Nothing in this tree
            // constructs a VTable yet (see the link note in volume.cpp), so no host layout
            // is pinned by anything.
            // -----------------------------------------------------------------------
            struct VTable;

            // rwccore.h:1490-1505 -- the descriptor slots are POINTERS TO MEMBER FUNCTION
            // of Volume (the concrete bodies are the primitive subclasses' members, stored
            // downcast). On the X360 (MSVC, single inheritance, no vptr) that is a plain
            // 4-byte code pointer, which is exactly the 4-byte stride the dump above shows.
            typedef RwBool  (Volume::*GetBBoxFn)(const Matrix44Affine* lpTransform,
                                                 RwBool lbTight, AABBox& lrBBox) const;
            typedef Vector3 (Volume::*GetBBoxDiagFn)() const;

            // The four slots whose parameter types (Interval, Feature, GPInstance,
            // VolumeLineSegIntersectResult) are NOT reconstructed in this tree yet. They
            // are carried as a generic member-function pointer so the slot ORDER stays
            // honest and nothing can call them by accident. rwccore.h:1493-1505 has the
            // real signatures for whoever lands those types.
            typedef void (Volume::*UnrecoveredMethodFn)();

            struct VTable
            {
                u32                 muTypeID;               // +0x00  DWARF `typeID`
                GetBBoxFn           mpfnGetBBox;            // +0x04  DWARF `getBBox`
                GetBBoxDiagFn       mpfnGetBBoxDiag;        // +0x08  DWARF `getBBoxDiag`
                UnrecoveredMethodFn mpfnGetInterval;        // +0x0C  DWARF `getInterval`
                UnrecoveredMethodFn mpfnGetMaximumFeature;  // +0x10  DWARF `getMaximumFeature`
                UnrecoveredMethodFn mpfnCreateGPInstance;   // +0x14  DWARF `createGPInstance`
                UnrecoveredMethodFn mpfnLineSegIntersect;   // +0x18  DWARF `lineSegIntersect`
                UnrecoveredMethodFn mpfnRelease;            // +0x1C  DWARF `release`
                const char*         mpcName;                // +0x20  DWARF `name` (const RwChar*)
                u32                 muFlags;                // +0x24  DWARF `flags`
            };

            // `Volume::vTable` @ +0x40 (rwccore.h:1600). Read by byte offset for the same
            // serialised-record reason as the getters below.
            //
            // HOST REPRESENTATION (2026-08-18, wave Q5 integration -- the "honest host
            // design" of scratchpad/waveQ5/rwc3.owner.md section 7): the console keeps a
            // 4-byte descriptor POINTER in this slot (FixableVolume::FixUp @0x828A87A0
            // swaps the on-disk type enum for gVolumeVTable[enum]; FixDown puts the enum
            // back). An x64 pointer is 8 bytes and would overlap +0x44..+0x47 (the box
            // half-extent X lane), and the record cannot grow (96-byte serialised stride).
            // So on the host the slot ALWAYS holds the 4-byte type enum (1..6) and every
            // reader indexes gVolumeVTable[] with it -- the same descriptor the console
            // pointer would have named. FixUp/FixDown are therefore validation-only on
            // the host. Defined below the class, after gVolumeVTable's declaration.
            const VTable* GetVTable() const;

            // rwccore.h:1616-1621 `return vTable->typeID;`. The SDK returns
            // rw::collision::VolumeType; the u32 spelling is kept because
            // BrnPropEntityDebugComponent.cpp already switches on the value as a word.
            // Behaviour is unchanged from the previous `**(u32**)(this+0x40)` form -- the
            // descriptor's leading word IS typeID (measured, table above).
            u32 GetType() const
            {
                return GetVTable()->muTypeID;
            }

            // Box half-extents (x/y/z); x lane doubles as the capsule/cylinder half-height.
            // The union arm at console +0x44 (volume.h:1534-1540).
            const Vector3& GetExtent() const
            {
                return *reinterpret_cast<const Vector3*>(maPayload + 0x44);
            }

            // Sphere / capsule / cylinder radius -- `float32_t radius` @ +0x50
            // (volume.h:1545).
            f32 GetRadius() const
            {
                return *reinterpret_cast<const f32*>(maPayload + 0x50);
            }

            // rwccore.h:1701-1706 `return &transform;` -- the volume-local transform IS the
            // record's first 64 bytes (DWARF volume.h:1447/:1525). Corroborated by rung 1:
            // the GetBBox call sites pass the VOLUME POINTER ITSELF as the `tm` argument
            // (PropEntityModule::InitializePropPhysicsData @0x822DA840 emits
            // `(*(*(volume+64)+4))(volume, volume, 1, &aabb)`), which is only meaningful if
            // &transform == volume.
            //
            // ⚠️ The SDK (and the DWARF) return a POINTER here, not a reference; the
            // reference spelling is this tree's and is kept because
            // BrnPropEntityDebugComponent.cpp:323/:354 already multiplies the result.
            // ⚠️ The previous comment on this line said the transform sits at +0x30. That
            // was wrong: +0x30 is the wAxis (translation) ROW of a 64-byte Matrix44Affine
            // whose base is +0x00. Corrected 2026-08-18 (waveQ2 rwcollision owner).
            const Matrix44Affine& GetRelativeTransform() const
            {
                return *reinterpret_cast<const Matrix44Affine*>(maPayload);
            }

            // -----------------------------------------------------------------------
            // rwccore.h:1713-1718 -- the SDK's own one-line inline:
            //     inline RwBool Volume::GetBBox(const Matrix44Affine *tm, RwBool tight,
            //                                   AABBox &bBox) const
            //     { return (this->*(vTable->getBBox))(tm, tight, bBox); }
            // DWARF declaration: volume.h:1454
            //     RwBool GetBBox(const rw::math::vpu::Matrix44Affine *, RwBool, AABBox &) const;
            //
            // There is NO out-of-line body in the X360 image and no per-address export --
            // it is header-inline and the compiler expands it at every call site. The
            // expansion is visible verbatim in PropManager::GetPropInertia @0x82612640:
            //     0x82612750  lwz   r11, 0x40(r3)   ; r3 = the volume, +0x40 = vTable
            //     0x82612754  lwz   r11, 4(r11)     ; descriptor +0x04 = getBBox
            //     0x82612758  mtctr r11
            //     0x8261275C  bctrl                 ; r3=volume r4=&tm r5=1 r6=&AABBox
            // (identical shape in GetPartInertia @0x82612AC8 and in
            // PropEntityModule::InitializePropPhysicsData @0x822DA840). Neither of those
            // callers tests the RwBool in r3.
            //
            // MUST NOT be virtual -- see the class banner.
            // -----------------------------------------------------------------------
            RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool lbTight, AABBox& lrBBox) const
            {
                return (this->*(GetVTable()->mpfnGetBBox))(lpTransform, lbTight, lrBBox);
            }

            // rwccore.h:1719-1723 / DWARF volume.h:1457. Same dispatch, descriptor slot
            // +0x08. No caller in the reconstructed tree yet; landed with GetBBox because
            // the descriptor slot it uses is already typed above.
            Vector3 GetBBoxDiag() const
            {
                return (this->*(GetVTable()->mpfnGetBBoxDiag))();
            }

            // -----------------------------------------------------------------------
            // The two functions the X360 image DOES define out of line for this class.
            // Both were already bodied in SDKs/EATech/rwcollision/volume.cpp; they are
            // declared here (2026-08-18, waveQ2 rwcollision owner) so that TU can stop
            // carrying its own second definition of `class Volume` -- an ODR fork of a
            // type that has this real home, which `cl /c` cannot see.
            // -----------------------------------------------------------------------

            // @ 0x82BB03A8 -- fills the 7-entry shared descriptor table (slot 0 stays null)
            // and returns 1 (`li r3,1` at 0x82BB03B8, MEASURED -- the old `return 0` gate
            // did not even match the console's return value).
            //
            // NOW `static`, as rwccore.h:1580-1581 / DWARF volume.h:1491 / the X360 body
            // (which never touches `this`) all say, and as the tree's two other minimal
            // forward declarations already spelled it:
            // GameShared/GameClasses/SceneManager/CgsSceneManagerModule.cpp:48 (the ONLY
            // caller, at :191) and GameSource/World/WorldLinkStubs.cpp:150.
            //
            // ⚠️⚠️ THIS CHANGE IS HALF OF ONE COMMIT. Static and non-static are DIFFERENT
            // MANGLED SYMBOLS, so until the inert boot gate
            // `int rw::collision::Volume::InitializeVTable() { return 0; }` in
            // GameSource/World/WorldLinkStubs.cpp is DELETED (block
            // :2414-2427 as of 2026-08-18 -- banner through closing brace), this
            // declaration + volume.cpp's real body collide with it: LNK2005. The gate file
            // is conductor-owned, so the delete is REPORTED, not done here (waveQ5 rwc3).
            //
            // WHAT IT BUYS: while the gate was live, the caller bound to `return 0`,
            // volume.cpp's real body was dead, gVolumeVTable stayed ALL ZERO,
            // FixableVolume::FixUp wrote NULL into every volume's +0x40 slot, and
            // GetType()/GetBBox()/GetBBoxDiag() null-dereferenced. With the gate gone the
            // table is filled with the six real descriptors defined in volume.cpp, so
            // GetType() returns the true VolumeType and FixUp/FixDown round-trip.
            static RwBool InitializeVTable();

            // @ 0x82BB12D0 -- the copy assignment MSVC emitted out of line (rwccore.h
            // does not declare one, so this is the implicit member). The console body
            // moves the first 64 bytes as four VMX rows and the remaining 32 word by
            // word; net effect is a 96-byte payload copy.
            Volume& operator=(const Volume& lrSource);

        private:
            u8 maPayload[96];   // serialised rwcollision volume record (see volume.cpp)
        };

        // ---------------------------------------------------------------------------
        // The shared per-type descriptor table -- X360 dword_8327EEE0..dword_8327EEF8,
        // seven consecutive words, filled by Volume::InitializeVTable @0x82BB03A8 and
        // read by FixableVolume::FixUp, SphereVolume::Initialize @0x82BA84E8,
        // BoxVolume::BoxVolume @0x82BAA0F0, CylinderVolume::Initialize @0x82BAD3F0,
        // TriangleVolume::Initialize @0x82BB0680 and the two resource FixUps.
        //
        // The element type is the DWARF's: `rw::collision::Volume::VTable *[7]
        // vTableArray` (volume.h:1521; SDK rwccore.h:1597 `static VTable *vTableArray[]`).
        // It used to be `void*[7]`, which threw the type away and forced a
        // `const_cast<u8*>` at every fill site. Declared here (2026-08-18, waveQ5 rwc3)
        // so consumers can stop re-declaring it privately.
        //
        // ⚠️ CHANGING THE ELEMENT TYPE CHANGES THE MANGLED SYMBOL -- MEASURED with
        // dumpbin: `void*[7]`  -> ?gVolumeVTable@collision@rw@@3PAPEAXA
        //          `VTable*[7]`-> ?gVolumeVTable@collision@rw@@3PAPEAUVTable@Volume@12@A.
        // The vendor narrow-phase TUs (vendor/renderware/collision/CollisionVolume.hpp)
        // declare the SAME symbol with their own `struct Volume { struct VTable; }` --
        // the leaf `struct VTable` under a scope named `Volume` mangles identically, so
        // both halves of the tree bind to the one table volume.cpp fills.
        // ---------------------------------------------------------------------------
        extern Volume::VTable* gVolumeVTable[E_VOLUMETYPE_NUMINTERNALTYPES];

        // The +0x40 slot holds the 4-byte VolumeType on the host (see the declaration
        // in the class); the console's pointer is recovered by indexing the table.
        inline const Volume::VTable* Volume::GetVTable() const
        {
            const u32 luType = *reinterpret_cast<const u32*>(maPayload + 0x40);  // serialised record slot
            return gVolumeVTable[luType < static_cast<u32>(E_VOLUMETYPE_NUMINTERNALTYPES) ? luType : 0u];
        }
    }
}
