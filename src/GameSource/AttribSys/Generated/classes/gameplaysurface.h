#pragma once

// Attrib::Gen::gameplaysurface — generated AttribSys class (the "gameplay surface"
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::gameplaysurface::gameplaysurface @ 0x825C0C78
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams/propscrashbinlist. The X360 build inlines the
// generated accessor / `using Instance::...` API away, so the constructor is the only
// gameplaysurface function in the ledger (minimal, X360-faithful recon). Derives from
// Attrib::Instance. Called by BrnPhysics::Vehicle::VehicleManager::ReadSurfaceProperties.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class gameplaysurface : private Instance
    {
    public:
        struct _LayoutStruct
        {
            bool mbIsWater;
        };

        explicit gameplaysurface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        explicit gameplaysurface(const RefSpec& lrRefSpec, void* lpOwner = nullptr);

        const void* GetAttributeData() const { return GetLayoutPointer(); }
        const bool& IsWater() const
        {
            return static_cast<const _LayoutStruct*>(GetLayoutPointer())->mbIsWater;
        }
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::gameplaysurface,
    // then give the instance a default data area (1 byte) if construction left it without one.
    inline gameplaysurface::gameplaysurface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_GAMEPLAYSURFACE_CLASS = 713126835; // Attrib::ClassName::gameplaysurface (0x2A8173B3)
        if (GetClass() != KI_GAMEPLAYSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_GAMEPLAYSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1u);
    }

    inline gameplaysurface::gameplaysurface(const RefSpec& lrRefSpec, void* lpOwner)
        : Instance(lrRefSpec, lpOwner)
    {
        static const int KI_GAMEPLAYSURFACE_CLASS = 713126835; // low word 0x2A8173B3
        if (GetClass() != KI_GAMEPLAYSURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_GAMEPLAYSURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1u);
    }
}
}
