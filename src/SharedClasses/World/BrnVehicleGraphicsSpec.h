#ifndef SHAREDCLASSES_WORLD_BRNVEHICLEGRAPHICSSPEC_H
#define SHAREDCLASSES_WORLD_BRNVEHICLEGRAPHICSSPEC_H

#include "types.hpp"

// ============================================================================
// BrnVehicle::GraphicsSpec  --  SharedClasses/World/BrnVehicleGraphicsSpec.h
//
// The per-car VISUAL manifest: the resource `VEH_<id>_GR.bin` publishes as
// "<id>_Graphics" (resource type 0x10006). There is no per-car mesh tree in
// Paradise -- a car is muPartsCount body-part MODELS plus a locator matrix per
// part, a collision volume id per part, and the rigid-body -> skin transform
// tables the deformation system drives. Wheels are a SEPARATE resource
// (BrnWheel::GraphicsSpec).
//
// This is the type BrnWorld::RaceCarStreamer caches as
// ResourcePtr<BrnVehicle::GraphicsSpec> and BrnWorld::RaceCarEntityModule::
// RenderRaceCar @0x822CF6A0 walks to build a car's dispatch entries.
//
// ---- SOURCES -------------------------------------------------------------
// MEMBER NAMES + ORDER: the DecFIGS DWARF for SharedClasses/World/
// BrnVehicleGraphicsSpec.h (gated on the X360 ledger). Independently corroborated
// by the burnout.wiki "Vehicle Graphics Spec" table, which lists the same members
// in the same order.
// BEHAVIOUR / RELOCATION: BURNOUT_X360_ARTIST.XEX GraphicsSpecResourceType::FixUp
// @0x8267E3E8 and ::FixDown @0x8267E338 (see BrnVehicleGraphicsSpecResourceType.cpp),
// which rebase exactly the five pointer slots below plus the muPartsCount-entry
// mppRigidBodyToSkinMatrixTransforms array.
//
// ---- ⚠️ THE POINTER SLOTS ARE 4 BYTES WIDE, ON x64 TOO --------------------
// This is the single fact that decides how the render leg is written, and it was
// settled by a RUNTIME PROBE over the live loaded resource, not by reasoning
// (race-car streamer wave, 2026-07-31; the spec was dumped word-by-word straight
// out of RaceCarStreamer::OnGraphicsLoaded on the real pool-4 allocation):
//
//     base                    = 89893712
//     +0  muVersion           = 3            <- matches FixUp's `*a2 != 3` check
//     +4  muPartsCount        = 24
//     +8  mppPartModels       = 89893760     == base + 48
//     +12 muShatteredGlassPartsCount = 6
//     +16 mpShatteredGlassParts      = 89893856  == base + 144
//     +20 mpPartLocators             = 89893936  == base + 224
//     +24 mpPartVolumeIDs            = 89895472  == base + 1760
//     +28 mpNumRigidBodiesForPart    = 89895504  == base + 1792
//     +32 mppRigidBodyToSkinMatrixTransforms = 89895536 == base + 1824
//     +48.. the 24 model pointers themselves (4-byte stride: base+48 .. base+144,
//           which is exactly where mpShatteredGlassParts starts -- 6 x 12-byte
//           ShatteredGlassPart records then run base+144 .. base+216, and
//           mpPartLocators picks up at the next 16-byte boundary, base+224).
//
// `muShatteredGlassPartsCount == 6` sitting at +12 PROVES the slot at +8 is four
// bytes, not eight: a 64-bit read there would splice the count into the pointer's
// high half. So the ported .BIN keeps the 32-bit console record shape, and every
// pointer here is a LOAD-RELATIVE u32 that FixUp has already rebased to a real
// runtime address (the resource heap happens to live below 4 GB, which is what
// makes that representable at all).
//
// ⇒ Declaring these as `Model**` / `Matrix44Affine*` on the x64 gate would read
//   GARBAGE. They are u32 slots with typed accessors that widen on read. Do NOT
//   "fix" the widths to match the DWARF's pointer types -- the DWARF describes a
//   32-bit build and the DATA is still in that shape.
// ============================================================================

