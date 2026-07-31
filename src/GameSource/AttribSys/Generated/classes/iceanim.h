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

        // ADDITIVE GROW (ShotSelector::GetCrashShot @0x822398EC): construct over a
        // ShotList RefSpec element (the real X360 ctor symbol takes the RefSpec + the
        // owner). Declaration-only (bodied with the generated AttribSys layer).
        iceanim(const Attrib::RefSpec& lrRefSpec, void* lpOwner);

        // ADDITIVE GROW (ShotSelector::GetCrashShot @0x822398F0..F8): the generated
        // per-attribute reads on the resolved layout block -- SuitableFor (the shot's
        // event-flag mask, layout +0x00) and ShotProperties (the property mask, layout
        // +0x04). DWARF spells both as generated accessors (iceanim.h:199/:193);
        // modelled as plain u32 reads per the minimal-recon convention.
        u32 SuitableFor() const     { return reinterpret_cast<const u32*>(GetLayoutPointer())[0]; }
        u32 ShotProperties() const  { return reinterpret_cast<const u32*>(GetLayoutPointer())[1]; }

        // The ICE take guid this shot names. Bodied in iceanim.cpp.
        //
        // ⭐ OFFSET BASE CORRECTED 2026-07-31, and it was wrong before. This used to be
        // annotated "instance +0xC, past the 8-byte class-key head". The X360 call site
        // (BehaviourIceAnim::SetParameters @0x8220F5C0) settles it:
        //     Attrib::Gen::iceanim::iceanim(v6, a2, 0);   // a2 = the shot RefSpec
        //     v4 = *(v7 + 12);                            // v7 == v6[+4] == mpAttributeData
        //     *(a1 + 3616) = v4;                          // miAnimGuid
        // i.e. the guid is at the resolved LAYOUT block +0xC, not at the object +0xC. It is
        // read through a live instance built from the shot's RefSpec, exactly like
        // SuitableFor()/ShotProperties() above.
        //
        // ⚠️ FLAG -- LATENT CALL-SITE MISMATCH, not fixed here. BrnBehaviourIceAnim.h:368
        // does `typedef Attrib::Gen::iceanim ShotReference`, and SetParameters is handed a
        // ShotReference* that is really the raw 24-byte Attrib::RefSpec element. The X360
        // wraps that RefSpec in a TEMPORARY iceanim and reads the temporary's layout; the
        // committed C++ calls lpParameters->GetAnimGuid() directly on the RefSpec, so it
        // will read RefSpec+4 as if it were mpAttributeData. Same for GetClassKey(), which
        // reads the RefSpec's leading qword and happens to be right for that reason. That
        // TU is unmounted (the ICE frontier), so this is latent; whoever mounts
        // BrnBehaviourIceAnim.cpp must construct the temporary iceanim the console does.
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
