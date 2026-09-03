#ifndef BRN_AI_MODULE_ROUTES_H
#define BRN_AI_MODULE_ROUTES_H

// =================================================================================================
// BrnAIModule_Routes.h -- declarations of the AIModule route legs lane A5 landed as free functions
// (BrnAIModule_Routes.cpp; aiwave 2026-09-03). On the console these are AIModule::UpdateCarRoutes
// @0x827955F0 (DWARF BrnAIModule.h:293) and three inlined queue appends inside AIModule::Update
// @0x8279B478 (0x8279B728..44 / 0x8279B7D4..0x8279B808). Kept as free functions in namespace
// BrnAI::AIModuleRoutes so the header that owns AIModule (lane A1's grows) is not touched;
// fold them back as members when convenient (nothing else changes).
// =================================================================================================

#include "types.hpp"

namespace BrnAI
{
    class  AIModule;
    struct AICar;
    namespace AIModuleIO     { struct InputBuffer; struct OutputBuffer; }
    namespace RouteMapModuleIO { struct InputBuffer; struct OutputBuffer; }

    namespace AIModuleRoutes
    {
        // 0x8279B728..0x8279B744: the AI input's race-route requests -> the transient "Route" input.
        // PRE: lpRouteInputBuffer write-locked, lpInputBuffer read-locked.
        void AppendRaceRouteRequests(RouteMapModuleIO::InputBuffer* lpRouteInputBuffer,
                                     const AIModuleIO::InputBuffer* lpInputBuffer);

        // AIModule::UpdateCarRoutes @0x827955F0: hand each RouteResponse to its AICar.
        // PRE: lpRouteOutputBuffer read-locked.
        void UpdateCarRoutes(AIModule* lpAIModule,
                             AIModuleIO::OutputBuffer* lpOutputBuffer,
                             const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer,
                             const AICar* lpPlayerAICar);

        // 0x8279B7E8..0x8279B800: the "Route" output's responses -> the AI output buffer's queue.
        void AppendRouteResponses(AIModuleIO::OutputBuffer* lpOutputBuffer,
                                  const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer);

        // The 0x8279B7D4..0x8279B808 bracket as one call: LockForRead, UpdateCarRoutes,
        // AppendRouteResponses, UnlockForRead.
        void ProcessRouteResponses(AIModule* lpAIModule,
                                   AIModuleIO::OutputBuffer* lpOutputBuffer,
                                   const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer,
                                   const AICar* lpPlayerAICar);
    }
}

#endif // BRN_AI_MODULE_ROUTES_H
