#pragma once

// Attrib::Gen::proceduralshake -- generated AttribSys class (procedural camera-shake
// attributes: Pitch/Roll/Yaw Frequency+Scale + ShakeMethod). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::proceduralshake::proceduralshake @ 0x8220AE30
//
// The X360 build inlines the generated accessor / `using Instance::...` API away, so the
// constructor is the only proceduralshake function in the ledger (minimal, X360-faithful
// recon), same shape as the sibling generated classes debrisparams / iceanim /
// surfacelist. Derives from Attrib::Instance. Consumed by
// BrnDirector::KeyAnimShakeController::Update.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class proceduralshake : private Instance
    {
    public:
        // The FULL 64-bit class key. RECOVERED 2026-09-06 (camera-shake lane) from the ctor
        // @0x8220AE30: `lis 0xB8FD / ori 0xFFFF / lis 0x88C5 / ori 0xA4BD / insrdi r11,r10,32,0`
        // -> 0x88C5A4BD_B8FDFFFF. Its ONE consumer, KeyAnimShakeController::Update
        // @0x8223D488, compares a shot-list RefSpec against exactly this doubleword before it
        // constructs one, so the low word alone is not enough there.
        static const u64 KU_PROCEDURALSHAKE_CLASS_KEY = 0x88C5A4BDB8FDFFFFull;

        explicit proceduralshake(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // ⭐ THE REFSPEC ctor -- and it is the one the console symbol actually IS.
        // ADDITIVE 2026-09-06 (camera-shake lane). `Attrib::Gen::proceduralshake::
        // proceduralshake` @0x8220AE30 opens with `bl sub_8280A248`, i.e.
        // Attrib::Instance::Instance(const RefSpec&, void*) -- NOT the Collection* ctor this
        // header modelled as the whole class. Its only xref is
        // BrnDirector::KeyAnimShakeController::Update, which builds one over a shake-anim
        // shot-list element. Same body shape as the Collection* sibling below (class check,
        // then a 0x1C-byte default data area when the resolve produced no layout block) and
        // the same shape as iceanim's committed RefSpec ctor.
        proceduralshake(const RefSpec& lrRefSpec, void* lpOwner);

        // Re-exported past the PRIVATE inheritance, exactly as cameradefaults and
        // cameraexternalbehaviour already do it: the console reads this record's attribute
        // data (`lwz r11, 4(instance)` then +0x00/+0x04/+0x08/+0x0C/+0x10/+0x14/+0x18) and
        // that slot is +0x04 on console and +0x08 here, so the ONLY sound way to reach it is
        // the accessor.
        using Instance::GetLayoutPointer;
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::proceduralshake
    // (skipping the assert when the class matches or is unset/0), then give the instance a
    // default data area (0x1C bytes) if it has none.
    inline proceduralshake::proceduralshake(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PROCEDURALSHAKE_CLASS = -1191313409; // 0xB8FDFFFF, Attrib::ClassName::proceduralshake
        if (GetClass() != KI_PROCEDURALSHAKE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROCEDURALSHAKE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1Cu);
    }

    // X360 @0x8220AE30, statement for statement:
    //     bl sub_8280A248                       ; Instance(refspec, owner)
    //     GetClass() == 0x88C5A4BD_B8FDFFFF ?   -> skip
    //     GetClass() == 0                   ?   -> skip
    //     AssertOnClassCheck(GetClass(), ClassKey(), GetCollection())
    //     if (!mpAttributeData) mpAttributeData = DefaultDataArea(0x1C)
    // (The class comparison narrows to the low word here for the same reason iceanim.cpp
    // records: this tree's Instance::GetClass returns `int`. It affects the diagnostic
    // assert only, never the construction or the layout pointer.)
    inline proceduralshake::proceduralshake(const RefSpec& lrRefSpec, void* lpOwner)
        : Instance(lrRefSpec, lpOwner)
    {
        static const int KI_PROCEDURALSHAKE_CLASS = -1191313409; // Attrib::ClassName::proceduralshake
        if (GetClass() != KI_PROCEDURALSHAKE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROCEDURALSHAKE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1Cu);
    }
}
}
