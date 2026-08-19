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
#include "BrnCommonTypes.h"   // Vector4 / Matrix44Affine (rw::math::vpu)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"     // CgsGeometric::Sphere   (16 bytes)
#include "GameShared/GameClasses/Geometric/Primitives/CgsCapsule.h"    // CgsGeometric::Capsule  (32 bytes)
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"        // CgsGeometric::Box      (80 bytes)
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"   // CgsGeometric::Cylinder (80 bytes)

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

// ⭐ THE LOCAL `CgsGeometric::Box` FORK THAT STOOD HERE IS GONE (2026-08-19, wave Q6
// cluster `addprim`). It was `struct alignas(16) Box { Vector4 maRows[5]; }` with its own
// banner calling itself "its provisional layout home". Box now lives at its real,
// MEASURED home -- GameShared/GameClasses/Geometric/Primitives/CgsBox.h, included above --
// which is where the console's own Box::Set body is (its assert's __FILE__ string, dumped
// from the image this wave, is ".../Geometric/Primitives/CgsBox.h", line 95). The five
// 16-byte rows the fork modelled are exactly `Matrix44Affine mTransform` (rows 0..3) plus
// `Vector3Plus mDimensionsAndFatness` (row 4), so the layout is unchanged and sizeof is
// still 80; only the member NAMES changed, and AddPrimitivePair below was updated with it.

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
        //     typeID 1 SPHERE   -> 0x82814C04 -> AddPrimitive(Sphere*)   @0x82814508
        //     typeID 2 CAPSULE  -> 0x82814B88 -> AddPrimitive(Capsule*)  sub_82814600
        //     typeID 3 TRIANGLE -> the DEFAULT arm 0x82814CAC, i.e. UNSUPPORTED: it fires the
        //                          refusal assert at CgsPrimitivePairListBuilder.cpp:319
        //                          (`li r5, 0x13F`). The message string, dumped whole from the
        //                          image this wave (aTriedToAddARwV @0x820DB268) rather than left
        //                          truncated, is "Tried to add a RW volume that wasn't a box,
        //                          capsule, sphere or cylinder (only these are supported)".
        //     typeID 4 BOX      -> 0x82814B20 -> CgsGeometric::Box::Set @0x825E6918 then
        //                          AddPrimitive(Box*)      sub_82814570
        //     typeID 5 CYLINDER -> 0x82814C48 -> AddPrimitive(Cylinder*) sub_82814678
        // typeID 0 (NULL) and 6 (AGGREGATE) reach the same default label through the `bgt` --
        // both are out of the 5-entry table's range once 1 is subtracted.
        // ⚠️ The "(NOT in tree)" markers this list used to carry on three of the arms are GONE
        // because they are no longer true: all four leaves and both missing primitive types
        // landed in wave Q6 round 2. Do not re-derive them from an older copy of this banner.
        //
        // ⭐⭐ BODIED 2026-08-19 (wave Q6 round 2, cluster `addprim`) -- THE LOUD CONDUCTOR GATE
        // THAT STOOD AT CgsPrimitivePairListBuilder.cpp:186 IS DELETED IN THE SAME COMMIT, and
        // that gate was the SOLE definition, so nothing else has to be retired with it. All five
        // arms are real, none is omitted: the four supported volume kinds build their packed
        // CgsGeometric primitive and append it, and the fifth (TRIANGLE, and every out-of-range
        // id) fires the console's own refusal assert. The three sibling overloads declared just
        // below and the two brand-new primitive types (CgsCapsule.h / CgsCylinder.h) landed with
        // it; CgsGeometric::Box moved from a fork in THIS header to its measured home CgsBox.h,
        // where its Set body (@0x825E6918) now lives.
        //
        // ⭐ THE CONSOLE'S Box::Set VALIDITY CALL IS LIVE AGAIN (wave Q7, 2026-08-19). This block
        // used to say it was the one thing the console does that this does not -- that
        // CgsGeometric::Box::Set's call to CgsGeometric::Box::IsValid @0x825BEB80 (264 insns) was
        // OMITTED because IsValid had no body anywhere in the tree. IsValid is now a real body in
        // CgsBox.cpp, Box::Set calls it, and its debug-only diagnostic assert is restored verbatim.
        // ⚠️ CONSEQUENCE FOR THIS FILE: PrimitivePairListBuilder::AddPrimitive calls Box::Set per
        // primitive per frame, so this TU now carries an undefined reference to
        // CgsGeometric::Box::IsValid -- CgsBox.cpp must be mounted or the link fails here.
        //
        // Its ONLY two callers in the whole image are PropManager::DoPartWorldContactGeneration
        // (0x82611F44) and ::DoPropInstanceWorldContactGeneration (0x826124A0) -- measured
        // xrefs -- both landed in wave Q6 round 1 in PropManager_wQ2_03.cpp.
        // ==========================================================================================
        void AddPrimitive(const ::rw::collision::Volume* lpVolume, Matrix44Affine lTransform,
                          f32 lfPadding, u16 lu16PrimitiveTag);                 // @0x82814AB8

        // AddPrimitive(Sphere*) @ 0x82814508 (25 insns) — append a single-sphere record: stamp a
        // sphere CollisionHeader, bump-allocate 16 bytes, copy the sphere, bump the count.
        void AddPrimitive(CgsGeometric::Sphere* lpSphere, f32 lfPadding, u16 lu16PrimitiveTag);

        // ==========================================================================================
        // ⭐ THE THREE SIBLING SINGLE-PRIMITIVE APPENDERS, landed 2026-08-19 (wave Q6, cluster
        // `addprim`). IDA leaves all three as `sub_` (the symbols are absent, the functions are
        // not); each is the byte-for-byte twin of the Sphere overload above with a different
        // EVolumeType and payload size, and each is reached from the switch in
        // AddPrimitive(const rw::collision::Volume*, ...).
        //
        //   AddPrimitive(Box*)      sub_82814570  35 insns  type 4, AllocateMemory(0x50)
        //   AddPrimitive(Capsule*)  sub_82814600  29 insns  type 2, AllocateMemory(0x20)
        //   AddPrimitive(Cylinder*) sub_82814678  36 insns  type 5, AllocateMemory(0x50)
        // Declared in the DWARF's own order (CgsPrimitivePairListBuilder.h:89 / :101 / :107),
        // with the DWARF's exact parameter types (CgsGeometric::X *, float32_t, uint16_t).
        //
        // ⚠️ THE DWARF HAS A SIXTH OVERLOAD THIS CLUSTER DELIBERATELY DID NOT LAND:
        //     void AddPrimitive(Triangle4 *, float32_t, uint16_t);   // DWARF :95
        // It is NOT reachable from the volume switch -- typeID 3 (rwcollision TRIANGLE) takes
        // the refusal arm -- and no X360 address for it has been located, so there is nothing
        // to reconstruct it from and nothing calling it. Left undeclared rather than declared
        // bodyless. Same for the DWARF's Destruct()/Release() and its four other
        // AddPrimitivePair overloads (Sphere/Sphere, Sphere/Box, Box/Sphere, Cylinder/Box).
        //
        // Instruction counts and function boundaries were MEASURED this wave on a private .i64
        // copy (scratchpad/waveQ6/ida_addprim/out.json `leaves`), because sub_82814570 has NO
        // per-address export JSON at all -- an export-run gap, not a missing function (AGENTS
        // gotcha 6). Its disassembly came from the same targeted headless-idat run
        // (scratchpad/waveQ6/ida_worldc/out.json `asm_82814570`).
        //
        // ⚠️ GOTCHA 3 AGAIN, and it is why the tag is the THIRD parameter and not the second:
        // the f32 padding rides f1 AND consumes the r5 GPR slot, so the u16 tag arrives in r6.
        // Every one of the three forwards r6 to AddCollisionHeader untouched (they only load r4
        // with the type and r3 with `this`), which is the direct proof of the shape.
        //
        // ⚠️ EACH HAS OTHER MEASURED CALLERS ALREADY WAITING IN THE TREE (xrefs measured this
        // wave). Landing these three UNBLOCKS, but does not by itself complete, the deferred
        // contact-emission blocks in:
        //     AddPrimitive(Box*)      <- DeformableObject::DoBodyPartWorldContactGeneration x2
        //                                (0x8260950C / 0x826096D8) and
        //                                VehicleManager::DoTrafficCarWorldContactGeneration x2
        //                                (0x8261C094 / 0x8261C11C)
        //     AddPrimitive(Cylinder*) <- DeformableObject::DoDetachedWheelWorldContactGeneration
        //                                (0x82609A60)
        //     AddPrimitive(Capsule*)  <- no caller other than the switch
        // ==========================================================================================
        void AddPrimitive(CgsGeometric::Box* lpBox, f32 lfPadding, u16 lu16PrimitiveTag);       // DWARF :89
        void AddPrimitive(CgsGeometric::Capsule* lpCapsule, f32 lfPadding, u16 lu16PrimitiveTag);// DWARF :101
        void AddPrimitive(CgsGeometric::Cylinder* lpCylinder, f32 lfPadding, u16 lu16PrimitiveTag);// DWARF :107

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
        // two-type header stamp used by the pair helpers.
        // ⚠️ STALE COMMENT CORRECTED 2026-08-19 (wave Q6, cluster `addprim`): this line used
        // to say "declared for coherence, defined in its own reconstruction". It is DEFINED
        // in the sibling .cpp -- it has been since 2026-08-06, when the LNK2019 from this very
        // claim is what caused it to be bodied. Believing the old wording would produce a
        // second definition and an LNK2005 in a mounted TU (bat:867).
        void AddCollisionHeader(EVolumeType lePrimTypeA, EVolumeType lePrimTypeB,
                                f32 lfPadding, u16 lu16PrimitiveTagA, u16 lu16PrimitiveTagB);

        // AllocateMemory @ 0x82812098 — bump allocator over mpaDataStream. Returns the
        // current write cursor (mpaDataStream + mu16UsedData) and advances mu16UsedData by
        // liBytes. Asserts the stream exists and the capacity is not exceeded.
        void* AllocateMemory(s32 liBytes);
    };
}
}
