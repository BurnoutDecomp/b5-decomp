#include "SDKs/EATech/rwcollision/volume_debug_access.h"   // rw::collision::Volume (the real home)

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::collision::Volume::InitializeVTable @ 0x82BB03A8
//   rw::collision::Volume::operator=        @ 0x82BB12D0
// plus the SDK-side declarations of the six per-type Volume descriptor RECORDS the first
// of those installs (X360 .rdata unk_82F91740 / unk_82F918C0 / unk_82F919A4 / unk_82F9176C /
// unk_82F91894 / unk_82F919D0, dumped slot by slot 2026-08-18 by waveQ5 rwc3). The records
// themselves are DEFINED in vendor/renderware/collision/VolumeVTables.cpp since 2026-08-19
// (wave Q5 vtbind) -- see the block below for why, and for the measured proof that both
// halves bind to one symbol.
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
//  3. The six `gVolumeHandler_82F91*` symbols became REAL `Volume::VTable` records instead
//     of single zero bytes in SDKs/EATech/AptRenderLinkStubs.cpp (whose six stub lines were
//     deleted in the same change -- both sides were the same owner's, so there was no window
//     in which the tree carried both). They were defined HERE with null method slots on
//     2026-08-18; the next block records where they went and why.
//
// ---------------------------------------------------------------------------------------
// ⭐ WHAT CHANGED 2026-08-19 (wave Q5 vtbind): THE SIX DESCRIPTORS ARE BOUND, AND THEY MOVED.
// ---------------------------------------------------------------------------------------
// Until this date the six records were DEFINED here with all seven method slots NULL, and
// the note below listed four obstacles (a)-(d) to binding them. (d) -- "a TU cannot NAME both
// rw::collision::Volume and any primitive header" -- was the head of that queue and is still
// TRUE **for this half**. What was never measured is the other half: the VENDOR side has no
// such conflict at all. scratchpad/waveQ5/probe_vtbind/measure_coinclude.cpp compiles
// CollisionVolume.hpp + AABBox.hpp + Capsule/Cylinder/Triangle/Aggregate + GPInstance +
// LineSegIntersect in ONE TU with the canonical ship flags: STATUS=pass. The fork is
// one-sided.
//
// So the six records now live in vendor/renderware/collision/VolumeVTables.cpp, where every
// primitive's methods are visible, and this TU keeps what only it can hold: the SDK-side
// declarations, the shared gVolumeVTable array, and InitializeVTable's fill. 34 of the 40
// method slots the shipped image binds are bound; 2 are genuinely 0 in the image; 6 are
// PARKED with per-slot reasons at the record (see that file). Obstacles (a) and (b) are
// SOLVED there by thin per-type adapters with the slot's exact signature -- there is no
// function-pointer cast anywhere -- and (c) is closed: BoxVolume.cpp now carries all four
// Box and all four Sphere bodies except the two LineSegIntersects.
//
// ⚠️ MOUNT: vendor/renderware/collision/VolumeVTables.cpp is a NEW TU and MUST be added to
// tools/build/build_game_exe.bat in the same commit, in the wave-Q5 C1 collision block. If it
// is missed, THIS TU's six references fail to resolve -- six LNK2019s, loud, not silent.
//
// ---------------------------------------------------------------------------------------
// LINK FACTS still open, REPORTED not fixed:
//  * The four obstacles as they stood before this change, kept for the record. (a) and (b)
//    were the reason the slots could not be spelled; they are handled by the adapters in
//    VolumeVTables.cpp, not made to disappear:
//      (a) The canonical slot type is a POINTER TO MEMBER of the 96-byte
//          `rw::collision::Volume` (rwccore.h:1490-1505). All four standalone primitive
//          classes -- vendor/renderware/collision/AggregateVolume.hpp, TriangleVolume.hpp:48,
//          CapsuleVolume.hpp:56, CylinderVolume.hpp:49 -- have no base clause, so
//          `&X::GetBBox` can never convert to one. The tree's slot type is therefore a FREE
//          function pointer with an explicit leading `const Volume*`, which is what the
//          three TU-local descriptor views in that directory already used. MEASURED
//          (scratchpad/waveQ5/probe_vtbind/measure_layout.cpp): on MSVC x64 the two
//          spellings are byte-for-byte the same record, so this is a spelling change, not a
//          layout change.
//      (b) THREE of those four declare the transform parameter as `const Vec4*`
//          (TriangleVolume.hpp:77, CapsuleVolume.hpp:91, CylinderVolume.hpp:72); only
//          AggregateVolume spells `const math::vpu::Matrix44Affine*`. Same pointer:
//          sizeof(Matrix44Affine)==64 == four 16-byte rows, sizeof(Vec4)==16 -- MEASURED,
//          measure_vocab.cpp / measure_vocab2.cpp. One documented adapter converts it.
//      (c) CLOSED. SphereVolume and BoxVolume had no bodies at all; BoxVolume.cpp now has
//          GetBBox / GetBBoxDiag / GetMaximumFeature / CreateGPInstance for both, from the
//          raw asm. ⚠️ TriangleVolume::GetBBoxDiag @0x82BBA110 is STILL an export hole (no
//          identity.json row, no per-address JSON) and its slot is parked, not stubbed.
//      (d) STILL TRUE FOR THIS TU, and that is now a documented property rather than a
//          blocker: this file cannot NAME both `rw::collision::Volume` (which reaches
//          BrnCommonTypes.h's `rw::math::vpu::Vector3`) and any primitive header (which
//          reaches AABBox.hpp -> the EATech `Vector3` class) -- a hard C2011, still
//          reproduced by scratchpad/waveQ5/probe_rwc3/probe_descriptor_blocked.cpp. It is
//          why the records are defined on the other side of the fork.
//  * HOST-WIDTH HAZARD -- CLOSED 2026-08-18, kept for the record because the fix is what
//    makes the enum-in-+0x40 representation below legal. `FixableVolume::FixUp` USED TO
//    store `gVolumeVTable[type]` through a `void**` into the volume record's +0x40 -- 8
//    bytes on x64 into a slot the console sized at 4 -- clobbering +0x44..+0x47, the box
//    half-extent X lane. IT NO LONGER DOES: GameShared/GameClasses/RenderWare/
//    FixableVolume.cpp now reads and writes a `u32*` at +0x40 and FixUp/FixDown are the
//    console's VALIDATION half only (the enum<->pointer swap is the identity on the host),
//    which is exactly the clean fix volume_debug_access.h's banner named -- keep a 4-byte
//    TYPE INDEX at +0x40 and index gVolumeVTable in the accessors. Every reader now
//    recovers the console's pointer as gVolumeVTable[type]. Nothing writes 8 bytes there.
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
        // DECLARED here, DEFINED in vendor/renderware/collision/VolumeVTables.cpp (2026-08-19,
        // wave Q5 vtbind). That TU is the only one that can see all six primitive classes at
        // once, which is what makes the method slots bindable at all; this one is the only one
        // that can hold `gVolumeVTable` in the SDK's own vocabulary. The two halves spell
        // `Volume::VTable` differently and bind to the SAME decorated symbol -- MEASURED with
        // dumpbin /symbols (scratchpad/waveQ5/vtbind/mangle_sdk.cpp + mangle_vendor.cpp):
        //     this half   UNDEF  ?gVolumeHandler_82F91740@collision@rw@@3UVTable@Volume@12@B
        //     vendor half SECT3  ?gVolumeHandler_82F91740@collision@rw@@3UVTable@Volume@12@B
        // MSVC encodes the leaf class key (`U` = struct VTable) and the enclosing SCOPE NAMES,
        // never the enclosing class key and never a member type -- which is exactly why the
        // fork is invisible at link time. The layout identity that makes that safe rather than
        // merely quiet is measured separately (measure_layout.cpp; see the header's VTable
        // banner).
        //
        // `extern const` because a namespace-scope `const` has INTERNAL linkage in C++; the
        // keyword forces external linkage on both halves, exactly as the retired
        // AptRenderLinkStubs.cpp stubs documented.
        //
        // The per-slot X360 addresses, the typeID/name/flags dump and the five parked slots
        // are all recorded at the records themselves in VolumeVTables.cpp -- deliberately not
        // duplicated here, so there is one place to keep true.
        // -------------------------------------------------------------------------------

        extern const Volume::VTable gVolumeHandler_82F91740;   // type 1 SPHERE    (unk_82F91740)
        extern const Volume::VTable gVolumeHandler_82F918C0;   // type 2 CAPSULE   (unk_82F918C0)
        extern const Volume::VTable gVolumeHandler_82F919A4;   // type 3 TRIANGLE  (unk_82F919A4)
        extern const Volume::VTable gVolumeHandler_82F9176C;   // type 4 BOX       (unk_82F9176C)
        extern const Volume::VTable gVolumeHandler_82F91894;   // type 5 CYLINDER  (unk_82F91894)
        extern const Volume::VTable gVolumeHandler_82F919D0;   // type 6 AGGREGATE (unk_82F919D0)

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
// The six descriptors' METHOD slots are BOUND as of the wave-Q5 vtbind change -- see the
// ⭐ block above and vendor/renderware/collision/VolumeVTables.cpp, which defines the
// records: 34 of the 40 image-bound method slots are live, 2 are genuine image zeros, and
// 6 are parked with per-slot reasons at the record. GetType()/name/flags are real.
// ===========================================================================================
