#pragma once

// CgsSceneManager::CgsCollision::PrimitivePairListBuilder — the writer side of a
// PrimitivePairList: it bump-allocates a packed-pair blob (via Prepare) and appends
// {CollisionHeader, primitive-A data, primitive-B data} records to it (via the
// AddPrimitive / AddPrimitivePair family). It derives from PrimitivePairList and adds
// NO data members of its own — every body operates purely on the four inherited base
// bookkeeping fields (mpaDataStream/mu16UsedData/mu16NumTests/mu16MaxDataSize) and the
// inherited CollisionHeader / EVolumeType / KAU16_VOLUME_SIZES.
//
// Each single-primitive record is { CollisionHeader (16 bytes), primitive data }.
// Each primitive-pair record is { CollisionHeader (16 bytes), primitive-A data,
// primitive-B data }. AddCollisionHeader stamps the 16-byte header; the Add* helpers
// then bump-allocate the primitive payload(s) and copy the caller's data in. A worst-case
// box/box pair record is 16 (header) + 5*16 = 96 bytes — the per-primitive reservation
// used by Prepare (X360 computes 96*count as ((count*3)<<5)).

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (rw::math::vpu)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"   // CgsGeometric::Sphere (16-byte)

namespace CgsMemory { class LinearMalloc; }

// The serialised 96-byte rwcollision volume record. POINTER USE ONLY here, so the real
// home (SDKs/EATech/rwcollision/volume_debug_access.h:117, `class Volume`) is NOT included --
// that header is ~400 lines and this one is pulled in by BrnVehicleManager.h. Same spelling
// and the same class key as the committed forward declaration at
// GameSource/World/EntityModules/PropEntityModule/BrnPropEntityDebugComponent.h:57.
// ⚠️ The unrelated `struct rw::collision::Volume` at vendor/renderware/collision/
// CollisionVolume.hpp:166 is a PRE-EXISTING, documented fork of this symbol (see the FORK
// note in volume_debug_access.h:109-117); the two are never co-included, and this
// declaration does not make that worse -- it matches the 96-byte record's key.
namespace rw { namespace collision { class Volume; } }

