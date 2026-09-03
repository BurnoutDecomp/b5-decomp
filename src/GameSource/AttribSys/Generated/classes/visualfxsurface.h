#pragma once

// Attrib::Gen::visualfxsurface — generated AttribSys class (visual-FX surface attribute
// schema; used by BrnEffects for wheel/detached-part/hinged-part surface-contact FX).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::visualfxsurface::visualfxsurface @ 0x8227FC00
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams/surfacelist. The X360 build inlines the generated accessor /
// `using` API away, so the constructor is the only visualfxsurface function in the
// ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
//
// ⭐⭐ CORRECTED 2026-09-03 — THE ARGUMENT IS AN Attrib::RefSpec, NOT AN Attrib::Collection*.
// This header declared `visualfxsurface(Collection*, void*)`. The asm says otherwise, and the
// two sibling ctors make the distinction unambiguous because they take DIFFERENT Instance
// constructors:
//
//   Attrib::Gen::surface::surface        @0x8227FAB0  bl Attrib__Instance__Instance   (@0x82802DB8, Collection*)
//   sub_8227FB58 (surface, RefSpec form) @0x8227FB58  bl sub_8280A248                 (const RefSpec&)
//   Attrib::Gen::visualfxsurface         @0x8227FC00  bl sub_8280A248                 (const RefSpec&)  <-- THIS ONE
//
// and sub_8280A248's first act is `bl Attrib__RefSpec__GetCollection` on r4, so r4 is a
// RefSpec*. Every caller agrees: the second argument is the surface layout block + 16 —
//   HandleWheels                 @0x82296C80:  visualfxsurface(v30, v33 + 16, 0)   // v33 = surface.mpAttributeData
//   WheelStateMachine::Update    @0x82293EB8:  visualfxsurface(&v33, v37[1] + 16, 0)
//   PostWorldPreparePrepare      @0x822902F0:  visualfxsurface(v49, v52 + 16, 0)
// — i.e. an Attrib::RefSpec EMBEDDED IN the surface's own layout at +0x10, resolved lazily.
// It was never "the visualfxsurface sub-collection", and reading it as one is what took the
// boot down twice (runs 17/18, 2026-09-02): Attrib::Instance::Instance(Collection*) reads
// mpData/mpSource at the x64 Collection offsets +0x28/+0x30 and bumps muRefCount at +0x08 —
// all of that lands PAST THE END of the 24-byte RefSpec — and the very next GetClass() loads
// `*(int*)*(Class**)(refspec + 0x20)`, a non-null garbage pointer. That is exactly the
// measured fault: `Attrib::Instance::GetClass + 0x11` is the `mov eax,[rax]` that follows
// `mov rax,[rax+0x20]` (mpClass) in the x64 body. A textbook valid-pointer/invalid-object.
//
// ⛔ The Collection* overload is NOT declared here, deliberately: the ledger attests exactly
// one visualfxsurface constructor and it is this one, and re-adding the other is how the
// pointer-cast came back.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class visualfxsurface : private Instance
    {
    public:
        explicit visualfxsurface(const RefSpec& lrRefSpec, void* lpOwner = nullptr);

        // The resolved attribute-data pointer (Attrib::Instance::GetLayoutPointer, public
        // on the base but reachable only from within the private-derived class). The wheel
        // FX reads the skid-smoke enable flags (+0x4C / +0x4D) and the two layers' spawn
        // parameters (+0x20.. / +0x34..) off this (WheelStateMachine::Update 0x82293EB8).
        // Additive re-export.
        const void* GetAttributeData() const { return GetLayoutPointer(); }
        // [FLAG PC bring-up, additive host helper] the resolved collection's key, so the
        // [skid-bind] probe can print BOTH SIDES of the resolve without a second lookup.
        using Instance::GetCollection;
    };

    // Chain the Instance ctor (the RefSpec overload @0x8280A248, which resolves + AddRefs +
    // caches the collection back into the ref), assert the collection's class is
    // ClassName::visualfxsurface, then give the instance a default data area (0x60 bytes)
    // if it has none.
    inline visualfxsurface::visualfxsurface(const RefSpec& lrRefSpec, void* lpOwner)
        : Instance(lrRefSpec, lpOwner)
    {
        static const int KI_VISUALFXSURFACE_CLASS = -509236432; // Attrib::ClassName::visualfxsurface (0xE1A5AB30)
        if (GetClass() != KI_VISUALFXSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_VISUALFXSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x60u);
    }
}
}
