#include "SDKs/EATech/rwcollision/volume_debug_access.h"   // rw::collision::Volume (the real home)

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::collision::Volume::InitializeVTable @ 0x82BB03A8
//   rw::collision::Volume::operator=        @ 0x82BB12D0
//
// InitializeVTable lazily fills the shared Volume descriptor table (7 words at
// dword_8327EEE0 -- the SDK's `Volume::VTable *vTableArray[VOLUMETYPENUMINTERNALTYPES]`,
// rwccore.h:1597) with the six per-type descriptor addresses (slot 0 stays null). The
// descriptors themselves are .data records in other TUs; they are referenced here as
// external symbols.
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
// LINK FACTS, reported not fixed (they are outside this file's ownership):
//  * ⚠️ THE BODY BELOW IS DEAD CODE TODAY. `InitializeVTable` should be a STATIC member
//    (rwccore.h:1580, DWARF volume.h:1491, and the X360 body never touches `this`), and
//    both other declarations of this class in the tree spell it static --
//    GameShared/GameClasses/SceneManager/CgsSceneManagerModule.cpp:48, which is the ONLY
//    caller (at :191), and GameSource/World/WorldLinkStubs.cpp:150. Static and non-static
//    are different mangled symbols, so the caller binds to the inert boot gate
//    `int rw::collision::Volume::InitializeVTable() { return 0; }` at
//    WorldLinkStubs.cpp:2573 and never to this. Net effect on the PC build: gVolumeVTable
//    stays ALL ZERO, FixableVolume::FixUp writes NULL into every volume's +0x40 slot, and
//    Volume::GetType/GetBBox/GetBBoxDiag would null-dereference. Fixing it is ONE commit
//    (add `static` here and in volume_debug_access.h, DELETE the WorldLinkStubs gate block
//    :2563-2576) and it is the conductor's call, because doing only half of it is LNK2005
//    in a build two other sessions are using. See the note beside the declaration.
//  * gVolumeVTable's ELEMENT TYPE should be `Volume::VTable*` (DWARF volume.h:1521 spells
//    the array `rw::collision::Volume::VTable *[7] vTableArray`). It is left as `void*`
//    because GameShared/GameClasses/RenderWare/FixableVolume.cpp:20 re-declares it
//    `extern void* gVolumeVTable[7]` in its own TU; changing the type here alone would be
//    a silent cross-TU type mismatch. Change both together.
//  * The six gVolumeHandler_* symbols are NOT the real descriptors today: they resolve to
//    single zero bytes defined as link stubs in SDKs/EATech/AptRenderLinkStubs.cpp:608+
//    ("FLAG link-stub"). So on the PC build every Volume ends up with a descriptor pointer
//    into a zero byte, and any call through Volume::GetBBox / GetBBoxDiag dispatches
//    through a null slot. The real descriptors are console .data records whose contents
//    ARE recoverable -- dumped in the VTable comment in volume_debug_access.h -- but
//    building real descriptors is a separate, MUCH larger job than this bullet used to say.
//    ⚠️ CORRECTED 2026-08-18 (round 3). It used to name only two missing bodies
//    ("SphereVolume::GetBBox @0x82BA8020 and BoxVolume::GetBBox @0x82BA9FC8 have no home"),
//    which reads as 4-of-6 ready. ZERO of the six are usable in the slot type landed in
//    volume_debug_access.h:160-162 today. The real obstacle set, in order:
//      (a) ALL SIX concrete GetBBox/GetBBoxDiag bodies must become members of classes DERIVED
//          from the 96-byte rw::collision::Volume before any of them can be stored in a
//          `Volume::*` descriptor slot. All four homed ones are STANDALONE classes with no
//          base clause -- vendor/renderware/collision/AggregateVolume.hpp:48,
//          TriangleVolume.hpp:48, CapsuleVolume.hpp:56, CylinderVolume.hpp:49 -- so
//          `&X::GetBBox` cannot convert to
//          `RwBool (Volume::*)(const Matrix44Affine*, RwBool, AABBox&) const`.
//      (b) THREE of those four additionally declare the transform parameter as `const Vec4*`
//          (TriangleVolume.hpp:77, CapsuleVolume.hpp:91, CylinderVolume.hpp:72); only
//          AggregateVolume.hpp:54 spells `const math::vpu::Matrix44Affine*`. That has to be
//          reconciled before the signatures match the slot.
//      (c) ONLY THEN do the genuinely un-homed bodies become the remaining work, and it is
//          more than two: SphereVolume::GetBBox @0x82BA8020, SphereVolume::GetBBoxDiag
//          @0x82BA8580, BoxVolume::GetBBox @0x82BA9FC8, BoxVolume::GetBBoxDiag @0x82BA8890
//          (all four are identity.json rows with primary_file null), plus the
//          GetMaximumFeature / CreateGPInstance / LineSegIntersect slots the same dump names.
//          ⚠️ TriangleVolume::GetBBoxDiag @0x82BBA110 appears in the IDA descriptor dump but
//          has NO identity.json row -- an export hole. Declare it; never body it from the
//          .i64 label alone.
//  * TWO TU-LOCAL DUPLICATE MODELS of the descriptor this file just homed, and they model the
//    slots INCOMPATIBLY (reported per gotcha 7; both are TU-local, so no link-time ODR break):
//    vendor/renderware/collision/VolumeBBoxQuery.cpp:68 `struct VolumeVTableView` and
//    PrimitiveIntersect.cpp:795 `struct VolumeVTable` both spell the slots as FREE function
//    pointers with an explicit `const Volume*` first parameter, while volume_debug_access.h
//    :160-162 spells them as pointers-to-MEMBER. On the console both are the same 4-byte code
//    address and both work on the host, but a real descriptor table can only be initialised
//    one way -- whoever builds it must pick one and retire the other. (PrimitiveIntersect's
//    view additionally binds itself to the OTHER, 128-byte Volume fork in CollisionVolume.hpp.)
// ---------------------------------------------------------------------------------------