namespace CgsGeometric
{
    // Box — 80-byte packed oriented box primitive: a 3-row orientation basis
    // (rows 0/16/32 = right/up/at), a position/centre row (48) and a half-dimensions
    // row (64). Copied verbatim (five 16-byte vectors) by AddPrimitive(Box*) /
    // AddPrimitivePair. Matches KAU16_VOLUME_SIZES[E_VOLUME_TYPE_BOX] = 80. Only ever
    // forward-declared elsewhere in the tree (BrnDeformableObject.h uses
    // `struct CgsGeometric::Box;`); this is its provisional layout home. The
    // near-identical-box assert in AddPrimitivePair reads these five rows.
    struct alignas(16) Box
    {
        Vector4 maRows[5];   // +0x00 basisX, +0x10 basisY, +0x20 basisZ, +0x30 pos, +0x40 halfDims
    };
}

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct PrimitivePairListBuilder : public PrimitivePairList
    {
        // --- lifecycle ---
        void Construct();                                   // @0x82814330 — zero the four base bookkeeping fields
        bool Prepare(CgsMemory::LinearMalloc* lpMalloc, u16 lu16NumPrimitives); // @0x82814348 — bump-allocate a 96*count blob

        // ==========================================================================================
        // ⭐ ADDED 2026-08-19 (wave Q6, prop-vs-world contact generation): the RWCOLLISION-VOLUME
        // overload. It is declared ABOVE the Sphere overload because that is the DWARF's own
        // declaration order (references/DecFIGS/dwarfdump/GameShared/GameClasses/SceneManager/
        // Collision/Primitives/CgsPrimitivePairListBuilder.h, SOURCE LINE 77 -- the dumpfile
        // prints it at its own line 17):
        //     void AddPrimitive(const VolRef::Volume *, Matrix44Affine, float32_t, uint16_t);
        // (`VolRef::Volume` is the DWARF's spelling of rw::collision::Volume -- volume.h:39
        //  `typedef rw::collision::Volume Volume`.) The Matrix44Affine is BY VALUE.
        //
        // X360 0x82814AB8 (140 insns, 0x82814AB8..0x82814CE4). IDA leaves it `sub_82814AB8` --
        // the symbol is absent, the function is not. Register map, read off the prologue
        // (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82814AB8.json, raw `assembly`):
        //     r3 = this        (`mr r31, r3`     @0x82814AD8)
        //     r4 = the volume  (`mr r11, r4`     @0x82814AD0; `lwz r10,0x40(r11)` == the
        //                       rwcollision per-TYPE descriptor, `lwz r10,0(r10)` == its typeID,
        //                       `addi r10,r10,-1`, 5-case jump table at jpt_82814B08)
        //     r5 = the transform, read as four 16-byte rows at +0x00/+0x10/+0x20/+0x30
        //     f1 = the padding (`fmr f31, f1`    @0x82814AD4)
        //     r7 = the tag     (`mr r30, r7`     @0x82814ADC, forwarded as r6 to every leaf)
        // ⚠️ GOTCHA 3 IS WHY r6 IS SKIPPED: the f32 rides f1 and consumes its GPR slot, so the
        // u16 tag lands in r7, not r6. r6 is never read by this function -- a signature derived
        // from the GPRs alone would invent a dead fifth parameter.
        //
        // WHAT IT DOES: switch on the volume's rwcollision type id and append the matching
        // packed CgsGeometric primitive, built from the volume payload times the caller's world
        // transform. Case index == typeID - 1, so the five arms are the six-entry gVolumeVTable
        // minus AGGREGATE:
        //     typeID 1 SPHERE   -> 0x82814C04 -> AddPrimitive(Sphere*) @0x82814508  (in tree)
        //     typeID 2 CAPSULE  -> 0x82814B88 -> sub_82814600                       (NOT in tree)
        //     typeID 3 TRIANGLE -> the DEFAULT arm 0x82814CAC, i.e. UNSUPPORTED: it fires
        //                          "Tried to add a RW volume that wasn't a "... at
        //                          CgsPrimitivePairListBuilder.cpp:319 (`li r5, 0x13F`)
        //     typeID 4 BOX      -> 0x82814B20 -> CgsGeometric::Box::Set @0x825E6918 then
        //                          sub_82814570                                     (NOT in tree)
        //     typeID 5 CYLINDER -> 0x82814C48 -> sub_82814678                       (NOT in tree)
        //
        // ⛔ NOT RECONSTRUCTED -- and this is a REPORTED HOLE, not an oversight.
        // ⚠️ CORRECTED wave Q6 round 1 (worldc #1): this used to read "DECLARATION ONLY -- NO
        // BODY ANYWHERE IN THE TREE", which is FALSE and is a DUPLICATE-DEFINITION TRAP. THE
        // ONLY DEFINITION IS THE LOUD NAMED GATE AT CgsPrimitivePairListBuilder.cpp:186 (that TU
        // IS MOUNTED -- tools/build/build_game_exe.bat:867) -- RETIRE THAT GATE IN THE SAME
        // COMMIT THAT LANDS THE REAL SWITCH, or you will add a second definition and get LNK2005
        // in a mounted TU. Bodying it needs three unexported leaves (sub_82814570 @0x82814570
        // has no per-address JSON at all; sub_82814600 and sub_82814678 do) plus two
        // CgsGeometric primitive types that do not exist in b5-decomp/src -- there is no
        // Capsule and no Cylinder under GameShared/GameClasses/Geometric/Primitives/, only
        // Sphere / SweptSphere / Box / AxisAlignedBox / Line / Triangle4. That is its own
        // cluster. A partial switch landed here would omit whole volume kinds with nothing in
        // the log to say so -- the exact failure mode this project's faithfulness rules exist to
        // stop -- so the declaration stands alone and the hole is reported to the conductor.
        // Its ONLY two callers in the whole image are PropManager::DoPartWorldContactGeneration
        // (0x82611F44) and ::DoPropInstanceWorldContactGeneration (0x826124A0) -- measured
        // xrefs -- both landed this wave in PropManager_wQ2_03.cpp.
        // ==========================================================================================
        void AddPrimitive(const ::rw::collision::Volume* lpVolume, Matrix44Affine lTransform,
                          f32 lfPadding, u16 lu16PrimitiveTag);                 // @0x82814AB8

        // AddPrimitive(Sphere*) @ 0x82814508 — append a single-sphere record: stamp a
        // sphere CollisionHeader, bump-allocate 16 bytes, copy the sphere, bump the count.
        void AddPrimitive(CgsGeometric::Sphere* lpSphere, f32 lfPadding, u16 lu16PrimitiveTag);

        // AddPrimitivePair(Box*, Box*) @ 0x82814708 — append a box-vs-box pair record:
        // (debug) assert the two boxes are not near-identical, stamp a box/box header,
        // bump-allocate two 80-byte box payloads, copy both boxes, bump the count.
        void AddPrimitivePair(CgsGeometric::Box* lpBoxA, CgsGeometric::Box* lpBoxB,
                              f32 lfPadding, u16 lu16PrimitiveTagA, u16 lu16PrimitiveTagB);

    private:
        // AddCollisionHeader(EVolumeType, f32, u16) @ 0x828143F0 — bump-allocate a 16-byte
        // CollisionHeader for a single-primitive record and stamp it.
        void AddCollisionHeader(EVolumeType lePrimType, f32 lfPadding, u16 lu16PrimitiveTag);

        // AddCollisionHeader(EVolumeType, EVolumeType, f32, u16, u16) @ 0x82814480 — the
        // two-type header stamp used by the pair helpers (owned by sub_82814480; declared
        // for coherence, defined in its own reconstruction).
        void AddCollisionHeader(EVolumeType lePrimTypeA, EVolumeType lePrimTypeB,
                                f32 lfPadding, u16 lu16PrimitiveTagA, u16 lu16PrimitiveTagB);

        // AllocateMemory @ 0x82812098 — bump allocator over mpaDataStream. Returns the
        // current write cursor (mpaDataStream + mu16UsedData) and advances mu16UsedData by
        // liBytes. Asserts the stream exists and the capacity is not exceeded.
        void* AllocateMemory(s32 liBytes);
    };
}
}
