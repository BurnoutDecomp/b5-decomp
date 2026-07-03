#pragma once

// ============================================================================
// BrnRaceCarAIInterfaces.h -- GROWN from the prior 256-byte NOMINAL stubs to the
// real, X360-verified layouts of BrnAI::AIModuleIO::RaceCarAIInterface and
// BrnAI::AIModuleIO::AIRaceCarInterface (each grown by its own 17-/7-function TU).
//
// RaceCarAIInterface (DWARF BrnRaceCarAIInterfaces.h:56, members :193-213): the
// per-active-car AI snapshot -- matrices/velocities/speeds/section indices, nine
// BitArray<8> state flags, a management VariableEventQueue<16384,16> @0x2F8, and
// the player-car data block @0x4310. Embedded by value in OutputBuffer_PreScene.
// Byte offsets are pinned by the asm bit-array index math (each BitArray<8> = 8
// bytes; word index N in the Is* accessors == byte 8*N):
//   maMatrices[8]        Matrix44Affine (64B)  0x000..0x200
//   maVelocities[8]      Vector3 (16B)         0x200..0x280
//   mafSpeeds[8]         float                 0x280..0x2A0
//   mauSectionIndices[8] u16                   0x2A0..0x2B0
//   mInAirBits           BitArray<8>  idx 86   0x2B0
//   mCrashingBits        BitArray<8>  idx 87   0x2B8
//   mShowtimeBits        BitArray<8>  idx 88   0x2C0
//   mOnStartLineBits     BitArray<8>  idx 89   0x2C8
//   mDriftingBits        BitArray<8>  idx 90   0x2D0
//   mRaceCarContactBits  BitArray<8>  idx 91   0x2D8
//   mPlayerContactBits   BitArray<8>  idx 92   0x2E0
//   mSetActiveRaceCars   BitArray<8>  idx 93   0x2E8
//   mFrontRayOccluded    BitArray<8>  idx 94   0x2F0
//   mManagementQueue     VariableEventQueue<16384,16>   0x2F8 (a1+760)
//   mPlayerCarPosition   Vector3               0x4310 (17168)
//   mPlayerCarDirection  Vector3               0x4320 (17184)
//   mePlayerActiveRaceCarIndex EActiveRaceCarIndex 0x4330 (17200)
//   mbPlayerDataSet      bool                  0x4334 (17204)
//   maeActiveRaceCarIndices[35] EActiveRaceCarIndex   0x4338
// Queue-size cross-check: VariableEventQueue<16384,16> = 1(mbIsConstructed)+16384
// (macData)+12(3*s32) = 16397 bytes @0x2F8(=760) => ends 0x4305; mPlayerCarPosition
// @0x4310 is the next 16-aligned slot. Consistent.
//
// AIRaceCarInterface (DWARF BrnRaceCarAIInterfaces.h:229, members :269-277): the
// inactive/out-of-range race-car snapshot the AI module publishes each frame.
// Embedded by value in InputBuffer_PostPhysics.
//   maPositions[35]          @ +0     (35*16 = 560)  Vector3
//   maAts[35]                @ +560   (35*16 = 560)  Vector3
//   mSetRaceCars     <35>    @ +1120  (8)
//   mCanPassThroughTraffic<35>@+1128  (8)
//   mCurrentNodePosition Vec2@ +1136  (16)
//   mNextNodePosition    Vec2@ +1152  (16)
//   total size = 1168 (16-aligned).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                        // Vector2/Vector3/Matrix44Affine (16B SIMD)
#include "GameSource/BurnoutConstants.h"                           // EActiveRaceCarIndex / EGlobalRaceCarIndex
#include "SharedClasses/World/BrnWorldRegion.h"                    // BrnWorld::EDistrict (SetUpOutOfRangeRaceCarEvent)
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"         // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue, CgsModule::Event
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"  // BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS

namespace BrnAI
{
    struct AICar;   // SetPlayerRouteNodePositions arg (full type BrnAICar.h; used only in the .cpp)

