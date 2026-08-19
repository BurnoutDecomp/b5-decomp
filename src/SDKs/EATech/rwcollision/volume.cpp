#include "SDKs/EATech/rwcollision/volume_debug_access.h"   // rw::collision::Volume (the real home)

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::collision::Volume::InitializeVTable @ 0x82BB03A8
//   rw::collision::Volume::operator=        @ 0x82BB12D0
// plus the six per-type Volume descriptor RECORDS the first of those installs
// (X360 .rdata unk_82F91740 / unk_82F918C0 / unk_82F919A4 / unk_82F9176C /
// unk_82F91894 / unk_82F919D0), dumped slot by slot 2026-08-18 (waveQ5 rwc3).
//
// InitializeVTable fills the shared Volume descriptor table (7 words at
// dword_8327EEE0 -- the SDK's `Volume::VTable *vTableArray[VOLUMETYPENUMINTERNALTYPES]`,
// rwccore.h:1597 / DWARF volume.h:1521) with the six per-type descriptor addresses
// (slot 0 stays null) and returns 1. MEASURED store order and return value:
//     82BB03E8 stw r4 ,+0x00   (0)          82BB03EC stw r10,+0x18  (AGGREGATE)
//     82BB03F0 stw r9 ,+0x04   (SPHERE)     82BB03F4 stw r8 ,+0x08  (CAPSULE)
//     82BB03F8 stw r7 ,+0x0C   (TRIANGLE)   82BB03FC stw r6 ,+0x10  (BOX)
//     82BB0400 stw r5 ,+0x14   (CYLINDER)   82BB03B8 li  r3 ,1      (return)
//
// operator= copies the 96-byte volume payload from the source. The X360 body does the
// first 64 bytes via four VMX vector moves and the remaining 32 bytes word-by-word
// (with some redundant re-stores Hex-Rays surfaced); the net effect is a 96-byte copy.
//
// ---------------------------------------------------------------------------------------
// 2026-08-18 (waveQ2 rwcollision owner): this file used to declare its OWN
// `class Volume { u8 maPayload[96]; }` beside the real one in volume_debug_access.h -- a
// second definition of a type that has a home, i.e. an ODR break the per-TU `cl /c` gate
// cannot see (AGENTS.md "Don't locally redefine ... a type that has a reconstructable
// home"). The local copy is gone; the two members are now declared in that header.
//
// ---------------------------------------------------------------------------------------
// WHAT CHANGED 2026-08-18 (waveQ5 rwc3), and the TWO INTEGRATION EDITS IT REQUIRES
// ---------------------------------------------------------------------------------------
//  1. `InitializeVTable` is now STATIC (rwccore.h:1580-1581, DWARF volume.h:1491, and the
//     X360 body never touches `this`; both other declarations in the tree -- the only
//     caller at CgsSceneManagerModule.cpp:48/:191 and WorldLinkStubs.cpp:150 -- already
//     spelled it static, so this body was DEAD and the caller bound to an inert gate).
//     ⚠️ REQUIRES: delete the boot gate `int rw::collision::Volume::InitializeVTable()
//     { return 0; }` in GameSource/World/WorldLinkStubs.cpp -- block :2414-2427 as of
//     this date (banner through closing brace) -- IN THE SAME COMMIT, else LNK2005.
//  2. `gVolumeVTable`'s element type is now the DWARF's `Volume::VTable*` instead of
//     `void*`.  ⚠️ REQUIRES: GameShared/GameClasses/RenderWare/FixableVolume.cpp:20's
//     private re-declaration `extern void* gVolumeVTable[7];` must become
//     `extern Volume::VTable* gVolumeVTable[7];` (or, better, that file should include
//     this header) IN THE SAME COMMIT. MEASURED with dumpbin: the two spellings mangle to
//     ?gVolumeVTable@collision@rw@@3PAPEAXA and
//     ?gVolumeVTable@collision@rw@@3PAPEAUVTable@Volume@12@A -- different symbols, so
//     leaving :20 alone is an LNK2019 in FixableVolume.obj.
//  3. The six `gVolumeHandler_82F91*` symbols are REAL `Volume::VTable` records now,
//     defined below, instead of single zero bytes in SDKs/EATech/AptRenderLinkStubs.cpp
//     (whose six stub lines were deleted in the same change -- both sides are the same
//     owner's, so there is no window in which the tree carries both).
//
// LINK FACTS still open, REPORTED not fixed:
//  * ⚠️ THE SIX DESCRIPTORS ARE PARTIAL, AND DELIBERATELY SO. Every value that could be
//    grounded is grounded (typeID, name, flags -- all read out of the shipped image); the
//    seven METHOD slots are null with their X360 addresses named beside them, because
//    NONE of them can be bound on the host today. The obstacle set, re-measured this
//    round (it is the waveQ2 owner's list, still true line for line):
//      (a) The slot type is a POINTER TO MEMBER of the 96-byte `rw::collision::Volume`
//          (volume_debug_access.h). All four homed primitive classes are STANDALONE with
//          no base clause -- vendor/renderware/collision/AggregateVolume.hpp:48,
//          TriangleVolume.hpp:48, CapsuleVolume.hpp:56, CylinderVolume.hpp:49 -- so
//          `&X::GetBBox` can never convert to
//          `RwBool (Volume::*)(const Matrix44Affine*, RwBool, AABBox&) const`.
//      (b) THREE of those four declare the transform parameter as `const Vec4*`
//          (TriangleVolume.hpp:77, CapsuleVolume.hpp:91, CylinderVolume.hpp:72); only
//          AggregateVolume.hpp:54 spells `const math::vpu::Matrix44Affine*`.
//      (c) SphereVolume and BoxVolume have NO bodies anywhere in the tree, and it is more
//          than the two GetBBoxes: SphereVolume::GetBBox @0x82BA8020 (both bodies exist in
//          the image and were disassembled this round), SphereVolume::GetBBoxDiag
//          @0x82BA8580, BoxVolume::GetBBox @0x82BA9FC8, BoxVolume::GetBBoxDiag @0x82BA8890,
//          plus every Sphere/Box GetMaximumFeature / CreateGPInstance / LineSegIntersect
//          the dump names. ⚠️ TriangleVolume::GetBBoxDiag @0x82BBA110 appears in the dump
//          but has NO identity.json row -- an export hole; declare it, never body it from
//          the .i64 label alone.
//      (d) STRUCTURAL: a TU cannot even NAME both `rw::collision::Volume` (which reaches
//          BrnCommonTypes.h's vendor-POD `rw::math::vpu::Vector3`) and any of the four
//          primitive headers (which reach AABBox.hpp -> the EATech `Vector3` class) --
//          that is the C2011 duplicate the waveQ2 owner measured (rwvol.owner.md s4).
//          So the descriptor TU that binds the real slots cannot exist until that
//          vocabulary is collapsed. This is the true head of the queue, not (c).
//  * HOST-WIDTH HAZARD, PRE-EXISTING, NOT INTRODUCED HERE. `FixableVolume::FixUp`
//    (FixableVolume.cpp:44) stores `gVolumeVTable[type]` through a `void**` into the
//    volume record's +0x40 -- 8 bytes on x64 into a slot the console sized at 4 -- so it
//    clobbers +0x44..+0x47, the box half-extent X lane. That was already happening (it
//    wrote an 8-byte NULL); with a real table it writes 8 real bytes. The clean host fix
//    is the one named in volume_debug_access.h's banner: keep a 4-byte TYPE INDEX at +0x40
//    and index gVolumeVTable in the accessors -- which needs FixUp, FixDown, the four
//    Initialize bodies and both TU-local descriptor views changed together, so it is NOT a
//    one-file job and was not attempted here. No mounted TU reads GetExtent() today
//    (BrnPropEntityDebugComponent.cpp is not in build_game_exe.bat), so nothing regresses.
//  * TWO TU-LOCAL DUPLICATE MODELS of the descriptor, reported per gotcha 7 (both are
//    TU-local, so no link-time ODR break): vendor/renderware/collision/VolumeBBoxQuery.cpp:68
//    `struct VolumeVTableView` and PrimitiveIntersect.cpp:795 `struct VolumeVTable` both
//    spell the slots as FREE function pointers with an explicit `const Volume*` first
//    parameter, while volume_debug_access.h spells them as pointers-to-MEMBER. On the
//    console both are the same 4-byte code address; a real table can only be initialised
//    one way. (PrimitiveIntersect's view additionally binds to the OTHER, 128-byte Volume
//    fork in CollisionVolume.hpp.)
// ---------------------------------------------------------------------------------------

