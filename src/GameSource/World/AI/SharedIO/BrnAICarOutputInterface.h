#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h
// ============================================================================
// Real home (DWARF + X360 assert strings, e.g.
// "..\\..\\..\\GameSource\\World/AI/SharedIO/BrnAICarOutputInterface.h") of
// BrnAI::AIModuleIO::AICarOutputInterface -- the per-frame AI output the AI
// module publishes for the rest of the game (the player route, plus per-race-car
// distance-to-checkpoint and AI-section snapshots). Embedded BY VALUE in the
// PostWorldInputBuffer of both BrnGameState and BrnAI module-IO, and dereferenced
// by BrnGameState::ScoringSystem::UpdateRacePositions to read each car's
// distance-to-checkpoint (GetAICarDistanceToCheckpoint).
//
// Layout SHAPE from DecFIGS DWARF (BrnAICarOutputInterface.h:47-96). Member byte
// offsets verified against the X360 spine:
//   GetAISectionIndex            @0x8230F888  -> *(2*(idx+2636)+this)  => mauAISections @ +5272 (u16 stride 2)
//   SetAICarDistanceToCheckpoint @0x827644F0  -> *&this[idx+1283] = f  => mafDistanceToCheckpoint @ +5132 (f32 stride 4)
//   UpdateRacePositions read     @0x8232A668  -> *(4*(idx+1283)+iface) => GetAICarDistanceToCheckpoint(idx)
// mafDistanceToCheckpoint @ +5132 ends at +5272 (== mauAISections start), so the
// leading by-value Route member is exactly 5132 bytes on the X360 spine -- which
// the DWARF Route layout (RouteNode[320]=5120 + miNodeCount + miDefaultStartNode
// + Status enum) reproduces exactly.
//
// MINIMAL SLICE: the leading `Route mPlayerRoute` is an own-TU type whose real
// home (GameSource/World/AI/Route/BrnRoute.h) is not yet reconstructed. The AICar
// accessors only ever return &mPlayerRoute (const Route*), never touch its
// internals, so a complete byte-exact-sized Route slice keeps mafDistanceToChe-
// ckpoint / mauAISections pinned at their true offsets. Replace the slice with
// the real BrnRoute.h type (without moving anything) when that TU lands.

#include "types.hpp" // s32/u16/u8/f32
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

namespace BrnWorld
{
    // The out-of-range AI-car table cap the AICarOutputInterface arrays are sized
    // by; the X360 asserts spell the name "BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS"
    // and gate the index with `< 35`. Homed here additively (no committed home yet).
    const s32 KI_MAX_OUT_OF_RANGE_RACE_CARS = 35;
}

namespace BrnAI
{
namespace AIModuleIO
{
    // ------------------------------------------------------------------------
    // Minimal complete slice of BrnAI::Route (DWARF BrnRoute.h:141). Real home
    // GameSource/World/AI/Route/BrnRoute.h (not yet reconstructed). Byte-exact at
    // 5132 so the by-value member below pins the AICar arrays to their X360
    // offsets; AICarOutputInterface only returns &mPlayerRoute by pointer.
    // ------------------------------------------------------------------------
    struct AICarOutputInterfaceRouteSlice
    {
        u8 maOpaque[5132]; // NOMINAL contents -- exact size; full layout grown by BrnRoute.h's own TU
    };

    // ========================================================================
    // BrnAI::AIModuleIO::AICarOutputInterface  (DWARF BrnAICarOutputInterface.h:47)
    // ========================================================================
    struct AICarOutputInterface
    {
        // X360 0x827644F0 ("liAICarIndex >= 0 && liAICarIndex < KI_MAX_OUT_OF_RANGE_RACE_CARS"
        //                  + "The lfDistToRouteEnd is less than 0! Mental!"). Self-contained.
        void SetAICarDistanceToCheckpoint(s32 liAICarIndex, f32 lfDistanceToCheckpoint)
        {
            CGS_ASSERT(liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                       "liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
            CGS_ASSERT(lfDistanceToCheckpoint >= 0.0f,
                       "The lfDistToRouteEnd  is less than 0! Mental!");
            mafDistanceToCheckpoint[liAICarIndex] = lfDistanceToCheckpoint;
        }

        // X360 (companion getter; read by UpdateRacePositions @0x8232A668 as
        // *(4*(idx+1283)+iface), with the same index-bound assert). Self-contained.
        f32 GetAICarDistanceToCheckpoint(s32 liAICarIndex) const
        {
            CGS_ASSERT(liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                       "liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
            return mafDistanceToCheckpoint[liAICarIndex];
        }

        // X360 0x8230F888 ("liAICarIndex >= 0 && liAICarIndex < KI_MAX_OUT_OF_RANGE_RACE_CARS",
        //                  return *(2*(idx+2636)+this)). Self-contained.
        u16 GetAISectionIndex(s32 liAICarIndex) const
        {
            CGS_ASSERT(liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                       "liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
            return mauAISections[liAICarIndex];
        }

        // Companion setter (DWARF BrnAICarOutputInterface.h:71). Self-contained.
        void SetAISectionIndex(s32 liAICarIndex, u16 luAISectionIndex)
        {
            CGS_ASSERT(liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS,
                       "liAICarIndex >= 0 && liAICarIndex < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS");
            mauAISections[liAICarIndex] = luAISectionIndex;
        }

        // Player-route accessors (DWARF BrnAICarOutputInterface.h:74-89). Return the
        // embedded route by pointer; the Route slice is opaque-but-complete here.
        const AICarOutputInterfaceRouteSlice* GetPlayerRoute() const { return &mPlayerRoute; }
        s32  GetPlayerRouteNodeIndex() const { return miPlayerRouteNodeIndex; }
        void SetPlayerInShortcut(bool lbInShortcut) { mbPlayerIsInShortcut = lbInShortcut; }
        bool IsPlayerInShortcut() const { return mbPlayerIsInShortcut; }

        // ---- DECLARE-ONLY (bodies live in this type's own TU, not self-contained) ----
        void Construct();
        void SetPlayerRoute(const AICarOutputInterfaceRouteSlice* lpPlayerRoute, s32 liPlayerRouteNodeIndex);

    private:
        // --- data members at the DWARF-attested order; offsets X360-verified -----
        AICarOutputInterfaceRouteSlice mPlayerRoute;                                  // +0x0000 (5132B)
        f32                            mafDistanceToCheckpoint[BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS]; // +5132 (35*4)
        u16                            mauAISections[BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS];          // +5272 (35*2)
        s32                            miPlayerRouteNodeIndex;                                // +5344
        bool                           mbPlayerIsInShortcut;                                  // +5348
    };
}
}
