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

#include <cstddef>   // offsetof (the layout pins after the class)

namespace Attrib
{
namespace Gen
{
    class surface : private Instance
    {
    public:
        // ⭐ FX-RUMBLE3 2026-09-24: the leading 0x40 bytes are named. The DWARF (surface.h:73..:111)
        // has six attributes -- DebugRenderColor (a Vector4) and five RefSpecs -- and the generator
        // lays the RefSpecs out in reverse alphabetical order after the 16-byte vector:
        //     +0x00 DebugRenderColor  the lane block the "Surface list appears to be corrupt" checks
        //                             read (RumbleManager::UpdateSurfaceRumble 0x82378B94 `lvx128`,
        //                             EffectsModule::PostWorldPreparePrepare)
        //     +0x10 VisualFXSurface   (EffectsModule: `visualfxsurface(surfaceLayout + 16)`)
        //     +0x28 RumbleSurface     (UpdateSurfaceRumble: `rumblesurface(surfaceLayout + 0x28)`
        //                             @0x82378D14 / 0x82378EE8 / 0x823791E4)
        //     +0x40 PhysicsSurface, +0x58 GameplaySurface, +0x70 AudioSurface (the pins below)
        // 0x88 bytes of data in the 0x90-byte area (DefaultDataArea(0x90)). Serialised offsets:
        // they do not widen on the x64 host (RefSpec is 0x18 bytes on both).
        struct _LayoutStruct
        {
            f32     mafDebugRenderColor[4];   // +0x00  DebugRenderColor (DWARF Vector4, 16 bytes)
            RefSpec mVisualFXSurface;         // +0x10
            RefSpec mRumbleSurface;           // +0x28
            RefSpec mPhysicsSurface;          // +0x40
            RefSpec mGameplaySurface;         // +0x58
            RefSpec mAudioSurface;            // +0x70
        };

        explicit surface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        explicit surface(const RefSpec& lrRefSpec, void* lpOwner = nullptr);

        // The resolved attribute-data pointer (Attrib::Instance::GetLayoutPointer, public
        // on the base but reachable only from within the private-derived class). The wheel
        // FX reads the surface's visual-FX sub-collection off this (WheelStateMachine::
        // Update 0x82293EB8: LODWORD(surfaceInstance[1]) + 16). Additive re-export.
        const void* GetAttributeData() const { return GetLayoutPointer(); }
        // [FLAG PC bring-up, additive host helper] the resolved collection's key, so the
        // [skid-bind] probe can name WHICH surface collection each element resolved to.
        using Instance::GetCollection;

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
        // DecFIGS surface.h:103 -- the surface's rumblesurface ref (layout +0x28), which
        // RumbleManager::UpdateSurfaceRumble @0x82378AE0 hands the rumblesurface ctor.
        const RefSpec& RumbleSurface() const
        {
            return static_cast<const _LayoutStruct*>(GetLayoutPointer())->mRumbleSurface;
        }
        // DecFIGS surface.h:82 -- the 16-byte debug colour at layout +0x00, the one lane block
        // the surface-list sanity checks test (IsZero -> "Surface list appears to be corrupt").
        const f32 (&DebugRenderColor() const)[4]
        {
            return static_cast<const _LayoutStruct*>(GetLayoutPointer())->mafDebugRenderColor;
        }
    };

    static_assert(offsetof(surface::_LayoutStruct, mRumbleSurface) == 0x28, "RumbleSurface @+0x28 (UpdateSurfaceRumble 0x82378D14)");
    static_assert(offsetof(surface::_LayoutStruct, mPhysicsSurface) == 0x40, "PhysicsSurface @+0x40 (ReadSurfaceProperties)");
    static_assert(offsetof(surface::_LayoutStruct, mAudioSurface) == 0x70, "AudioSurface @+0x70 (RoadnoiseEffect::UpdateParams)");

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
