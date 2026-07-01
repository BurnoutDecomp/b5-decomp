#pragma once

// Attrib::Gen::iceanim -- generated AttribSys class (ICE camera-take animation
// attributes). The generated accessor / `using` API is inlined away at the call
// sites, so the constructor is the only iceanim entry point in the build (a
// minimal generated-ctor recon, same shape as the sibling generated classes
// surfacelist / debrisparams). Derives from Attrib::Instance.
#include "types.hpp"                                                          // s64 (ClassKey)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class iceanim : private Instance
    {
    public:
        explicit iceanim(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // The 64-bit class-key tag identifying an iceanim runtime object. Checked by
        // BrnDirector::SimpleIceTakedownPlayer::SetIceAnim against a passed object's
        // leading class-key tag.
        static s64 ClassKey() { return 0x4644E379A997C1EELL; }

        // The instance's STORED class-key tag: the leading 8 bytes of the object. The
        // inline form callers use (mirrors SimpleIceTakedownPlayer::SetIceAnim's
        // *reinterpret_cast<const s64*>(lpIceAnim)). The class-key asserts compare this
        // against ClassKey().
        s64 GetClassKey() const { return *reinterpret_cast<const s64*>(this); }

        // ADDITIVE GROW (BrnBehaviourIceAnim.h's SetParameters/ChangeMovie consumers): the take
        // guid carried by this parameter block (instance +0xC, past the 8-byte class-key head
        // GetClassKey() reads). FLAG: declaration-only here -- this class is used as a raw
        // serialised attrib-data block by its consumers (the class-key IS the object's own
        // leading bytes, not reached through Instance::mpAttributeData), so the +0xC field is
        // this object's own concern; no body is fabricated. The consuming behaviour's own TU
        // provides the body when it is reconstructed.
        s32 GetAnimGuid() const;
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::iceanim
    // (skipping the assert when the class is unset/0), then give the instance a
    // default data area (0x10 bytes) if it has none.
    inline iceanim::iceanim(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_ICEANIM_CLASS = -1449672210; // Attrib::ClassName::iceanim
        if (GetClass() != KI_ICEANIM_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_ICEANIM_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }
}
}
