#pragma once

// =================================================================================================
// CgsSceneManager::CgsCollision::LineWithTriangleListStreamJobDesc -- the descriptor for the
// VEHICLE TRACTION LINE test, plus the two fixed-stride records that flow through its stream.
//
// This is the job every wheel's ground contact comes from: one downward line per wheel, tested
// against the triangles the triangle cache holds, harvested by
// VehicleManager::ReadRaceCarTractionLineTestResults -> RaceCarPhysics::AddTractionPoint ->
// Wheel::SetRoadContact -> Wheel::mRoadContact.mbIsOnGround.
//
// The class name is NOT inferred. PS3 DWARF spells the nested result type in the mangle of
// VehicleManagerDebugComponent::RecordStuckInCollisionLineTestResult @0x6B1DA0:
//   ..._ZN10BrnPhysics7Vehicle28VehicleManagerDebugComponent36RecordStuckInCollisionLineTestResultE
//      PKN15CgsSceneManager12CgsCollision33LineWithTriangleListStreamJobDesc12StreamResultE
// and the descriptor's own Prepare is PS3 @0xB084E0
//   CollisionJobDescription::Prepare(this, E_COLLISION_TYPE_LINE_TRIANGLE_LIST /*16*/,
//                                    nullptr, 0.0f, nullptr);  *this = lpStreamProducer;
//
// The X360 build INLINES that Prepare into BaseCollisionGenerator::RunLineWithTriangleListStream
// @0x82810E80, and the inlined stores are the layout proof (batch+0x3D0 is the descriptor base):
//   0x82810F54  stw  r20, 976(r11)   -> +0x00  mpStreamProducer
//   0x82810F4C  stw  r25, 1216(r11)  -> +0xF0  mpResultsList = 0     (CollisionJobDescription)
//   0x82810F44  stfs f31, 1220(r11)  -> +0xF4  mfRadius      = 0.0f  (flt_82001CC0)
//   0x82810F50  stw  r25, 1224(r11)  -> +0xF8  mpDebugStream = 0 (null reader)
//   0x82810F48  stb  r21, 1231(r11)  -> +0xFF  muJobType     = 16
// and ContactGeneratorJob::Execute reads the type back with `lbz r11, 0xFF(r4)` -- +0xFF is
// exactly batch+0x4CF minus batch+0x3D0, so the two directions agree.
//
// ⭐⭐ FIVE LINES PER COMMAND, NOT FOUR -- and that one number closes every gap in both records.
// A previous decode of these two records left `command +0x40..0x4F`, `command +0x90..0x9F`,
// `result +0xB0..0xB3` and `result +0xB8..0xBF` as unattested/unread. They are none of those:
// every array here has FIVE slots. Four independent witnesses:
//   1. 176 == 5*16 + 5*16 + 4 + 4 + 4, rounded up to 16. Exact, and it puts maLineEnd at the
//      +0x50 stride the worker actually indexes with (`addi r16, r18, 0x50` @0x82921AC8).
//   2. 192 == 5*16 + 5*16 + 5*4 + 5*1, rounded up to 16. Exact, and it puts the per-line hit
//      flags at +0xB4, which is where both consumers read them.
//   3. The worker's own stack scratch is five vectors per array -- IDA's frame for
//      ExecuteLineWithTriangleListStream @0x82921968 gives `v99[80]` (best-t), `v100[16]+v101[64]`
//      (tag) and `v102[16]+v103[64]` (normal); 80 == 5*16.
//   4. ⭐ PhysicalTrafficManager::AddTrafficTractionLineTests @0x8261D580 writes
//      `stw r10, 0xA8(cmd)` with **5** (its pseudocode `*(_R30 + 168) = 5`), against 4 in
//      VehicleManager::AddRaceCarTractionLineTests @0x825E9640 and in
//      AddPlayerStuckInCollisionLineTests @0x825E9B28. A TRAFFIC car posts five lines. The fifth
//      slot is used by shipped code, so it is a slot, not padding.
//
// ⚠️⚠️ THE COMMAND RECORD'S SIZE IS A CONTRACT, NOT A LAYOUT. SimpleDataStreamProducer is
// constructed with miCommandSize = 176 and miResultSize = 192 (CgsCollisionGenerator_LineStream
// .cpp), and the poster strides the shared buffer by those numbers. On x64 mpTriangles widens
// 4 -> 8, and the record still comes to exactly 176 (160 + 8 + 4 + 4, 16-aligned) -- checked by
// the static_asserts below rather than assumed. The two s32 counts consequently sit at host
// +0xA8/+0xAC instead of the console's +0xA4/+0xA8; that is invisible because the producer
// (AddRaceCarTractionLineTests) and the consumer (ContactGeneratorJob) both reach them BY NAME.
// If a future wave ever adds a member here, re-check the two static_asserts first: growing past
// 176/192 silently corrupts every command after the first.
// =================================================================================================

#include "types.hpp"

#include <cstddef>            // offsetof (the layout gates at the foot of this header)

#include "BrnCommonTypes.h"   // Vector4 (rw::math::vpu::Vector4)
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"

