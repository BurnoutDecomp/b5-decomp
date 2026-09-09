// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_wG_ReadSurface.cpp
//
// VehicleManager::ReadSurfaceProperties(u64) -- fills the four global per-surface banks
// (KAVF_SURFACE_GRIP / _ROUGHNESS / _LINEAR_DRAG and the KAB_SURFACE_IS_WATER flags) from the
// AttribSys `surfacelist` collection. Until it has run, gbReadSurfaceProperties is false and every
// VehiclePhysics road query reads the statically-zeroed bank.
//
// The vault must be built by the current attribsys-vault converter
// (build_game_data.py --only "SURFACELIST.BIN" --force): a stale SURFACELIST.BIN dies inside
// Attrib::Collection::GetData and then AVs.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"   // the four tables + gbReadSurfaceProperties
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/physicssurface.h"
#include "GameSource/AttribSys/Generated/classes/gameplaysurface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint (the [surface-bank] witness)

#include <cmath>

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // The epsilon the console splats and compares the sample quad against, lane by lane.
    const f32 KF_SURFACE_LIST_MIN_MAGNITUDE = 1.1920928955078125e-07f;  // FLT_EPSILON

    // The element index of the surface the corruption check samples.
    const u32 KU_SAMPLE_SURFACE_INDEX = 1u;

    // The null-element fallback size for a RefSpec array slot (the console passes the byte
    // literal 0x18; sizeof is the host-correct spelling of the same record).
    const u32 KU_REF_SPEC_DATA_AREA_BYTES = static_cast<u32>(sizeof(Attrib::RefSpec));
}