    namespace AIModuleIO
    {
        // DWARF BrnRaceCarAIInterfaces.h:284. Event type ids stored as the AddEvent liType.
        enum EEvent
        {
            E_EVENT_ATTACH_AI_CONTROL            = 0,
            E_EVENT_ACTIVATE_RACE_CAR            = 1,
            E_EVENT_DEACTIVATE_RACE_CAR          = 2,
            E_EVENT_DETACH_AI_CONTROL            = 3,
            E_EVENT_PLAYER_TAKEN_OVER            = 4,
            E_EVENT_SET_UP_OUT_OF_RANGE_RACE_CAR = 5,
            E_EVENT_ADD_CAR_TO_MODE              = 6,
            E_EVENT_REMOVE_CAR_FROM_MODE         = 7,
            E_EVENT_COUNT                        = 8
        };

        // ---- Management-queue event records (DWARF BrnRaceCarAIInterfaces.h:311/319/327/334).
        // Each derives from CgsModule::Event; packed by byte image into the queue by
        // VariableEventQueue<16384,16>::AddEvent<EventT>(&event, liType) (liSize == sizeof(EventT)).
        // Only the four this TU constructs are declared here.

        // BrnRaceCarAIInterfaces.h:311
        struct ActivateRaceCarEvent : public CgsModule::Event
        {
            EGlobalRaceCarIndex meGlobalRaceCarIndex;   // :313
            EActiveRaceCarIndex meActiveRaceCarIndex;   // :314
        };

        // BrnRaceCarAIInterfaces.h:319
        struct DeactivateRaceCarEvent : public CgsModule::Event
        {
            EGlobalRaceCarIndex meGlobalRaceCarIndex;   // :321
            bool                mbIsInAMode;            // :322
        };

        // BrnRaceCarAIInterfaces.h:327
        struct DetachAIControlEvent : public CgsModule::Event
        {
            EGlobalRaceCarIndex meGlobalRaceCarIndex;   // :329
        };

        // BrnRaceCarAIInterfaces.h:334
        struct SetUpOutOfRangeRaceCarEvent : public CgsModule::Event
        {
            EGlobalRaceCarIndex meGlobalRaceCarIndex;       // :336
            Vector3             mPosition;                  // :337 (16B SIMD)
            Vector3             mAt;                        // :338 (16B SIMD)
            u16                 muSection;                  // :339
            BrnWorld::EDistrict meDistrict;                 // :340
            u8                  muNumberOfMedalsToUnlock;   // :341
        };

        // ====================================================================
        // BrnAI::AIModuleIO::RaceCarAIInterface  (DWARF BrnRaceCarAIInterfaces.h:56)
        // ====================================================================
        struct alignas(16) RaceCarAIInterface
        {
            // BitArray<8> == CgsContainers::BitArray<8> (single 64-bit field, 8 bytes).
            typedef CgsContainers::BitArray<8u> ActiveRaceCarBitArray;   // DWARF :44