namespace CgsGraphics { struct Model; }

namespace rw { namespace math { namespace vpu { struct Matrix44Affine; } } }

namespace BrnVehicle
{

// FixUp's version gate (`*a2 != 3` -> assert). Probe-confirmed: the shipped
// VEH_PUSMC01_GR.bin carries 3.
const u32 KU_VEHICLE_GRAPHICS_SPEC_VERSION = 3;

// One cracked/shattered glass pane. 12 bytes (probe: 6 records fill
// base+144 .. base+216). mpModel is the same 4-byte load-relative slot as the
// body-part model pointers.
struct ShatteredGlassPart
{
    u32 mpModel;            // +0  load-relative CgsGraphics::Model*
    u32 muBodyPartIndex;    // +4  index into the mppPartModels table
    u32 muBodyPartType;     // +8
};

struct GraphicsSpec
{
    // ---- the serialised record (offsets probe-verified; see the banner) ----
    u32 muVersion;                              // +0   == KU_VEHICLE_GRAPHICS_SPEC_VERSION
    u32 muPartsCount;                           // +4   body-part count (24 on the Cavalry)
    u32 mppPartModels;                          // +8   -> u32[muPartsCount] of Model*
    u32 muShatteredGlassPartsCount;             // +12  NOT rebased (an inline count)
    u32 mpShatteredGlassParts;                  // +16  -> ShatteredGlassPart[count]
    u32 mpPartLocators;                         // +20  -> Matrix44Affine[muPartsCount]
    u32 mpPartVolumeIDs;                        // +24  -> u8[muPartsCount]
    u32 mpNumRigidBodiesForPart;                // +28  -> u8[muPartsCount]
    u32 mppRigidBodyToSkinMatrixTransforms;     // +32  -> u32[muPartsCount] of Matrix44Affine*

    // ---- typed accessors: widen the load-relative u32 slots on read --------
    // Every one of these is the console's plain member read; the cast exists only
    // because the slot is four bytes wide on a 64-bit host (see the banner).

    static const void* Widen( u32 luSlot )
    {
        return reinterpret_cast< const void* >( static_cast< uintptr_t >( luSlot ) );
    }

    const CgsGraphics::Model* GetPartModel( u32 luPart ) const
    {
        const u32* lpTable = static_cast< const u32* >( Widen( mppPartModels ) );
        return ( lpTable != 0 )
            ? static_cast< const CgsGraphics::Model* >( Widen( lpTable[luPart] ) )
            : 0;
    }

    const rw::math::vpu::Matrix44Affine* GetPartLocators() const
    {
        return static_cast< const rw::math::vpu::Matrix44Affine* >( Widen( mpPartLocators ) );
    }

    const ShatteredGlassPart* GetShatteredGlassParts() const
    {
        return static_cast< const ShatteredGlassPart* >( Widen( mpShatteredGlassParts ) );
    }

    const u8* GetPartVolumeIDs() const
    {
        return static_cast< const u8* >( Widen( mpPartVolumeIDs ) );
    }

    const u8* GetNumRigidBodiesForPart() const
    {
        return static_cast< const u8* >( Widen( mpNumRigidBodiesForPart ) );
    }
};

// BrnWheel::GraphicsSpec is the wheel-side twin (DWARF: muVersion, mpWheelModel,
// mpCaliperModel; Construct() seeds both models to (Model*)-1 == "absent"). Its own
// home lands with the wheel graphics TU -- NOT declared here, so this header stays the
// single definition of the vehicle side.

} // namespace BrnVehicle

#endif // SHAREDCLASSES_WORLD_BRNVEHICLEGRAPHICSSPEC_H
