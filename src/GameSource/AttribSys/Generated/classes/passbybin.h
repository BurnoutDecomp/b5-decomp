#pragma once

// Attrib::Gen::passbybin — generated AttribSys class (passby-effect distance/volume
// bin table). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::passbybin::passbybin @ 0x82697688
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams/iceanim/shotgroup. The X360 build inlines the
// generated accessor / `using Instance::…` API away, so the constructor is the only
// passbybin function in the ledger (minimal, X360-faithful recon). Derives from
// Attrib::Instance. Used by BrnSound::Logic::Passby::PassbyEffect /
// BrnSound::Logic::Passby::PassbyStateManager.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

#include <cstddef>   // offsetof (the schema-offset pins below)

namespace Attrib
{
namespace Gen
{
    class passbybin : private Instance
    {
    public:
        // The full 64-bit class key. hash64("passbybin") (Bob Jenkins lookup8, seed
        // 0xABCDEF0011223344) == 0xFB59833B6E841773, and that is the doubleword the console
        // ctor @0x82697688 compares GetClass() against (`lis 0x6E84 / ori 0x1773`,
        // `lis -0x4A7 / ori 0x833B`, `insrdi`, `cmpld` at 0x826976A4..0x826976B8). It is also
        // the mClass of the schema's ClassLoadData record at 0x82CD4288. (The ctor below
        // still compares the low word only -- it is untouched here.)
        static const u64 KU_CLASS_KEY = 0xFB59833B6E841773ull;

        // The serialised layout size: the schema ClassLoadData @0x82CD4288 (mLayoutSize 0x70,
        // 15 definitions) and the ctor's DefaultDataArea(0x70) at 0x82697714.
        static const u32 KU_LAYOUT_SIZE = 0x70u;

        // DWARF passbybin.h:131 (`globalenginedata::_LayoutStruct::RwVector2`): a
        // rw::math::vpu::Vector2 slot -- 16 bytes in the data (schema mSize 16, alignment 2^4),
        // of which x / y are read.
        struct RwVector2
        {
            f32 x;
            f32 y;
            u8  mauPad[8];
        };

        // The 0x70-byte layout, one member per schema Definition record (exe-baked schema bin
        // @0x82CD53B0; each record's address is cited). DATA-format offsets: the Text slots
        // are 4-byte serialised references and nothing here widens on the x64 host.
        struct _LayoutStruct
        {
            RwVector2 VolumeNormal;          // +0x00  record 0x82CD6F88 (Vector2)
            RwVector2 VolumeBoost;           // +0x10  record 0x82CD6EF8 (Vector2)
            RwVector2 PitchCurveNormal;      // +0x20  record 0x82CD6E98 (Vector2)
            RwVector2 PitchCurveBoost;       // +0x30  record 0x82CD6EE0 (Vector2)
            u32       LastPassBy;            // +0x40  record 0x82CD6F28 (Text)
            u32       LastBoostPassBy;       // +0x44  record 0x82CD6FD0 (Text)
            u32       FirstPassBy;           // +0x48  record 0x82CD6F40 (Text)
            u32       FirstBoostPassBy;      // +0x4C  record 0x82CD6EC8 (Text)
            f32       VelocityThreshold_Min; // +0x50  record 0x82CD6FA0 (Float)
            f32       VelocityThreshold_Max; // +0x54  record 0x82CD6F70 (Float)
            f32       TriggerRadius;         // +0x58  record 0x82CD6F58 (Float)
            s32       mLastPassBy;           // +0x5C  record 0x82CD6F10 (Int32)
            s32       mLastBoostPassBy;      // +0x60  record 0x82CD6FB8 (Int32)
            s32       mFirstPassBy;          // +0x64  record 0x82CD6FE8 (Int32)
            s32       mFirstBoostPassBy;     // +0x68  record 0x82CD6EB0 (Int32)
            u8        mauPad6C[4];           // +0x6C  (layout size 0x70)
        };