            // ---- interface (this batch's 17 methods; remaining DWARF methods live in sibling TUs) ----
            void ActivateRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex, EActiveRaceCarIndex leActiveRaceCarIndex);   // :79
            void DeactivateRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex, bool lbIsInAMode);                          // :85
            void DetachAIControl(EGlobalRaceCarIndex leGlobalRaceCarIndex);                                              // :90
            void SetPlayerActiveRaceCarData(Vector3 lPosition, Vector3 lDirection, EActiveRaceCarIndex leActiveRaceCarIndex); // :101
            void SetUpOutOfRangeRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex, Vector3 lPosition, Vector3 lAt,
                                        u16 luSection, BrnWorld::EDistrict leDistrict, u8 luNumberOfMedalsToUnlock);     // :130

            Vector3               GetPlayerCarPosition() const;                                    // :170
            Vector3               GetPlayerCarDirection() const;                                   // :171
            EActiveRaceCarIndex   GetPlayerActiveRaceCarIndex() const;                             // :172
            const Matrix44Affine& GetActiveRaceCarMatrix(EActiveRaceCarIndex leActiveRaceCarIndex) const;   // :173
            u16                   GetActiveRaceCarSection(EActiveRaceCarIndex leActiveRaceCarIndex) const;   // :176
            bool                  IsActiveRaceCarInAir(EActiveRaceCarIndex leActiveRaceCarIndex) const;      // :177
            bool                  IsActiveRaceCarCrashing(EActiveRaceCarIndex leActiveRaceCarIndex) const;   // :178
            bool                  IsActiveRaceCarInShowtime(EActiveRaceCarIndex leActiveRaceCarIndex) const; // :179
            bool                  IsActiveRaceCarOnStartLine(EActiveRaceCarIndex leActiveRaceCarIndex) const;// :180
            bool                  IsActiveRaceCarDrifting(EActiveRaceCarIndex leActiveRaceCarIndex) const;   // :181
            bool                  IsActiveRaceCarTouchingAnother(EActiveRaceCarIndex leActiveRaceCarIndex) const; // :182
            bool                  IsActiveRaceCarTouchingPlayer(EActiveRaceCarIndex leActiveRaceCarIndex) const;  // :184

            // ---- data members; byte offsets in the header comment above ----
            Matrix44Affine        maMatrices[8];              // :193  0x000
            Vector3               maVelocities[8];            // :194  0x200
            f32                   mafSpeeds[8];               // :195  0x280
            u16                   mauSectionIndices[8];       // :196  0x2A0

            ActiveRaceCarBitArray mInAirBits;                 // :197  0x2B0
            ActiveRaceCarBitArray mCrashingBits;              // :198  0x2B8
            ActiveRaceCarBitArray mShowtimeBits;              // :199  0x2C0
            ActiveRaceCarBitArray mOnStartLineBits;           // :200  0x2C8
            ActiveRaceCarBitArray mDriftingBits;              // :201  0x2D0
            ActiveRaceCarBitArray mRaceCarContactBits;        // :202  0x2D8
            ActiveRaceCarBitArray mPlayerContactBits;         // :203  0x2E0
            ActiveRaceCarBitArray mSetActiveRaceCars;         // :204  0x2E8
            ActiveRaceCarBitArray mFrontRayOccluded;          // :205  0x2F0

            CgsModule::VariableEventQueue<16384, 16> mManagementQueue;   // :207  0x2F8 (a1+760)

            Vector3               mPlayerCarPosition;         // :209  0x4310
            Vector3               mPlayerCarDirection;        // :210  0x4320
            EActiveRaceCarIndex   mePlayerActiveRaceCarIndex; // :211  0x4330
            bool                  mbPlayerDataSet;            // :212  0x4334
            EActiveRaceCarIndex   maeActiveRaceCarIndices[35];// :213  0x4338
        };

        // ====================================================================
        // BrnAI::AIModuleIO::AIRaceCarInterface  (DWARF BrnRaceCarAIInterfaces.h:229)
        // ====================================================================
        // Inactive/out-of-range race-car data the AI module publishes each frame:
        // per-car world position + facing ("at"), a "was this car updated" bitset,
        // a "may pass through traffic" bitset, and the two player-route node
        // positions used for extrapolation. Embedded by value in InputBuffer_PostPhysics.
        struct alignas(16) AIRaceCarInterface
        {
            // @0x8276D120  Record one inactive car: set its updated bit (asserting it
            // was not already set) and store its position + facing.
            void UpdateInactiveRaceCarData(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                           Vector3 lPosition, Vector3 lAt);

            // @0x8276D360  Record the "can pass through traffic" flag for a car.
            void UpdateAllRaceCarData(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                      bool lbCanPassThroughTraffic);

            // @0x827650B8  Cache the player's current + next route-node positions
            // (planar x,y only) from the player AI car's embedded route.
            void SetPlayerRouteNodePositions(const AICar* lpAICar);

            // @0x822B2DC0  True iff UpdateInactiveRaceCarData has recorded this car.
            bool WasInactiveRaceCarUpdated(s8 liRaceCar) const;

            // @0x822B3010  The recorded world position of an updated inactive car.
            Vector3 GetInactiveRaceCarPosition(s8 liRaceCar) const;

            // @0x822B3178  The recorded facing ("at") of an updated inactive car.
            Vector3 GetInactiveRaceCarAt(s8 liRaceCar) const;

            // @0x822B2EE8  True iff the car is permitted to pass through traffic.
            bool CanPassThroughTraffic(s8 liRaceCar) const;

            // --- data members; byte offsets are X360-verified store-for-store ----
            Vector3                     maPositions[BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS]; // +0     (35*16)
            Vector3                     maAts[BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS];       // +560   (35*16)
            CgsContainers::BitArray<35> mSetRaceCars;                                         // +1120  (8)
            CgsContainers::BitArray<35> mCanPassThroughTraffic;                               // +1128  (8)
            Vector2                     mCurrentNodePosition;                                 // +1136  (16)
            Vector2                     mNextNodePosition;                                    // +1152  (16)
        };
    }
}