namespace rw
{
    namespace collision
    {
        // -------------------------------------------------------------------------------
        // The six per-type Volume descriptors -- X360 .rdata records, one per VolumeType.
        //
        // The console's home for each of these is that primitive's OWN TU (the record sits
        // in the same .rdata run as that class's strings); the tree has no SphereVolume.cpp
        // or BoxVolume.cpp, and the four TUs that do exist cannot include this header (see
        // obstacle (d) above), so they are homed here -- beside the table they fill -- and
        // keep the address-suffixed names the rest of the tree already refers to.
        //
        // `extern const` because a namespace-scope `const` has INTERNAL linkage in C++ and
        // these are referenced by name from comments/notes elsewhere and were external
        // before; the keyword forces external linkage, exactly as the retired
        // AptRenderLinkStubs.cpp stubs documented.
        //
        // GROUNDED: typeID, name and flags are the words read out of the image (the table
        // in volume_debug_access.h's VTable comment). NOT GROUNDED, hence null: the seven
        // method slots -- their X360 addresses are named on each line so a future owner can
        // bind them without re-dumping, but binding is impossible today for the reasons in
        // the LINK FACTS block. A null slot is an honest hole: the previous spelling was a
        // single zero BYTE, so `GetType()` read four bytes of whatever followed it.
        // -------------------------------------------------------------------------------

