#pragma once

// Attrib::Gen::surface — generated AttribSys class (per-surface-type physical/audio
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::surface::surface @ 0x8227FAB0
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist / debrisparams. The X360 build inlines the generated accessor /
// `using` API away, so the constructor is the only surface function in the ledger
// (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class surface : private Instance
    {
    public:
        struct _LayoutStruct
        {
            u8      maLeadingAttributes[0x40];
            RefSpec mPhysicsSurface;
            RefSpec mGameplaySurface;
            RefSpec mAudioSurface;
        };

        explicit surface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        explicit surface(const RefSpec& lrRefSpec, void* lpOwner = nullptr);

        // The resolved attribute-data pointer (Attrib::Instance::GetLayoutPointer, public
        // on the base but reachable only from within the private-derived class). The wheel
        // FX reads the surface's visual-FX sub-collection off this (WheelStateMachine::
        // Update 0x82293EB8: LODWORD(surfaceInstance[1]) + 16). Additive re-export.
        const void* GetAttributeData() const { return GetLayoutPointer(); }

        // DecFIGS surface.h:89/:96; Breaker ReadSurfaceProperties addresses the
        // PhysicsSurface and GameplaySurface RefSpecs at layout +0x40/+0x58.
        const RefSpec& PhysicsSurface() const
        {
            return static_cast<const _LayoutStruct*>(GetLayoutPointer())->mPhysicsSurface;
        }
        const RefSpec& GameplaySurface() const
        {
            return static_cast<const _LayoutStruct*>(GetLayoutPointer())->mGameplaySurface;
        }
        // DecFIGS surface.h:73/:75; RoadnoiseEffect::UpdateParams
        // @0x826E5CE4 constructs the audiosurface instance from layout +0x70.
        // PhysicsSurface and GameplaySurface occupy the preceding two 0x18-byte
        // RefSpecs at +0x40/+0x58, so this named field closes exactly on that pin.
        const RefSpec& AudioSurface() const
        {
            return static_cast<const _LayoutStruct*>(GetLayoutPointer())->mAudioSurface;
        }
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::surface,
    // then give the instance a default data area (0x90 bytes) if it has none.
    inline surface::surface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_SURFACE_CLASS = 2016857936; // Attrib::ClassName::surface (0x7836CF50)
        if (GetClass() != KI_SURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }

    // Breaker sub_8227FB58: the generated RefSpec overload used by
    // VehicleManager::ReadSurfaceProperties' per-surface loop.
    inline surface::surface(const RefSpec& lrRefSpec, void* lpOwner)
        : Instance(lrRefSpec, lpOwner)
    {
        static const int KI_SURFACE_CLASS = 2016857936; // low word 0x7836CF50
        if (GetClass() != KI_SURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x90u);
    }
}
}