namespace rw
{
    namespace collision
    {
        // The six per-type Volume descriptors (defined in other TUs; see the link note).
        extern const u8 gVolumeHandler_82F91740;   // type 1 SPHERE
        extern const u8 gVolumeHandler_82F918C0;   // type 2 CAPSULE
        extern const u8 gVolumeHandler_82F919A4;   // type 3 TRIANGLE
        extern const u8 gVolumeHandler_82F9176C;   // type 4 BOX
        extern const u8 gVolumeHandler_82F91894;   // type 5 CYLINDER
        extern const u8 gVolumeHandler_82F919D0;   // type 6 AGGREGATE

        // Shared Volume descriptor table (dword_8327EEE0 .. dword_8327EEF8) -- the SDK's
        // Volume::vTableArray.
        void* gVolumeVTable[7];

        // Store order preserved from the X360 body (0, 6, 1, 2, 3, 4, 5).
        RwBool Volume::InitializeVTable()
        {
            gVolumeVTable[0] = 0;
            gVolumeVTable[6] = const_cast<u8*>(&gVolumeHandler_82F919D0);
            gVolumeVTable[1] = const_cast<u8*>(&gVolumeHandler_82F91740);
            gVolumeVTable[2] = const_cast<u8*>(&gVolumeHandler_82F918C0);
            gVolumeVTable[3] = const_cast<u8*>(&gVolumeHandler_82F919A4);
            gVolumeVTable[4] = const_cast<u8*>(&gVolumeHandler_82F9176C);
            gVolumeVTable[5] = const_cast<u8*>(&gVolumeHandler_82F91894);
            return 1;
        }

        Volume& Volume::operator=(const Volume& lrSource)
        {
            memcpy(maPayload, lrSource.maPayload, sizeof(maPayload));
            return *this;
        }
    }
}