namespace CgsGeometric { struct Triangle4; }
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct LineWithTriangleListStreamJobDesc : public CollisionJobDescription
    {
        // The per-command line capacity, settled four ways in the banner above.
        static const s32 KI_MAX_LINES_PER_COMMAND = 5;

        // One posted batch of line tests. Written by VehicleManager::AddRaceCarTractionLineTests
        // @0x825E9640 / PhysicalTrafficManager::AddTrafficTractionLineTests @0x8261D580 /
        // VehicleManager::AddPlayerStuckInCollisionLineTests @0x825E9B28 into the producer's
        // command buffer; read by ContactGeneratorJob::ExecuteLineWithTriangleListStream.
        //
        // ⚠️ miNumTriangleBatches counts Triangle4 BLOCKS OF FOUR, not triangles: the worker's
        // outer loop strides mpTriangles by sizeof(Triangle4) == 0xE0 once per decrement
        // (`0x8292212C addi r10, r10, 0xE0` / `0x82922128 addi r20, r20, -1`). The console's own
        // field name would read as a triangle count; it is not one, and the name here says so.
        struct alignas(16) StreamCommand
        {
            Vector4 maLineStart[KI_MAX_LINES_PER_COMMAND];  // +0x00
            Vector4 maLineEnd[KI_MAX_LINES_PER_COMMAND];    // +0x50
            const CgsGeometric::Triangle4* mpTriangles;     // +0xA0 (console 4 bytes, host 8)
            s32     miNumTriangleBatches;                   // console +0xA4
            s32     miNumLines;                             // console +0xA8
        };

        // One harvested answer, one slot per line. Read by
        // VehicleManager::ReadRaceCarTractionLineTestResults @0x82618058 (already landed) and by
        // PS3's VehicleManagerDebugComponent::RecordStuckInCollisionLineTestResult @0x6B1DA0,
        // whose `lbz r0, 0xB4(r9)` / `lvx v0, 0, r10` pin the hit flag and the position.
        //
        // ⚠️ EVERY SLOT IS CLEARED EVEN ON A MISS (0x82922144..0x82922190 stores a zero vector to
        // position and to normal and a zero word to the tag before the hit test). A port that
        // skips the clear inherits the previous command's answer -- the exact silent-drop shape
        // this project keeps paying for -- so the clear is unconditional here too.
        struct alignas(16) StreamResult
        {
            Vector4 maHitPosition[KI_MAX_LINES_PER_COMMAND];  // +0x00
            Vector4 maHitNormal[KI_MAX_LINES_PER_COMMAND];    // +0x50
            u32     mauSurfaceTag[KI_MAX_LINES_PER_COMMAND];  // +0xA0
            u8      mabHit[KI_MAX_LINES_PER_COMMAND];         // +0xB4
        };

        // X360 +0x00, the only member of the derived part.
        CgsMemory::SimpleDataStreamProducer* mpStreamProducer;

        CgsMemory::SimpleDataStreamProducer* GetStreamProducer() const { return mpStreamProducer; }

        // PS3 @0xB084E0. X360 inlines this into RunLineWithTriangleListStream (the five stores
        // quoted in the banner), so it carries no X360 symbol; it is named here so the dispatcher
        // writes MEMBERS rather than the console's byte offsets.
        void Prepare(CgsMemory::SimpleDataStreamProducer* lpStreamProducer)
        {
            mpStreamProducer = lpStreamProducer;

            mpResultsList = NULL;   // 0x82810F4C  stw  r25, 0x4C0
            mfRadius      = 0.0f;   // 0x82810F44  stfs f31, 0x4C4  (flt_82001CC0 == 0.0f)
            mpDebugStream = 0;   // (was misnamed miStatus; DWARF h:119)      // 0x82810F50  stw  r25, 0x4C8
            muJobType     = static_cast<u8>(E_COLLISIONJOB_LINE_WITH_TRIANGLE_LIST_STREAM);
        }
    };

    // The two size contracts the producer is constructed with. Both are GATES, not comments:
    // CreateLineWithTriangleListStream hands GetRequiredBufferSizes / Construct the literals 176
    // and 192, and the poster strides the shared buffer by them.
    static_assert(sizeof(LineWithTriangleListStreamJobDesc::StreamCommand) == 176,
                  "StreamCommand must stay the producer's miCommandSize (176) after x64 widening");
    static_assert(sizeof(LineWithTriangleListStreamJobDesc::StreamResult) == 192,
                  "StreamResult must stay the producer's miResultSize (192)");
    static_assert(offsetof(LineWithTriangleListStreamJobDesc::StreamCommand, maLineEnd) == 0x50,
                  "maLineEnd @ +0x50 -- the stride the worker indexes with (addi r16, r18, 0x50)");
    static_assert(offsetof(LineWithTriangleListStreamJobDesc::StreamCommand, mpTriangles) == 0xA0,
                  "mpTriangles @ +0xA0");
    static_assert(offsetof(LineWithTriangleListStreamJobDesc::StreamResult, maHitNormal) == 0x50,
                  "maHitNormal @ +0x50");
    static_assert(offsetof(LineWithTriangleListStreamJobDesc::StreamResult, mauSurfaceTag) == 0xA0,
                  "mauSurfaceTag @ +0xA0");
    static_assert(offsetof(LineWithTriangleListStreamJobDesc::StreamResult, mabHit) == 0xB4,
                  "mabHit @ +0xB4 -- where BOTH consumers read the per-line hit flag");
}
}