// The surface-property bank loader. Called from WorldModule::Prepare's world-entity stage with
// the world's surface-list collection key.
void VehicleManager::ReadSurfaceProperties(u64 luSurfaceListKey)
{
    Attrib::Gen::surfacelist lSurfaceList;
    lSurfaceList.ChangeWithDefault(luSurfaceListKey);

    // [FLAG PC bring-up guard -- NOT in the console body.] The console's database is always up
    // by the time this runs; on the PC a failed resolve (database not initialised, or the class
    // / collection absent from a stale vault) leaves the instance with no collection, and the
    // element walk below then dies in Attrib::Collection::GetData and AVs -- taking the boot
    // with it. Bail loudly instead and leave gbReadSurfaceProperties FALSE, so the consumers'
    // own "before properties have been loaded" asserts are what names the failure.
    // DELETE-WHEN the attrib database bring-up guarantees the surfacelist collection resolves at
    // WorldModule::Prepare time.
    if (!lSurfaceList.IsValid())
    {
        static bool sbReported = false;
        if (!sbReported)
        {
            sbReported = true;
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[surface-bank] ReadSurfaceProperties: surfacelist did not resolve "
                       "(no collection bound by ChangeWithDefault) -- the grip/roughness/drag "
                       "bank stays at its static zeros [FLAG PC bring-up guard]\n";
            }
        }
        return;
    }

    // ---- the corruption check on element 1 -----------------------------------------------
    {
        void* lpSampleRefData = lSurfaceList.Surfaces(KU_SAMPLE_SURFACE_INDEX);
        if (!lpSampleRefData)
            lpSampleRefData = Attrib::DefaultDataArea(KU_REF_SPEC_DATA_AREA_BYTES);

        Attrib::RefSpec* lpSampleRef = static_cast<Attrib::RefSpec*>(lpSampleRefData);
        Attrib::Gen::surface lSampleSurface(
            const_cast<Attrib::Collection*>(lpSampleRef->GetCollection()), 0);

        // A componentwise fabs compared against a splatted epsilon; the branch is taken on
        // "NONE of the four lanes greater", so the assert condition is "at least one lane
        // exceeds epsilon".
        const f32* lpfSampleQuad = static_cast<const f32*>(lSampleSurface.GetAttributeData());
        const bool lbSurfaceListLooksSane =
               (std::fabs(lpfSampleQuad[0]) > KF_SURFACE_LIST_MIN_MAGNITUDE)
            || (std::fabs(lpfSampleQuad[1]) > KF_SURFACE_LIST_MIN_MAGNITUDE)
            || (std::fabs(lpfSampleQuad[2]) > KF_SURFACE_LIST_MIN_MAGNITUDE)
            || (std::fabs(lpfSampleQuad[3]) > KF_SURFACE_LIST_MIN_MAGNITUDE);
        CGS_ASSERT(lbSurfaceListLooksSane, "Surface list appears to be corrupt");
    }

    // ---- the count, then the per-surface walk ---------------------------------------------
    KI_NUM_USED_SURFACES = lSurfaceList.Num_Surfaces();

    // [FLAG PC host guard -- NOT in the console body.] The console loops to Num_Surfaces with no
    // upper bound because its four banks and the shipped list were sized together. On the host
    // the banks are KI_MAX_NUM_SURFACES long and a longer list would walk off the end of three
    // 16-byte-strided globals. Same shape of guard the reset pump already applies to
    // KAB_SURFACE_IS_WATER at its own read site.
    CGS_ASSERT(KI_NUM_USED_SURFACES <= KI_MAX_NUM_SURFACES,
               "KI_NUM_USED_SURFACES <= KI_MAX_NUM_SURFACES");
    const s32 liSurfaceCount = (KI_NUM_USED_SURFACES < KI_MAX_NUM_SURFACES)
                                   ? KI_NUM_USED_SURFACES
                                   : KI_MAX_NUM_SURFACES;

    for (s32 liSurface = 0; liSurface < liSurfaceCount; ++liSurface)
    {
        void* lpSurfaceRefData = lSurfaceList.Surfaces(static_cast<u32>(liSurface));
        if (!lpSurfaceRefData)
            lpSurfaceRefData = Attrib::DefaultDataArea(KU_REF_SPEC_DATA_AREA_BYTES);

        Attrib::Gen::surface lSurface(
            *static_cast<Attrib::RefSpec*>(lpSurfaceRefData), 0);
        // Construction order is gameplay-then-physics, as emitted; the refs are the surface
        // layout's +0x58 and +0x40 RefSpecs.
        Attrib::Gen::gameplaysurface lGameplaySurface(lSurface.GameplaySurface(), 0);
        Attrib::Gen::physicssurface lPhysicsSurface(lSurface.PhysicsSurface(), 0);

        // +0x00 roughness, +0x04 linear drag, +0x08 grip. Each scalar is splatted into all
        // four lanes of its table slot.
        const f32 lfRoughness  = lPhysicsSurface.Roughness();
        const f32 lfLinearDrag = lPhysicsSurface.LinearDrag();
        const f32 lfGrip       = lPhysicsSurface.Grip();

        KAVF_SURFACE_ROUGHNESS[liSurface]   = VecFloat{lfRoughness, lfRoughness, lfRoughness, lfRoughness};
        KAVF_SURFACE_GRIP[liSurface]        = VecFloat{lfGrip, lfGrip, lfGrip, lfGrip};
        KAVF_SURFACE_LINEAR_DRAG[liSurface] = VecFloat{lfLinearDrag, lfLinearDrag, lfLinearDrag, lfLinearDrag};
        KAB_SURFACE_IS_WATER[liSurface]     = lGameplaySurface.IsWater();
    }

    // [surface-bank] boot witness -- diagnostic only, DELETE-WHEN the bank has a test that
    // asserts a non-zero grip.
    {
        static bool sbBankReported = false;
        if (!sbBankReported)
        {
            sbBankReported = true;
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[surface-bank] ReadSurfaceProperties: numSurfaces="
                    << liSurfaceCount << " grip[0..3]=" << KAVF_SURFACE_GRIP[0].x << ","
                    << KAVF_SURFACE_GRIP[1].x << "," << KAVF_SURFACE_GRIP[2].x << ","
                    << KAVF_SURFACE_GRIP[3].x << " rough[1]=" << KAVF_SURFACE_ROUGHNESS[1].x
                    << " drag[1]=" << KAVF_SURFACE_LINEAR_DRAG[1].x
                    << " water[1]=" << (KAB_SURFACE_IS_WATER[1] ? 1 : 0) << "\n";
            }
        }
    }

    gbReadSurfaceProperties = true;
}

}
}
