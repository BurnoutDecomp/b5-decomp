#ifndef BRN_AI_MODULE_ROUTES_H
#define BRN_AI_MODULE_ROUTES_H

// =================================================================================================
// BrnAIModule_Routes.h -- declarations of the two queue appends AIModule::Update @0x8279B478 inlines
// around the route round trip (0x8279B728..44 and 0x8279B7E8..0x8279B800), kept as free functions in
// namespace BrnAI::AIModuleRoutes (BrnAIModule_Routes.cpp; aiwave 2026-09-03). PausedUpdate
// @0x8279A1E0 inlines the same two appends (0x8279A3FC / 0x8279A474).
//
// AIModule::UpdateCarRoutes @0x827955F0 (DWARF BrnAIModule.h:293) is an AIModule MEMBER again since
// 2026-09-22 (crash parity G04-D4/D5): it reads the two player cursors and mRaceBalancingManager.
// The ProcessRouteResponses wrapper that bracketed it is gone -- AIModule::Update now spells the
// console's 0x8279B7D4..0x8279B808 sequence itself.
// =================================================================================================

#include "types.hpp"

namespace BrnAI
{
    namespace AIModuleIO     { struct InputBuffer; struct OutputBuffer; }
    namespace RouteMapModuleIO { struct InputBuffer; struct OutputBuffer; }

    namespace AIModuleRoutes
    {
        // 0x8279B728..0x8279B744: the AI input's race-route requests -> the transient "Route" input.
        // PRE: lpRouteInputBuffer write-locked, lpInputBuffer read-locked.
        void AppendRaceRouteRequests(RouteMapModuleIO::InputBuffer* lpRouteInputBuffer,
                                     const AIModuleIO::InputBuffer* lpInputBuffer);

        // 0x8279B7E8..0x8279B800: the "Route" output's responses -> the AI output buffer's queue.
        // PRE: lpRouteOutputBuffer read-locked, lpOutputBuffer write-locked.
        void AppendRouteResponses(AIModuleIO::OutputBuffer* lpOutputBuffer,
                                  const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer);
    }
}

#endif // BRN_AI_MODULE_ROUTES_H
