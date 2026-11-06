#pragma once

#include "BrnCommonTypes.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationConstants.h"
#include <cstddef>

namespace BrnTraffic
{
    // :156 -- the per-PHYSICAL-traffic-vehicle scratch record: everything the deformation /
    // render side needs for a traffic car that has been promoted to a real physics body.
    // One of these per KU_MAX_PHYSICAL_TRAFFIC_VEHICLES slot (maTrafficPhysicsInfoList).
    //
    // :153/:169 mDetachedPartQueue -- ARTIST DIVERGES FROM THE DWARF HERE (2026-09-02,
    // traffic-deformation wave). DecFIGS spells it `EventQueue<DetachedPartRenderEvent,20>`
    // (an 80-byte-stride event array + length), but every ARTIST access to the span
    // +0x000..+0x520 is the compact per-vehicle record below, and three independent X360
    // sites agree on its layout:
    //   * the writer, ProcessDeformationData @0x8271EA94..0x8271EB10: `lbz r29, 0(info)` as
    //     the slot (asserted < KU_MAX_DETACHED_PARTS_PER_VEHICLE == 20), `stbx partIndex` at
    //     info+1+slot, the event's four transform rows at info+32+64*slot, `stb ++count, 0(info)`;
    //     and its per-frame reset `stb 0, 0(info)` over all 25 records @0x8271E984;
    //   * TrafficPhysicsInfo::Construct @0x82751EB0 `stb 0, 0(r3)` -- the count, not the top
    //     byte of a 32-bit mpEvents as an earlier wave read it;
    //   * the reader, RenderTrafficCar pseudocode :1319-:1345: `if (*info)` count,
    //     `info[1 + n]` part index, `info + 32 + 64*n` the transform.
    // The DWARF type would be 1612 bytes; the span the ARTIST stride leaves it is 1312, which
    // is exactly 32 + 20 * 64. mvRoadTestNormal_HeightAboveRoad follows at +0x520 on both.
    // The record's ARTIST name is not recovered (no symbol touches it); the DWARF typedef
    // name is kept for the member's sake.
    struct TrafficPhysicsInfo
    {
        struct DetachedPartRenderQueue
        {
            u8             muNumParts;                                                            // +0x00
            u8             mau8PartIndex[BrnPhysics::Deformation::KU_MAX_DETACHED_PARTS_PER_VEHICLE]; // +0x01
            // 11 bytes of alignment to the first 16-byte row (Matrix44Affine is 16-aligned).
            Matrix44Affine maTransforms[BrnPhysics::Deformation::KU_MAX_DETACHED_PARTS_PER_VEHICLE]; // +0x20
        };

        // :173 -- KU_MAX_VERLET_POINTS worth of skinning scratch (the 128-point Verlet skin).
        static const u32 KU_NUM_SKINNING_OFFSETS = 128;
        static const u32 KU_NUM_WHEELS           = 4;
        static const u32 KU_MAX_LIGHT_LOCATORS   = 24;
        static const u32 KU_NUM_GLASS_PANES      = 8;

        // DWARF BrnTrafficEntityModule.h:162 -- the bits of muContactSideFlags. X360-attested
        // by both ends: ProcessContactPoint @0x82720C68 ORs in 1 (0x82720E6C `ori r11,r10,1`)
        // or 2 (0x82720EA4 `ori r11,r10,2`), and UpdateVehicleStuckTimers @0x82708D48 tests
        // mask 1 then mask 2 (`li r5,1` / `li r5,2`) against the front / back stuck timers.
        enum EContactSideFlag
        {
            E_CONTACT_SIDE_NONE  = 0,
            E_CONTACT_SIDE_FRONT = 1,
            E_CONTACT_SIDE_BACK  = 2,
        };