        extern const Volume::VTable gVolumeHandler_82F91740;
        const Volume::VTable gVolumeHandler_82F91740 =        // type 1 SPHERE
        {
            E_VOLUMETYPE_SPHERE,
            0,   // getBBox           X360 0x82BA8020  SphereVolume::GetBBox        (un-homed)
            0,   // getBBoxDiag       X360 0x82BA8580  SphereVolume::GetBBoxDiag    (un-homed)
            0,   // getInterval       X360 0x82BAC980  ICF-folded `li r3,1 ; blr`
            0,   // getMaximumFeature X360 0x82BA81B0                               (un-homed)
            0,   // createGPInstance  X360 0x82BA8100                               (un-homed)
            0,   // lineSegIntersect  X360 0x82BA82C8                               (un-homed)
            0,   // release           X360 0x82AD5078  ICF-folded empty `blr`
            "SphereVolume",
            0
        };

        extern const Volume::VTable gVolumeHandler_82F918C0;
        const Volume::VTable gVolumeHandler_82F918C0 =        // type 2 CAPSULE
        {
            E_VOLUMETYPE_CAPSULE,
            0,   // getBBox           X360 0x82BAF9C8  CapsuleVolume::GetBBox     (homed, unbindable (a)+(b))
            0,   // getBBoxDiag       X360 0x82BAF6A8  CapsuleVolume::GetBBoxDiag (homed, unbindable (a))
            0,   // getInterval       X360 0x82BAC980  ICF-folded `li r3,1 ; blr`
            0,   // getMaximumFeature X360 0x82BAF808                             (un-homed)
            0,   // createGPInstance  X360 0x82BAF6F0                             (un-homed)
            0,   // lineSegIntersect  X360 0x82BAFCF8                             (un-homed)
            0,   // release           X360 0x82AD5078  ICF-folded empty `blr`
            "CapsuleVolume",
            0
        };

