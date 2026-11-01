#pragma once

// Native rwcollision Volume declaration. The runtime descriptor is a pointer;
// serialized FixableVolume data holds its type word until FixUp.

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
        struct Interval;
        struct Feature;
        struct GPInstance;
        struct VolumeLineSegIntersectResult;

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
        enum VolumeType
        {
            VOLUMETYPENULL = 0,
            VOLUMETYPESPHERE          = 1,   // descriptor 0x82F91740, leading word 1
            VOLUMETYPECAPSULE         = 2,   // descriptor 0x82F918C0, leading word 2
            VOLUMETYPETRIANGLE        = 3,   // descriptor 0x82F919A4, leading word 3
            VOLUMETYPEBOX            = 4,   // descriptor 0x82F9176C, leading word 4
            VOLUMETYPECYLINDER        = 5,   // descriptor 0x82F91894, leading word 5
            VOLUMETYPEAGGREGATE       = 6,   // descriptor 0x82F919D0, leading word 6
            VOLUMETYPENUMINTERNALTYPES = 7,
            VOLUMETYPEFORCEENUMSIZEINT = 2147483647
        };

        // The type-specific members overlay the same twelve bytes.
        struct Aggregate;
        struct SphereSpecificData { void* mpNothing; };
        struct CapsuleSpecificData { f32 mfHalfHeight; };
        struct TriangleSpecificData { f32 mfEdgeCos0, mfEdgeCos1, mfEdgeCos2; };
        struct BoxSpecificData { f32 mfHx, mfHy, mfHz; };
        struct CylinderSpecificData { f32 mfHalfHeight, mfInnerRadius; };
        struct AggregateSpecificData { Aggregate* mpAggregate; };

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
            // 8-byte pointer, so the struct is laid out by NAME and its host sizeof
            // deliberately differs from the console's 0x28.
            //
            // ⭐ THE SLOTS ARE BOUND SINCE 2026-08-19 (wave Q5 vtbind). The six records
            // are DEFINED in vendor/renderware/collision/VolumeVTables.cpp -- the only TU
            // that can see all six primitive classes at once -- and declared here in this
            // half's vocabulary. The two spellings are LAYOUT-IDENTICAL and MANGLE
            // IDENTICALLY; both facts are measured, see the gVolumeVTable note at the foot
            // of this file and the banner of that TU.
            // -----------------------------------------------------------------------
            struct VTable;

            typedef RwBool (Volume::*GetBBoxFn)(const Matrix44Affine*, RwBool, AABBox&) const;
            typedef Vector3 (Volume::*GetBBoxDiagFn)() const;
            typedef RwBool (Volume::*GetIntervalFn)(const Vector3&, Interval&) const;
            typedef RwBool (Volume::*GetMaximumFeatureFn)(RwBool, const Vector3&, Feature&) const;
            typedef RwBool (Volume::*CreateGPInstanceFn)(GPInstance&, const Matrix44Affine*) const;
            typedef RwBool (Volume::*LineSegIntersectFn)(const Vector3&, const Vector3&,
                const Matrix44Affine*, VolumeLineSegIntersectResult&, const f32) const;
            typedef void (Volume::*ReleaseFn)();

            struct VTable
            {
                VolumeType    muTypeID;               // +0x00  DWARF `typeID`
                GetBBoxFn     mpfnGetBBox;            // +0x04  DWARF `getBBox`
                GetBBoxDiagFn mpfnGetBBoxDiag;        // +0x08  DWARF `getBBoxDiag`
                GetIntervalFn mpfnGetInterval;        // +0x0C  DWARF `getInterval`
                GetMaximumFeatureFn mpfnGetMaximumFeature;  // +0x10  DWARF `getMaximumFeature`
                CreateGPInstanceFn mpfnCreateGPInstance;   // +0x14  DWARF `createGPInstance`
                LineSegIntersectFn mpfnLineSegIntersect;   // +0x18  DWARF `lineSegIntersect`
                ReleaseFn mpfnRelease;            // +0x1C  DWARF `release`
                const char*   mpcName;                // +0x20  DWARF `name` (const RwChar*)
                u32           muFlags;                // +0x24  DWARF `flags`
            };


            // rwccore.h:1616-1621 `return vTable->typeID;`. The SDK returns
            // rw::collision::VolumeType; the u32 spelling is kept because
            // BrnPropEntityDebugComponent.cpp already switches on the value as a word.
            // Behaviour is unchanged from the previous `**(u32**)(this+0x40)` form -- the
            // descriptor's leading word IS typeID (measured, table above).
            VolumeType GetType() const
            {
                return mpVTable->muTypeID;
            }

            // The primitive-specific data is available through its union arm.
            const f32& GetRadius() const
            {
                return mfRadius;
            }

            // volume.h:1447/1450 return the actual transform address.
            const Matrix44Affine* GetRelativeTransform() const { return &mTransform; }
            Matrix44Affine* GetRelativeTransform() { return &mTransform; }

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
                return (this->*(mpVTable->mpfnGetBBox))(lpTransform, lbTight, lrBBox);
            }

            RwBool CreateGPInstance(GPInstance& instance, const Matrix44Affine* transform) const
            {
                return (this->*(mpVTable->mpfnCreateGPInstance))(instance, transform);
            }

            RwBool LineSegIntersect(const Vector3& start, const Vector3& end,
                                    const Matrix44Affine* transform,
                                    VolumeLineSegIntersectResult& result, f32 fatness) const
            {
                return (this->*(mpVTable->mpfnLineSegIntersect))(start, end, transform, result, fatness);
            }

            // The native member return uses the Vector3 ABI.
            Vector3 GetBBoxDiag() const
            {
                return (this->*(mpVTable->mpfnGetBBoxDiag))();
            }

            // -----------------------------------------------------------------------
            // The two functions the X360 image DOES define out of line for this class.
            // Both were already bodied in SDKs/EATech/rwcollision/volume.cpp; they are
            // declared here (2026-08-18, waveQ2 rwcollision owner) so that TU can stop
            // carrying its own second definition of `class Volume` -- an ODR fork of a
            // type that has this real home, which `cl /c` cannot see.
            // -----------------------------------------------------------------------

            // @ 0x82BB03A8 -- fills the 7-entry shared descriptor table (slot 0 stays null)
            // and returns 1 (MEASURED).
            //
            // `static`, as rwccore.h:1580-1581 says and the console body (which never
            // touches `this`) confirms. Static and non-static are DIFFERENT MANGLED
            // SYMBOLS, so every declaration must agree; the only caller is
            // SceneManagerModule::Construct (GameShared/GameClasses/SceneManager/
            // CgsSceneManagerModule.cpp).
            //
            // The six descriptors it installs are defined in
            // vendor/renderware/collision/VolumeVTables.cpp; with the table filled,
            // GetType() returns the true VolumeType and FixUp/FixDown round-trip.
            static RwBool InitializeVTable();

            // @ 0x82BB12D0 -- the copy assignment MSVC emitted out of line (rwccore.h
            // does not declare one, so this is the implicit member). The console body
            // moves the first 64 bytes as four VMX rows and the remaining 32 word by
            // word; net effect is a 96-byte payload copy.
            Volume& operator=(const Volume& lrSource);

        public:
            mutable Matrix44Affine mTransform;
            VTable* mpVTable;
            union
            {
                SphereSpecificData mSphereData;
                CapsuleSpecificData mCapsuleData;
                TriangleSpecificData mTriangleData;
                BoxSpecificData mBoxData;
                CylinderSpecificData mCylinderData;
                AggregateSpecificData mAggregateData;
            };
            f32 mfRadius;
            u32 muGroupID;
            u32 muSurfaceID;
            mutable u32 muFlags;
        };

        CGS_STATIC_ASSERT((sizeof(Volume::VTable) == 0x28), "Native Volume descriptor size");
        CGS_STATIC_ASSERT((offsetof(Volume::VTable, mpfnGetBBox) == 0x04), "Native Volume GetBBox slot");
        CGS_STATIC_ASSERT((sizeof(Volume) == 0x60), "Native Volume size");
        CGS_STATIC_ASSERT((offsetof(Volume, mpVTable) == 0x40), "Native Volume descriptor");
        CGS_STATIC_ASSERT((offsetof(Volume, mBoxData) == 0x44), "Native Volume data");
        CGS_STATIC_ASSERT((offsetof(Volume, mfRadius) == 0x50), "Native Volume radius");
        CGS_STATIC_ASSERT((offsetof(Volume, muFlags) == 0x5c), "Native Volume flags");

        // ARTIST 82BB03A8 fills the seven-entry descriptor registry.
        extern Volume::VTable* gVolumeVTable[VOLUMETYPENUMINTERNALTYPES];


    }
}
