#pragma once

#include <cstddef>                                                   // offsetof (layout pins)
#include "types.hpp"
#include "BrnCommonTypes.h"                                           // Matrix44Affine / Vector3
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // VolumeInstanceId

// CgsSceneManager::VolumeInstance - one live collision-volume placement: the
// world transform, its owning entity/volume indices and the cached-triangle
// window. Member names/order verbatim from the DecFIGS DWARF
// (CgsVolumeInstance.h:22); X360 sizeof 128 (the EntityManager's
// ObjectPool<VolumeInstance,5048> stride).
//
// ===========================================================================
// RECORD LAYOUT -- PINNED (wave Q5 / cluster A2, 2026-08-18)
//
// The scene-collision middle reads this record by name from four different
// owners (EntityManager, SceneManagerModule::UpdateCollisionBody,
// SceneSweeper::BuildCollidingPairs, BridgeOverlapGenerationToOverlapCulling),
// so the field roles are pinned here once from the X360 ARTIST stores rather
// than re-derived per caller.
//
//   X360 byte offset (DOCUMENTATION ONLY -- pointers do not widen in this
//   record, but the project rule is semantic parity by NAME; nothing in the
//   PC tree may key on these numbers):
//
//     +0x00  mWorldSpaceTransform         4x lvx128/stvx128 at 0/0x10/0x20/0x30
//                                         (EntityManager::AddVolumeInstance
//                                          @0x828CD8A4.., SetVolumeInstanceTransform
//                                          @0x828CD5F4..)
//     +0x40  mPadding                     stvx128 v127,r3,0x40  (SetVolumePadding
//                                         @0x828BA16C); zeroed by AddVolumeInstance
//     +0x50  mUserID                      std r27,0x50(r31)     (8 bytes -- the whole
//                                         packed VolumeInstanceId)
//     +0x58  mfCacheSphereRadius          (cache manager; never touched by the
//                                          EntityManager bodies)
//     +0x5C  miVolumeIndex                stw r24,0x5C(r31)     -- the VolumeManager
//                                         pool index the culler feeds to
//                                         VolumeManager::GetRwVolume
//     +0x60  miNextEntityVolumeInstance   stw -1,0x60(r31) then relinked by
//                                         AddVolumeInstanceToEntity / unlinked by
//                                         RemoveVolumeInstanceFromEntity @0x828BA180
//     +0x64  mu16EntityIndex              sth r26,0x64(r31)     -- the owning
//                                         EntityManager entity-pool slot
//     +0x66  mu16FirstCachedTriIndex      (triangle cache window)
//     +0x68  mu16NumCachedTris            (triangle cache window)
//     +0x6A  mx8Flags                     lbz/stb at 0x6A -- see the bit map below
//
// ---------------------------------------------------------------------------
// THE FLAG BITS. Values are DWARF-attested at namespace scope
// (CgsVolumeInstance.h:27/28) and X360-attested at three independent sites:
//
//   bit 0 (0x01) COLLIDABLE  EntityManager::SetVolumeForCollision @0x828CD69C
//                            `lbz r11,0x6A ; ori r11,r11,1 ; stb` (true arm) and
//                            @0x828CD6AC `clrrwi r11,r11,1` (false arm); identical
//                            pair in SetVolumeForCollisionByIndex @0x828B9EB4/0x828B9ED4.
//                            READ BY SceneManagerModule::UpdateCollisionBody
//                            @0x828C7590 `lbz r11,0x6A(r31) ; clrrwi r11,r11,31 ;
//                            cmplwi ; beq -> return` -- i.e. `if (!IsCollidable())
//                            return;` is the gate on re-posting a body to the sweeper.
//   bit 1 (0x02) CACHED      the cache manager's bit; cleared together with bit 0 by
//                            AddVolumeInstance's single `clrrwi r7,r7,2` @0x828CD8B4
//                            (clear the low TWO bits == SetCollidable(false) +
//                            SetCached(false), which is exactly the pair DWARF lists
//                            for CgsEntityManager.cpp:189).
//
// ⚠️ CORRECTION TO scratchpad/waveQ5/scene_collision_scout.md (cluster F1, func 52).
// The scout describes BridgeOverlapGenerationToOverlapCulling @0x828BA538 as testing
// "both instances' `+80` for-collision words nonzero". That is TWO errors, and both
// come from trusting Hex-Rays' `if ( *(result + 80) )` over the asm:
//   (a) +80 is DECIMAL 80 == 0x50 == mUserID, the packed VolumeInstanceId -- there is
//       no "for-collision word" at that offset; the for-collision state is mx8Flags
//       bit 0, ten bytes further on.
//   (b) the real test is NOT "nonzero". @0x828BA664..0x828BA67C the body loads BOTH
//       instances' mUserID (`ld r30,0x50` / `ld r11,0x50`), shifts each right by 32 to
//       take its embedded EntityId word (`srdi ; clrlwi` x2), compares them
//       (`cmplw cr6,r10,r11`) and SKIPS the AddEvent when they are EQUAL. It is the
//       SELF-PAIR REJECT: two volumes of the same entity never reach the narrow phase.
//       A "nonzero" gate passes every pair (mUserID is never 0 for a live instance),
//       so implementing the scout's reading would feed car-vs-its-own-volumes pairs
//       straight into DoPairQuery.
// Nothing in the PC tree may poke either field by offset -- both are named members here.
// ===========================================================================
namespace CgsSceneManager
{
    // DWARF CgsVolumeInstance.h:27/28 (namespace scope, const uint8_t).
    const u8 KU8_VOLUME_INSTANCE_FLAG_COLLIDABLE = 1;
    const u8 KU8_VOLUME_INSTANCE_FLAG_CACHED     = 2;