        explicit passbybin(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // DWARF passbybin.h:35. The console binds the effect's bin with the attested base
        // Attrib::Instance::ChangeWithDefault @0x8280D258 (PassbyEffect::Attach 0x826D5324);
        // re-published here because the base is PRIVATE -- the streamsettings sibling's shape.
        void ChangeWithDefault(const RefSpec& lrRefSpec)
        {
            Instance::ChangeWithDefault(const_cast<RefSpec*>(&lrRefSpec));
        }

        // Read-only accessors (DWARF passbybin.h:73-173), each at its schema offset. The
        // console readers: PassbyEffect::UpdateParams @0x826D5068 (VelocityThreshold_Min
        // +0x50, the four curves +0x00..+0x34) and ChooseSampleId @0x82689108 (+0x5C..+0x68).
        const RwVector2& VolumeNormal() const { return Layout().VolumeNormal; }
        const RwVector2& VolumeBoost() const { return Layout().VolumeBoost; }
        const RwVector2& PitchCurveNormal() const { return Layout().PitchCurveNormal; }
        const RwVector2& PitchCurveBoost() const { return Layout().PitchCurveBoost; }
        const f32& VelocityThreshold_Min() const { return Layout().VelocityThreshold_Min; }
        const f32& VelocityThreshold_Max() const { return Layout().VelocityThreshold_Max; }
        const f32& TriggerRadius() const { return Layout().TriggerRadius; }
        const s32& mLastPassBy() const { return Layout().mLastPassBy; }
        const s32& mLastBoostPassBy() const { return Layout().mLastBoostPassBy; }
        const s32& mFirstPassBy() const { return Layout().mFirstPassBy; }
        const s32& mFirstBoostPassBy() const { return Layout().mFirstBoostPassBy; }
        using Instance::IsValid;

    private:
        const _LayoutStruct& Layout() const
        {
            return *static_cast<const _LayoutStruct*>(GetLayoutPointer());
        }
    };

    // Every member pinned to its schema Definition record's mOffset (see the record addresses
    // on the members above), and the whole block to the ClassLoadData mLayoutSize.
    static_assert(offsetof(passbybin::_LayoutStruct, VolumeNormal) == 0x00, "schema 0x82CD6F88");
    static_assert(offsetof(passbybin::_LayoutStruct, VolumeBoost) == 0x10, "schema 0x82CD6EF8");
    static_assert(offsetof(passbybin::_LayoutStruct, PitchCurveNormal) == 0x20, "schema 0x82CD6E98");
    static_assert(offsetof(passbybin::_LayoutStruct, PitchCurveBoost) == 0x30, "schema 0x82CD6EE0");
    static_assert(offsetof(passbybin::_LayoutStruct, LastPassBy) == 0x40, "schema 0x82CD6F28");
    static_assert(offsetof(passbybin::_LayoutStruct, LastBoostPassBy) == 0x44, "schema 0x82CD6FD0");
    static_assert(offsetof(passbybin::_LayoutStruct, FirstPassBy) == 0x48, "schema 0x82CD6F40");
    static_assert(offsetof(passbybin::_LayoutStruct, FirstBoostPassBy) == 0x4C, "schema 0x82CD6EC8");
    static_assert(offsetof(passbybin::_LayoutStruct, VelocityThreshold_Min) == 0x50, "schema 0x82CD6FA0");
    static_assert(offsetof(passbybin::_LayoutStruct, VelocityThreshold_Max) == 0x54, "schema 0x82CD6F70");
    static_assert(offsetof(passbybin::_LayoutStruct, TriggerRadius) == 0x58, "schema 0x82CD6F58");
    static_assert(offsetof(passbybin::_LayoutStruct, mLastPassBy) == 0x5C, "schema 0x82CD6F10");
    static_assert(offsetof(passbybin::_LayoutStruct, mLastBoostPassBy) == 0x60, "schema 0x82CD6FB8");
    static_assert(offsetof(passbybin::_LayoutStruct, mFirstPassBy) == 0x64, "schema 0x82CD6FE8");
    static_assert(offsetof(passbybin::_LayoutStruct, mFirstBoostPassBy) == 0x68, "schema 0x82CD6EB0");
    static_assert(sizeof(passbybin::_LayoutStruct) == passbybin::KU_LAYOUT_SIZE,
                  "schema ClassLoadData 0x82CD4288: mLayoutSize 0x70");

    // Chain the Instance ctor, assert the collection's class is ClassName::passbybin,
    // then give the instance a default data area (0x70 bytes) if it has none.
    inline passbybin::passbybin(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PASSBYBIN_CLASS = 1854150515; // Attrib::ClassName::passbybin (0x6E841773)
        if (GetClass() != KI_PASSBYBIN_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PASSBYBIN_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x70u);
    }
}
}