        DetachedPartRenderQueue mDetachedPartQueue;                       // :169
        Vector3Plus mvRoadTestNormal_HeightAboveRoad;                     // :171
        Vector3Plus maSkinningOffsets_Scratch[KU_NUM_SKINNING_OFFSETS];   // :173
        Matrix44Affine maWheelTransforms[KU_NUM_WHEELS];                  // :175
        Vector3 maLightLocatorPositions[KU_MAX_LIGHT_LOCATORS];           // :178
        // :179 -- BrnPhysics::Deformation::ETagPointType[24], stored as the enum's underlying
        // value so this header stays out of the deformation include graph.
        //
        // ⭐⭐⭐ WIDTH CORRECTED 2026-09-06 (traffic wave). This was `s32[24]` under a FLAG that
        // said "the width does not change". It DOES: the console stores ONE BYTE per entry, and
        // that made the whole record 80 bytes too long on the host (4192 vs the console's 4112),
        // shifting EVERY member after it by 72. ProcessDeformationData @0x8271DEB0, the only
        // writer, read instruction by instruction:
        //     0x8271E92C  addi   r7, r7, -0x6E40   ; r7 = record + 0x60000 - 0x6E40 = +0x591C0
        //     0x8271E93C  lwz    r6, 0(r9)         ; SOURCE stride 4 (the 32-bit enum)
        //     0x8271E944  addi   r9, r9, 4
        //     0x8271E948  stbx   r6, r7, r8        ; DESTINATION: a BYTE
        //     0x8271E94C  addi   r8, r8, 1         ; DESTINATION stride ONE
        // The array base is `this + 0x58210` (== 360976; 0x82721144 `addis r3,r31,6` +
        // 0x8272114C `addi r3,r3,-0x7DF0`), so 0x591C0 - 0x58210 == 0xFB0 == 4016 is this array
        // and 4016 + 24*1 == 4040 is mabWheelExists -- which BrnTrafficEntityModule_Render.cpp
        // already documents at "info+4040+i", and which puts mfStuckTimeFront back on the
        // +0xFCC (4044) that UpdateVehicleStuckTimers @0x82708D90 pins. With s32 it sat at 4116.
        // The static_asserts in BrnTrafficEntityModule.cpp hold all of this down now.
        u8 maLightTagPointTypes[KU_MAX_LIGHT_LOCATORS];                   // :179 (X360 BYTE)
        bool mabWheelExists[KU_NUM_WHEELS];                               // :181 (X360 +0xFC8)
        f32 mfStuckTimeFront;                                             // :183
        f32 mfStuckTimeBack;                                              // :184
        f32 mfStuckTimerDebounce;                                         // :185
        f32 mfTimeNotDriving;                                             // :187
        f32 mfSteeringDirection;                                          // :190
        f32 mfDrivingDirection;                                           // :191
        s8  miNumLightLocators;                                           // :193
        bool mbIsDeforming;                                               // :194
        bool mbIsFatallyCrashing;                                         // :195
        u8  mu8RenderDamageFlags;                                         // :198
        f32 mafGlassPaneFractureAmounts[KU_NUM_GLASS_PANES];              // :202
        u8  muContactSideFlags;                                           // :204

        // SHIP-ONLY TAIL. The DWARF's TrafficPhysicsInfo ends at muContactSideFlags :204;
        // these two are a merge-window addition ARTIST has and DecFIGS does not, so they carry
        // an X360 attestation instead of a DWARF line: Construct @0x82751E88 closes with
        // `sth r4, 0x100A(r3)`, storing its s32 liIndex argument as a halfword, and Destruct
        // @0x82751EE8 is `li r11,-1 ; sth r11, 0x100A(r3)` and nothing else, which makes 0xFFFF
        // the sentinel and u16 the width. The reader is HandleExternalResponses @0x82732C68.
        // muPad205 is the console's alignment byte, modelled explicitly so the source pins the
        // order of the two ship members rather than a host padding rule.
        u8  muPad205;                 // X360 +0x1009 (alignment; no DWARF)
        u16 muOwningVehicleIndex;     // X360 +0x100A -- SHIP-ONLY, no DWARF

        // The "no owner" sentinel Destruct writes (`li r11,-1` truncated to the halfword).
        static const u16 KU16_NO_OWNING_VEHICLE = 0xFFFFu;

        void Construct(s32 liIndex); // :214
        void Destruct();             // :218
        bool IsStuckFront() const;   // :221
        bool IsStuckBack() const;    // :224
    };

    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo::DetachedPartRenderQueue, maTransforms) == 0x20),
                      "ARTIST detached part transforms");
    CGS_STATIC_ASSERT((sizeof(TrafficPhysicsInfo) == 0x1010), "ARTIST traffic physics stride");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, mvRoadTestNormal_HeightAboveRoad) == 0x520), "ARTIST road test normal");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, maSkinningOffsets_Scratch) == 0x530), "ARTIST skinning offsets");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, maWheelTransforms) == 0xD30), "ARTIST wheel transforms");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, maLightLocatorPositions) == 0xE30), "ARTIST light locator positions");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, maLightTagPointTypes) == 0xFB0), "ARTIST light locator tags");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, mabWheelExists) == 0xFC8), "ARTIST wheel flags");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, miNumLightLocators) == 0xFE4), "ARTIST light locator count");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, mafGlassPaneFractureAmounts) == 0xFE8), "ARTIST glass fracture amounts");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, mbIsDeforming) == 0xFE5), "ARTIST deforming flag");
    CGS_STATIC_ASSERT((offsetof(TrafficPhysicsInfo, mbIsFatallyCrashing) == 0xFE6), "ARTIST fatal crash flag");
}