    struct VolumeInstance
    {
        Matrix44Affine   mWorldSpaceTransform;       // h:48 (X360 +0x00)
        Vector3          mPadding;                   // h:49 (X360 +0x40; DWARF's own name)
        VolumeInstanceId mUserID;                    // h:50 (X360 +0x50)
        f32              mfCacheSphereRadius;        // h:51 (X360 +0x58)
        s32              miVolumeIndex;              // h:52 (X360 +0x5C)
        s32              miNextEntityVolumeInstance; // h:53 (X360 +0x60)
        u16              mu16EntityIndex;            // h:54 (X360 +0x64)
        u16              mu16FirstCachedTriIndex;    // h:55 (X360 +0x66)
        u16              mu16NumCachedTris;          // h:56 (X360 +0x68)
        u8               mx8Flags;                   // h:57 (X360 +0x6A)

        // DWARF h:72-83. HEADER INLINES: neither progress/status.json nor
        // progress/identity.json carries a `VolumeInstance::` row, and every X360
        // user shows the one-instruction splice folded in place (the ori/clrrwi
        // pairs quoted in the banner), so these have no out-of-line symbol to
        // collide with.
        bool IsCached() const
        {
            return (mx8Flags & KU8_VOLUME_INSTANCE_FLAG_CACHED) != 0;
        }

        void SetCached(bool lbCached)
        {
            if (lbCached)
            {
                mx8Flags |= KU8_VOLUME_INSTANCE_FLAG_CACHED;
            }
            else
            {
                mx8Flags &= static_cast<u8>(~KU8_VOLUME_INSTANCE_FLAG_CACHED);
            }
        }

        bool IsCollidable() const
        {
            return (mx8Flags & KU8_VOLUME_INSTANCE_FLAG_COLLIDABLE) != 0;
        }

        void SetCollidable(bool lbCollidable)
        {
            if (lbCollidable)
            {
                mx8Flags |= KU8_VOLUME_INSTANCE_FLAG_COLLIDABLE;
            }
            else
            {
                mx8Flags &= static_cast<u8>(~KU8_VOLUME_INSTANCE_FLAG_COLLIDABLE);
            }
        }
    };

    // ---- layout pins -------------------------------------------------------
    // HOST-PORTABLE ORDER + WIDTH pins only. Deliberately NOT the X360 byte
    // offsets: those are console values and pinning them on the x64 host is the
    // exact defect class this campaign keeps shipping. What IS invariant is the
    // DWARF member ORDER and the widths the X360 loads/stores use, so a future
    // edit that reorders or re-widens a field breaks the build instead of
    // silently re-pointing every by-name reader.
    static_assert(offsetof(VolumeInstance, mWorldSpaceTransform)
                    < offsetof(VolumeInstance, mPadding),
                  "VolumeInstance: mWorldSpaceTransform must precede mPadding (DWARF h:48/49)");
    static_assert(offsetof(VolumeInstance, mPadding) < offsetof(VolumeInstance, mUserID),
                  "VolumeInstance: mPadding must precede mUserID (DWARF h:49/50)");
    static_assert(offsetof(VolumeInstance, mUserID) < offsetof(VolumeInstance, mfCacheSphereRadius),
                  "VolumeInstance: mUserID must precede mfCacheSphereRadius (DWARF h:50/51)");
    static_assert(offsetof(VolumeInstance, mfCacheSphereRadius) < offsetof(VolumeInstance, miVolumeIndex),
                  "VolumeInstance: mfCacheSphereRadius must precede miVolumeIndex (DWARF h:51/52)");
    static_assert(offsetof(VolumeInstance, miVolumeIndex) < offsetof(VolumeInstance, miNextEntityVolumeInstance),
                  "VolumeInstance: miVolumeIndex must precede miNextEntityVolumeInstance (DWARF h:52/53)");
    static_assert(offsetof(VolumeInstance, miNextEntityVolumeInstance) < offsetof(VolumeInstance, mu16EntityIndex),
                  "VolumeInstance: miNextEntityVolumeInstance must precede mu16EntityIndex (DWARF h:53/54)");
    static_assert(offsetof(VolumeInstance, mu16EntityIndex) < offsetof(VolumeInstance, mu16FirstCachedTriIndex),
                  "VolumeInstance: mu16EntityIndex must precede mu16FirstCachedTriIndex (DWARF h:54/55)");
    static_assert(offsetof(VolumeInstance, mu16FirstCachedTriIndex) < offsetof(VolumeInstance, mu16NumCachedTris),
                  "VolumeInstance: mu16FirstCachedTriIndex must precede mu16NumCachedTris (DWARF h:55/56)");
    static_assert(offsetof(VolumeInstance, mu16NumCachedTris) < offsetof(VolumeInstance, mx8Flags),
                  "VolumeInstance: mu16NumCachedTris must precede mx8Flags (DWARF h:56/57)");

    // Widths the X360 accesses pin directly (std/stw/sth/stb + the two 16-byte
    // vector stores).
    static_assert(sizeof(VolumeInstance::mUserID) == 8,
                  "VolumeInstance::mUserID is the whole packed 64-bit id (X360 `std r27,0x50`)");
    static_assert(sizeof(VolumeInstance::mWorldSpaceTransform) == 64,
                  "VolumeInstance::mWorldSpaceTransform is 4 x 16-byte lanes (X360 4x stvx128)");
    static_assert(sizeof(VolumeInstance::mPadding) == 16,
                  "VolumeInstance::mPadding is one 16-byte lane (X360 stvx128 v127,r3,0x40)");
    static_assert(sizeof(VolumeInstance::mx8Flags) == 1,
                  "VolumeInstance::mx8Flags is a byte (X360 lbz/stb at +0x6A)");
}