        extern const Volume::VTable gVolumeHandler_82F919A4;
        const Volume::VTable gVolumeHandler_82F919A4 =        // type 3 TRIANGLE
        {
            E_VOLUMETYPE_TRIANGLE,
            0,   // getBBox           X360 0x82BBA038  TriangleVolume::GetBBox    (homed, unbindable (a)+(b))
            0,   // getBBoxDiag       X360 0x82BBA110  TriangleVolume::GetBBoxDiag (EXPORT HOLE: no identity.json row)
            0,   // getInterval       X360 0x82BAC980  ICF-folded `li r3,1 ; blr`
            0,   // getMaximumFeature X360 0x82BBACD0                             (un-homed)
            0,   // createGPInstance  X360 0x82BBAA00                             (un-homed)
            0,   // lineSegIntersect  X360 0x82BBB970                             (un-homed)
            0,   // release           X360 0x82AD5078  ICF-folded empty `blr`
            "TriangleVolume",
            0
        };

        extern const Volume::VTable gVolumeHandler_82F9176C;
        const Volume::VTable gVolumeHandler_82F9176C =        // type 4 BOX
        {
            E_VOLUMETYPE_BBOX,
            0,   // getBBox           X360 0x82BA9FC8  BoxVolume::GetBBox         (un-homed)
            0,   // getBBoxDiag       X360 0x82BA8890  BoxVolume::GetBBoxDiag     (un-homed)
            0,   // getInterval       X360 0x82BAC980  ICF-folded `li r3,1 ; blr`
            0,   // getMaximumFeature X360 0x82BA87F0                             (un-homed)
            0,   // createGPInstance  X360 0x82BA92E8                             (un-homed)
            0,   // lineSegIntersect  X360 0x82BA9478                             (un-homed)
            0,   // release           X360 0x82AD5078  ICF-folded empty `blr`
            "BoxVolume",
            0
        };

        extern const Volume::VTable gVolumeHandler_82F91894;
        const Volume::VTable gVolumeHandler_82F91894 =        // type 5 CYLINDER
        {
            E_VOLUMETYPE_CYLINDER,
            0,   // getBBox           X360 0x82BAD490  CylinderVolume::GetBBox    (homed, unbindable (a)+(b))
            0,   // getBBoxDiag       X360 0x82BAC7B8  CylinderVolume::GetBBoxDiag (homed, unbindable (a))
            0,   // getInterval       X360 0x82BAC980  ICF-folded `li r3,1 ; blr`
            0,   // getMaximumFeature X360 0x82BAC988                             (un-homed)
            0,   // createGPInstance  X360 0x82BAC7F8                             (un-homed)
            0,   // lineSegIntersect  X360 0x82BAF688                             (un-homed)
            0,   // release           X360 0x82AD5078  ICF-folded empty `blr`
            "CylinderVolume",
            0
        };

        extern const Volume::VTable gVolumeHandler_82F919D0;
        const Volume::VTable gVolumeHandler_82F919D0 =        // type 6 AGGREGATE
        {
            E_VOLUMETYPE_AGGREGATE,
            0,   // getBBox           X360 0x82BBBA10  AggregateVolume::GetBBox     (homed, unbindable (a))
            0,   // getBBoxDiag       X360 0x82BBBB58  AggregateVolume::GetBBoxDiag (homed, unbindable (a))
            0,   // getInterval       X360 0x00000000  GENUINELY NULL in the image
            0,   // getMaximumFeature X360 0x82B0F1B8  ICF-folded `li r3,0 ; blr`
            0,   // createGPInstance  X360 0x82B0F1B8  ICF-folded `li r3,0 ; blr` (same body)
            0,   // lineSegIntersect  X360 0x00000000  GENUINELY NULL in the image
            0,   // release           X360 0x82AD5078  ICF-folded empty `blr`
            "AggregateVolume",
            0
        };

        // Shared Volume descriptor table (dword_8327EEE0 .. dword_8327EEF8) -- the SDK's
        // Volume::vTableArray. Element type per DWARF volume.h:1521 (see the header note on
        // the mangling consequence for FixableVolume.cpp:20).
        //
        // The image shows all seven words ZERO (`.data`, not `.rdata`) -- that is "not
        // written yet", not a value (AGENTS gotcha 13); InitializeVTable is what writes it.
        Volume::VTable* gVolumeVTable[E_VOLUMETYPE_NUMINTERNALTYPES];

        // Store order preserved from the X360 body (0, 6, 1, 2, 3, 4, 5); returns 1.
        // The const_cast mirrors the console: the descriptors live in read-only data while
        // the SDK's array element type is a non-const `VTable*`.
        RwBool Volume::InitializeVTable()
        {
            gVolumeVTable[0]                      = 0;
            gVolumeVTable[E_VOLUMETYPE_AGGREGATE] = const_cast<Volume::VTable*>(&gVolumeHandler_82F919D0);
            gVolumeVTable[E_VOLUMETYPE_SPHERE]    = const_cast<Volume::VTable*>(&gVolumeHandler_82F91740);
            gVolumeVTable[E_VOLUMETYPE_CAPSULE]   = const_cast<Volume::VTable*>(&gVolumeHandler_82F918C0);
            gVolumeVTable[E_VOLUMETYPE_TRIANGLE]  = const_cast<Volume::VTable*>(&gVolumeHandler_82F919A4);
            gVolumeVTable[E_VOLUMETYPE_BBOX]      = const_cast<Volume::VTable*>(&gVolumeHandler_82F9176C);
            gVolumeVTable[E_VOLUMETYPE_CYLINDER]  = const_cast<Volume::VTable*>(&gVolumeHandler_82F91894);
            return 1;
        }

        Volume& Volume::operator=(const Volume& lrSource)
        {
            memcpy(maPayload, lrSource.maPayload, sizeof(maPayload));
            return *this;
        }
    }
}

// ===========================================================================================
// THE TWO "INIT WORDS" -- dword_8327EEEC / dword_8327EEF4 -- ARE SLOTS OF THE TABLE ABOVE.
// CLOSED 2026-08-18 (wave Q5 integration): the "honest host design" below is landed as
// ONE commit -- FixableVolume::FixUp/FixDown (validation-only on the host), Volume::GetVTable
// (volume_debug_access.h, indexes gVolumeVTable[type]), the four Initialize bodies
// (BoxVolume.cpp ConstructVolume stamps the enum for Box/Sphere; TriangleVolume.cpp:164
// stamps 3; CylinderVolume.cpp:118 stamps 5) and the three TU-local descriptor views
// (VolumeQuery.cpp / VolumeBBoxQuery.cpp / PrimitiveIntersect.cpp via CollisionVolume.hpp's
// GetVolumeDescriptor). The derivation is kept for the record:
//
//   TriangleVolume::Initialize @0x82BB0680   82BB06B4 lwz r10, dword_8327EEEC(r10)
//                                            82BB06DC stw r10, 0x40(r11)
//   CylinderVolume::Initialize @0x82BAD3F0   82BAD420 lwz r10, dword_8327EEF4(r10)
//                                            82BAD43C stw r10, 0x40(r11)
//
// dword_8327EEEC is `gVolumeVTable[3]` (+0x0C) and dword_8327EEF4 is `gVolumeVTable[5]`
// (+0x14) -- the TRIANGLE and CYLINDER slots of the very array defined above, MEASURED
// against InitializeVTable's own stores. The identical shape exists for the other two
// primitives:
//   SphereVolume::Initialize @0x82BA84E8  82BA8518 lwz +0x04 -> gVolumeVTable[1]
//   BoxVolume::BoxVolume     @0x82BAA0F0  82BAA124 lwz +0x10 -> gVolumeVTable[4]
//
// SO THE CONSOLE STORES THE DESCRIPTOR POINTER AT +0x40. On the host that pointer is 8
// bytes in a 4-byte slot (+0x44 is the box half-extent X lane; the record is the 96-byte
// serialised stride pinned at BrnPhysicsPropTypeData.h:201 and measured as i*96 in
// PropManager::GetPropInertia -- it cannot grow), so the host keeps the 4-byte TYPE ENUM
// in the slot and every reader recovers the console pointer as gVolumeVTable[enum].
// The six descriptors' METHOD slots are still null (binding them is the (c) queue in
// scratchpad/waveQ5/rwc3.owner.md); GetType()/name/flags are real.
// ===========================================================================================
