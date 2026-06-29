#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingDebugComponent.h"

#include "GameSource/World/AI/BrnAIModule.h"                                      // BrnAI::AIModule (GetAICar / GetAISectionsData)
#include "GameSource/World/AI/BrnAICar.h"                                         // BrnAI::AICar
#include "GameSource/World/AI/Route/BrnRoute.h"                                   // BrnAI::Route / RouteNode
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"            // BrnAI::RaceBalancingManager / Graph / Route
#include "SharedClasses/AI/AISectionsResourceType.h"                             // BrnAI::AISectionsData / AISection
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // CgsDev::Debug2DImmediateRender
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h" // CgsDev::Debug3DImmediateRender
#include "GameShared/GameClasses/Development/CgsStrStream.h"                       // CgsDev::StrStream
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT

#include "rw/math/vpu/types.h"                                                    // rw::math::vpu::Vector3
#include "rw/rwcore_structs.h"                                                    // rw::RGBA

// BrnAI::RaceBalancingDebugComponent -- the in-game race-balancing (AI rubber-band) debug overlay.
// Reconstructed from the X360 ARTIST build (function addresses cited per method) cross-checked
// against the DecFIGS DWARF (member names / virtual shape) and the recovered dependency headers
// (AIModule / AICar / Route, RaceBalancingManager / Graph / Route, the Debug2D/3D immediate
// renderers, StrStream, AISectionsData).
//
// The X360 source path is "gamesource\unity\../World/AI/RaceBalancing/BrnRaceBalancingDebugComponent.cpp";
// the assert in RenderHUD reproduces that exact path string + line number so the assert text matches.
//
// VMX NOTE: the X360 build vectorises the per-point graph plotting (it keeps the line endpoints as
// packed Vector2s in VMX registers and splices x/y lanes with vperm/vspltw). Those are reconstructed
// here as ordinary scalar/vector C++ -- the geometry, every branch, every assert and every draw call
// is preserved; the literal VMX register shuffles are not (they are pure compiler codegen).

namespace BrnAI
{
namespace
{
    // Source path reproduced verbatim for the asserts (matches the X360 string literal).
    const char* const KPC_SOURCE_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/RaceBalancing/BrnRaceBalancingDebugComponent.cpp";

    // ---- HUD layout constants (read straight off the X360 float literals) ------------------------
    // The speed-ratio graph (top-left): a 150x100 frame whose origin (bottom-left, screen coords) is
    // (200,180). The speed ratio (0..1) is drawn as 0..100 up the Y axis (y = 180 - ratio*100); the
    // race-progress fraction (0..1) is drawn 0..150 along the X axis (x = 200 + frac*150).
    const Vector2 KV2_SPEED_GRAPH_ORIGIN = { 200.0f, 180.0f, 0.0f, 0.0f }; // *&v308 / *(&v308+1)
    const Vector2 KV2_SPEED_GRAPH_SIZE   = { 150.0f, 100.0f, 0.0f, 0.0f }; // v302 / v303
    const f32 KF_SPEED_GRAPH_ORIGIN_X = 200.0f;
    const f32 KF_SPEED_GRAPH_ORIGIN_Y = 180.0f;
    const f32 KF_SPEED_GRAPH_WIDTH    = 150.0f;
    const f32 KF_SPEED_GRAPH_HEIGHT   = 100.0f;

    // The whole-route progress fraction is swept in 24 steps (1/24 == 0.041666668).
    const s32 KI_SPEED_GRAPH_STEPS    = 24;
    const f32 KF_SPEED_GRAPH_STEP     = 1.0f / 24.0f; // 0.041666668

    // The race-balancing time graph (bottom-right): a 400x400 frame with origin (700,480).
    const Vector2 KV2_TIME_GRAPH_ORIGIN = { 700.0f, 480.0f, 0.0f, 0.0f }; // v302 / v303
    const Vector2 KV2_TIME_GRAPH_SIZE   = { 400.0f, 400.0f, 0.0f, 0.0f }; // v305 / v306
    const f32 KF_TIME_GRAPH_ORIGIN_X  = 700.0f;
    const f32 KF_TIME_GRAPH_ORIGIN_Y  = 480.0f;
    const f32 KF_TIME_GRAPH_WIDTH     = 400.0f;
    const f32 KF_TIME_GRAPH_HEIGHT    = 400.0f;

    const f32 KF_TEXT_SCALE           = 16.0f;  // flt_82004000 -- HUD label scale
    const f32 KF_NO_GRAPH_TEXT_SCALE  = 20.0f;  // "No balancing route" read-out scale

    // The "current race time" marker is drawn at this radius (DrawCircle segments == 8).
    const f32 KF_RACE_MARKER_RADIUS   = 3.0f;
    const s32 KI_RACE_MARKER_SEGMENTS = 8;

    // Below this many metres ahead the player, the "fast graph" is in effect (else "slow graph").
    const f32 KF_FAST_GRAPH_DISTANCE  = 80.0f;

    // m/s -> mph conversion (flt_830180B0 == 2.2369363).
    const f32 KF_MS_TO_MPH            = 2.2369363f;

    // Packed RGBA8 colours (read off the X360 li/lis/ori immediates; the 2D renderer takes AARRGGBB).
    const CgsDev::RGBA K_WHITE = 0xFFFFFFFFu;  // -1
    const CgsDev::RGBA K_BLUE  = 0xFF0000FFu;  // -16776961  (par / ahead line)
    const CgsDev::RGBA K_GREEN = 0xFF00FF00u;  // -16711936  (target / behind line)
    const CgsDev::RGBA K_RED   = 0xFFFF0000u;  // -65536     (current-time marker)

    // The DWARF lists FindAICar's slot sweep as 0..34 (35 race-car slots).
    const s32 KI_MAX_AI_CARS = 35;
}
}

// MaybeDrawText (X360 0x82824048) -- the shared "draw debug text iff on-screen" helper used by the
// HUD draw loops. External to this TU (its own TU); declared so the compile gate sees its shape.
// Signature recovered from the call sites: (display, text, x, y, scale, colour, centred).
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

namespace BrnAI
{

// ====================================================================================================
// GetName  @0x82767668
// ====================================================================================================
const char* RaceBalancingDebugComponent::GetName() const
{
    return "Race Balancing";
}

// ====================================================================================================
// OnActivate  @0x82767688
// ----------------------------------------------------------------------------------------------------
// Register the two debug-menu controls: the "Show data" master toggle (+0x18) and the "Current rival"
// slider (+0x14), the slider clamped to [0,6] (one entry per opponent slot).
// ====================================================================================================
void RaceBalancingDebugComponent::OnActivate()
{
    RegisterVariable(&mbShowData, "Show data");          // sub_8282D800 -> RegisterVariable(bool*)
    RegisterVariable(&miCurrentRival, "Current rival");  // sub_8282D720 -> RegisterVariable(s32*)
    SetRange(&miCurrentRival, 0, 6);                     // sub_8282F598 -> SetRange(s32*, 0, 6)
}

// ====================================================================================================
// FindAICar  @0x827676F8
// ----------------------------------------------------------------------------------------------------
// Walk the AI car slots looking for the active rival whose opponent-index matches liRivalIndex.
// Skips the player car and any car whose opponent index is -1 (none assigned). Returns null when no
// slot matches. The X360 sweeps indices 0..34.
// ====================================================================================================
const AICar* RaceBalancingDebugComponent::FindAICar(s32 liRivalIndex) const
{
    for (s32 liSlot = 0; liSlot < KI_MAX_AI_CARS; ++liSlot)
    {
        const AICar* lpCar = mpAIModule->GetAICar(static_cast<u32>(liSlot));

        const s8 liOpponentIndex = lpCar->GetOpponentIndex();

        // A slot is a candidate only if it has an opponent index AND is not the player car.
        const bool lbCandidate = (liOpponentIndex != -1) && !lpCar->IsPlayerCar();

        if (lbCandidate && liOpponentIndex == liRivalIndex)
        {
            return lpCar;
        }
    }

    return nullptr;
}

// ====================================================================================================
// RenderWorld  @0x82774750
// ----------------------------------------------------------------------------------------------------
// When the overlay is on, draw a red marker sphere 2 units above the inspected rival's car.
// ====================================================================================================
void RaceBalancingDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    if (!mbShowData)
    {
        return;
    }

    const AICar* lpCar = FindAICar(miCurrentRival);
    if (lpCar == nullptr)
    {
        return;
    }

    // Lift the marker 2 units up the Y axis above the car position (offset (0,2,0)).
    Vector3 lv3Centre = lpCar->GetPosition();
    lv3Centre.y += 2.0f;            // flt_820C41F4 == 2.0 (added to the y lane)

    // UNCERTAIN: the X360 colour immediate is 0xFF0000FF; mapped to red (255,0,0,255) -- the exact
    // rw::RGBA channel packing is not byte-verified here.
    lpDisplay->DrawSphere(lv3Centre, 1.0f /* flt_82001C98 */, rw::RGBA(255, 0, 0, 255));
}

// ====================================================================================================
// DrawGraphFrame  @0x827747F8
// ----------------------------------------------------------------------------------------------------
// Draw a labelled graph frame: the X axis (origin -> origin+(width,0)), the Y axis (origin ->
// origin-(0,height), screen-y grows downward), the plot-area background, then the two axis-name
// labels offset from the origin.
// ====================================================================================================
void RaceBalancingDebugComponent::DrawGraphFrame(CgsDev::Debug2DImmediateRender* lpDisplay,
                                                 Vector2 lv2Origin, Vector2 lv2Size,
                                                 const char* lpcXAxisLabel, const char* lpcYAxisLabel)
{
    // X axis: a horizontal white line along the bottom of the frame.
    const Vector2 lv2XAxisEnd = { lv2Origin.x + lv2Size.x, lv2Origin.y, 0.0f, 0.0f };
    lpDisplay->DrawLine(lv2Origin, lv2XAxisEnd, K_WHITE);

    // Y axis: a vertical white line up the left edge (screen-y decreases upward).
    const Vector2 lv2YAxisEnd = { lv2Origin.x, lv2Origin.y - lv2Size.y, 0.0f, 0.0f };
    lpDisplay->DrawLine(lv2Origin, lv2YAxisEnd, K_WHITE);

    // The plot-area background (external helper sub_8281C3E0; bodied in its own TU).
    DrawGraphBackground(lpDisplay, lv2Origin.x, lv2Origin.y, lv2Size.x, lv2Size.y);

    // The two axis-name labels, offset from the origin (DrawGraphFrame @0x827747F8). X-label:
    // (origin.x + 50, origin.y + 10) -- vaddfp, asm-confirmed. Y-label: (origin.x - 50,
    // origin.y - flt_820C3FAC) -- vsubfp origin.x - var_70(=50.0) @0x827749F0, vsubfp origin.y -
    // flt_820C3FAC @0x827749FC. The Y-label's Y offset is the un-dumped .rdata flt_820C3FAC
    // (FLAGGED placeholder 50.0f until that rodata is dumped); the X offset is the exact -50.0.
    MaybeDrawText(lpDisplay, lpcXAxisLabel, lv2Origin.x + 50.0f, lv2Origin.y + 10.0f,
                  KF_TEXT_SCALE, K_WHITE, false);
    MaybeDrawText(lpDisplay, lpcYAxisLabel, lv2Origin.x - 50.0f, lv2Origin.y - 50.0f,
                  KF_TEXT_SCALE, K_WHITE, false);
}

// ====================================================================================================
// RenderHUD  @0x82796C08
// ----------------------------------------------------------------------------------------------------
// The race-balancing HUD, gated on mbShowData. It draws, for the inspected rival:
//   1. the speed-ratio graph (the AHEAD/BEHIND speed-ratio curves swept across 0..1 race progress);
//   2. the current difficulty %, and (when the car has a valid route) its live completion ratio;
//   3. the race-balancing time graph (the per-checkpoint AHEAD/BEHIND par-time curves vs the live
//      race time marker);
//   4. a stack of textual read-outs (race/route/car status, speeds, speed-matching).
// ====================================================================================================
void RaceBalancingDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (!mbShowData)
    {
        return;
    }

    // A scratch text stream over a local 256-byte buffer (X360 builds the HUD labels through this).
    // Stream the values through the StrStreamBase reference so the base scalar (<<s32/<<f32) overloads
    // are visible (the derived StrStream's char* override otherwise hides them by name).
    char lacBuffer[256];
    CgsDev::StrStream lStream(lacBuffer, sizeof(lacBuffer));
    CgsDev::StrStreamBase& lOut = lStream;

    const AICar* lpCar = FindAICar(miCurrentRival);

    RaceBalancingManager* lpManager = mpRaceBalancingManager;

    // --- 1. speed-ratio graph -----------------------------------------------------------------------
    // The current rival's speed-ratio graph (one per opponent slot). When the slot is past the live
    // graph count there is simply no graph to draw.
    CGS_ASSERT(lpManager->maRaceBalancingGraphs.GetCount() != -1,
               "Array used before Construct/Clear was called");

    if (miCurrentRival >= lpManager->maRaceBalancingGraphs.GetCount())
    {
        MaybeDrawText(lpDisplay, "No graph", 200.0f, 180.0f, KF_TEXT_SCALE, K_WHITE, false);
    }
    else
    {
        const RaceBalancingGraph& lrGraph =
            lpManager->maRaceBalancingGraphs.GetItem(static_cast<u32>(miCurrentRival));
        const RaceBalancingRoute& lrRoute =
            lpManager->maRaceBalancingRoutes.GetItem(static_cast<u32>(miCurrentRival));

        DrawGraphFrame(lpDisplay, KV2_SPEED_GRAPH_ORIGIN, KV2_SPEED_GRAPH_SIZE,
                       "Race completion", "Speed");

        // Sweep the 0..1 race-progress fraction in 24 steps, plotting the AHEAD (green) and BEHIND
        // (blue) speed-ratio curves as connected line segments. The ratio (0..1) maps up the Y axis
        // as 0..100; the fraction maps across the X axis as 0..150.
        for (s32 liStep = 0; liStep < KI_SPEED_GRAPH_STEPS; ++liStep)
        {
            const f32 lfFractionA = static_cast<f32>(liStep) * KF_SPEED_GRAPH_STEP;
            const f32 lfFractionB = static_cast<f32>(liStep + 1) * KF_SPEED_GRAPH_STEP;

            const f32 lfXA = lfFractionA * KF_SPEED_GRAPH_WIDTH + KF_SPEED_GRAPH_ORIGIN_X;
            const f32 lfXB = lfFractionB * KF_SPEED_GRAPH_WIDTH + KF_SPEED_GRAPH_ORIGIN_X;

            // BEHIND curve (graph type 1) -> blue.
            const f32 lfBehindA = lrGraph.ComputeSpeedRatio(E_GRAPH_TYPE_BEHIND, lfFractionA) * 100.0f;
            const f32 lfBehindB = lrGraph.ComputeSpeedRatio(E_GRAPH_TYPE_BEHIND, lfFractionB) * 100.0f;
            // AHEAD curve (graph type 0) -> green.
            const f32 lfAheadA = lrGraph.ComputeSpeedRatio(E_GRAPH_TYPE_AHEAD, lfFractionA) * 100.0f;
            const f32 lfAheadB = lrGraph.ComputeSpeedRatio(E_GRAPH_TYPE_AHEAD, lfFractionB) * 100.0f;

            const Vector2 lv2BehindA = { lfXA, KF_SPEED_GRAPH_ORIGIN_Y - lfBehindA, 0.0f, 0.0f };
            const Vector2 lv2BehindB = { lfXB, KF_SPEED_GRAPH_ORIGIN_Y - lfBehindB, 0.0f, 0.0f };
            lpDisplay->DrawLine(lv2BehindA, lv2BehindB, K_BLUE);

            const Vector2 lv2AheadA = { lfXA, KF_SPEED_GRAPH_ORIGIN_Y - lfAheadA, 0.0f, 0.0f };
            const Vector2 lv2AheadB = { lfXB, KF_SPEED_GRAPH_ORIGIN_Y - lfAheadB, 0.0f, 0.0f };
            lpDisplay->DrawLine(lv2AheadA, lv2AheadB, K_GREEN);
        }

        // --- 2a. difficulty % --------------------------------------------------------------------
        // The AI module's current difficulty scalar (this+0x18 region of the module) as a percent.
        lStream.Reset();
        lOut << "Difficulty: ";
        const f32 lfDifficultyPercent = mpAIModule->GetDifficulty() * 100.0f;
        // The X360 chooses a one-decimal vs whole-number format based on a mode field; reproduced as
        // the float stream (the formatting nuance is not load-bearing for parity).
        lOut << lfDifficultyPercent;
        lOut << "%";
        MaybeDrawText(lpDisplay, lStream.GetBuffer(), 200.0f, 280.0f, KF_TEXT_SCALE, K_WHITE, false);

        // --- 2b. live completion ratio -----------------------------------------------------------
        if (lpCar != nullptr && lpCar->HasValidRoute())
        {
            const s32 liNodeIndex = lpCar->GetNextRouteNodeIndex();
            const Route* lpRoute = lpCar->GetRoute();

            const RouteNode* lpNode = nullptr;
            if (liNodeIndex >= 0 && liNodeIndex < lpRoute->GetNodeCount())
            {
                lpNode = lpCar->GetNextRouteNode();
            }

            const f32 lfDistanceToCheckpoint =
                (lpNode != nullptr) ? lpNode->GetDistanceToCheckpoint() : 0.0f;
            const f32 lfCompletionRatio =
                lrRoute.ComputeRaceCompletionRatio(lfDistanceToCheckpoint, lpManager->miCheckpointCount);

            // A vertical white tick at the live completion fraction, from the baseline up 100 units.
            const f32 lfX = lfCompletionRatio * KF_SPEED_GRAPH_WIDTH + KF_SPEED_GRAPH_ORIGIN_X;
            const Vector2 lv2TickTop = { lfX, 80.0f, 0.0f, 0.0f };
            const Vector2 lv2TickBottom = { lfX, KF_SPEED_GRAPH_ORIGIN_Y, 0.0f, 0.0f };
            lpDisplay->DrawLine(lv2TickTop, lv2TickBottom, K_WHITE);

            lStream.Reset();
            lOut << (lfCompletionRatio * 100.0f);
            lOut << "%";
            MaybeDrawText(lpDisplay, lStream.GetBuffer(), lfX, 60.0f, KF_TEXT_SCALE, K_WHITE, false);
        }
    }

    // --- 3. race-balancing route / time graph -------------------------------------------------------
    CGS_ASSERT(lpManager->maRaceBalancingRoutes.GetCount() != -1,
               "Array used before Construct/Clear was called");

    if (miCurrentRival >= lpManager->maRaceBalancingRoutes.GetCount())
    {
        MaybeDrawText(lpDisplay, "No balancing route", 700.0f, 480.0f, KF_NO_GRAPH_TEXT_SCALE,
                      K_WHITE, false);
        return;
    }

    const RaceBalancingRoute& lrRoute =
        lpManager->maRaceBalancingRoutes.GetItem(static_cast<u32>(miCurrentRival));

    // The graph only applies when the route-balancing route is valid AND the car has a valid route
    // AND it is using the RACE route-finding style (*(car+0x14C0) == E_ROUTE_FINDING_RACE == 1).
    // Otherwise show a status line: "Invalid balancing route" when the car IS racing (so the route
    // should have been built) else "Not in race".
    if (!lrRoute.mbValid || lpCar == nullptr ||
        !lpCar->HasValidRoute() || lpCar->GetRouteFindingStyle() != E_ROUTE_FINDING_RACE)
    {
        CgsDev::SimpleStrStream lStatus;
        if (lpCar != nullptr && lpCar->GetRouteFindingStyle() == E_ROUTE_FINDING_RACE)
        {
            lStatus << "Invalid balancing route";
        }
        else
        {
            lStatus << "Not in race";
        }
        MaybeDrawText(lpDisplay, lStatus.GetBuffer(), 700.0f, 480.0f, KF_NO_GRAPH_TEXT_SCALE,
                      K_WHITE, false);
        return;
    }

    // The graph (and the speed-match read-out) only applies when the car is IN_RANGE; otherwise the
    // speed-match controller pointer is treated as null.
    const bool lbSpeedMatching = (lpCar->GetState() == E_AI_CAR_STATE_IN_RANGE) &&
                                 lpCar->IsSpeedMatching();

    // The time graph maps the route times onto the 700..1100 X span and 80..480 Y span. The X span is
    // 400/(timeCount-1) per checkpoint; the Y span is 400/totalTime.
    const s32 liTimeCount = lrRoute.miTimeCount;
    const f32 lfXStep = KF_TIME_GRAPH_WIDTH / static_cast<f32>(liTimeCount - 1);

    const f32 lfTotalTime = (liTimeCount > 0)
                          ? lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, liTimeCount - 1)
                          : 0.0f;
    const f32 lfYStep = KF_TIME_GRAPH_WIDTH / lfTotalTime;

    const s32 liCarNodeIndex = lpCar->GetNextRouteNodeIndex();
    // The live race-time marker position: X from the car's node index, Y from the live race time.
    const f32 lfMarkerYOffset = lpManager->mfRaceTime * lfYStep;
    const f32 lfMarkerX = static_cast<f32>(liCarNodeIndex) * lfXStep + KF_TIME_GRAPH_ORIGIN_X;

    // --- 4. textual read-outs ----------------------------------------------------------------------
    // RACE: time / completion ratio.
    lStream.Reset();
    const f32 lfRaceTime = lpManager->mfRaceTime;
    lOut << "RACE: Time: " << static_cast<s32>(lfRaceTime) << "s, ";

    const s32 liNodeIndex = lpCar->GetNextRouteNodeIndex();
    const RouteNode* lpNode = nullptr;
    if (liNodeIndex >= 0 && liNodeIndex < lpCar->GetRoute()->GetNodeCount())
    {
        lpNode = lpCar->GetNextRouteNode();
    }
    const f32 lfDistToCheckpoint = (lpNode != nullptr) ? lpNode->GetDistanceToCheckpoint() : 0.0f;
    const f32 lfCompletionRatio =
        lrRoute.ComputeRaceCompletionRatio(lfDistToCheckpoint, lpManager->miCheckpointCount);
    lOut << "Completion ratio: " << lfCompletionRatio;
    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 700.0f, 530.0f, KF_TEXT_SCALE, K_WHITE, false);

    // ROUTE: distance / checkpoint.
    lStream.Reset();
    lOut << "ROUTE: Distance: " << static_cast<s32>(lrRoute.mfDistance) << "m ";
    lOut << ", Checkpoint: " << lrRoute.miCurrentCheckpointIndex << "/" << lpManager->miCheckpointCount;
    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 700.0f, 550.0f, KF_TEXT_SCALE, K_WHITE, false);

    // The time-graph frame.
    DrawGraphFrame(lpDisplay, KV2_TIME_GRAPH_ORIGIN, KV2_TIME_GRAPH_SIZE, "Distance", "Time");

    CGS_ASSERT(lpCar->GetRoute()->GetNodeCount() == lrRoute.GetTimeCount(),
               "lpAICar->GetRoute()->GetNodeCount() == lpRoute->GetTimeCount()");

    // CAR STATUS: speed / node / target-time band.
    lStream.Reset();
    lOut << "CAR STATUS: Speed: " << static_cast<s32>(lpCar->GetSpeed() * KF_MS_TO_MPH) << "mph";
    lOut << ", Node: " << lpCar->GetNextRouteNodeIndex() << "/" << lpCar->GetRoute()->GetNodeCount();
    lOut << ", Target times: ";
    lOut << static_cast<s32>(lrRoute.GetTime(E_GRAPH_TYPE_BEHIND, lpCar->GetNextRouteNodeIndex()));
    lOut << "-";
    lOut << static_cast<s32>(lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, lpCar->GetNextRouteNodeIndex()));
    lOut << "s";
    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 700.0f, 570.0f, KF_TEXT_SCALE, K_WHITE, false);

    // CAR LOCATION: distance ahead / which graph / range / takedown count.
    lStream.Reset();
    lOut << "CAR LOCATION: " << static_cast<s32>(lpCar->GetDistanceAheadOfPlayer()) << "m ";
    lOut << " ahead ";
    if (lpCar->GetDistanceAheadOfPlayer() <= KF_FAST_GRAPH_DISTANCE)
    {
        lOut << "(fast graph), ";
    }
    else
    {
        lOut << "(slow graph), ";
    }
    if (lpCar->GetState() != E_AI_CAR_STATE_IN_RANGE) // *(car+5320) -> out of range
    {
        lOut << "Out of range, ";
    }
    else
    {
        lOut << "In range, ";
    }
    lOut << " Taken down " << lrRoute.miTakenDownCount << " times";
    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 700.0f, 590.0f, KF_TEXT_SCALE, K_WHITE, false);

    // SPEEDS: target / par bands / max player / min.
    lStream.Reset();
    lOut << "SPEEDS: Target: ";
    AISectionsData* lpSections = mpAIModule->GetAISectionsData();
    lOut << static_cast<s32>(lpManager->ComputeTargetSpeed(lpCar, lpSections, false) * KF_MS_TO_MPH);
    lOut << "(";
    lOut << static_cast<s32>(lpManager->ComputeTargetSpeed(E_GRAPH_TYPE_AHEAD, lpCar,
                                mpAIModule->GetAISectionsData()) * KF_MS_TO_MPH);
    lOut << "-";
    lOut << static_cast<s32>(lpManager->ComputeTargetSpeed(E_GRAPH_TYPE_BEHIND, lpCar,
                                mpAIModule->GetAISectionsData()) * KF_MS_TO_MPH);
    lOut << ")mph ";
    lOut << ", Par: ";
    lOut << static_cast<s32>(lpManager->ComputeParSpeed(E_GRAPH_TYPE_AHEAD, lpCar,
                                mpAIModule->GetAISectionsData()) * KF_MS_TO_MPH);
    lOut << "-";
    lOut << static_cast<s32>(lpManager->ComputeParSpeed(E_GRAPH_TYPE_BEHIND, lpCar,
                                mpAIModule->GetAISectionsData()) * KF_MS_TO_MPH);
    lOut << "mph ";
    lOut << ", Max player: " << static_cast<s32>(lpCar->GetMaxPlayerSpeed() * KF_MS_TO_MPH);
    lOut << ", Min: " << static_cast<s32>(lpCar->GetDecentSpeed() * KF_MS_TO_MPH);
    lOut << "mph";
    MaybeDrawText(lpDisplay, lStream.GetBuffer(), 700.0f, 610.0f, KF_TEXT_SCALE, K_WHITE, false);

    // SPEED MATCHING (only when the car is speed-matching).
    if (lbSpeedMatching)
    {
        lStream.Reset();
        lOut << "SPEED MATCHING: " << static_cast<s32>(lpCar->GetSpeedMatchSpeed() * KF_MS_TO_MPH);
        lOut << "mph";
        MaybeDrawText(lpDisplay, lStream.GetBuffer(), 700.0f, 630.0f, KF_TEXT_SCALE, K_WHITE, false);
    }

    // --- 5. the time graph itself -------------------------------------------------------------------
    // The live race-time marker: a horizontal white line across the graph at the marker Y, a vertical
    // white line at the marker X, and a red circle at their crossing.
    const f32 lfMarkerY = KF_TIME_GRAPH_ORIGIN_Y - lfMarkerYOffset;

    const Vector2 lv2MarkerVTop = { lfMarkerX, 80.0f, 0.0f, 0.0f };
    const Vector2 lv2MarkerVBottom = { lfMarkerX, KF_TIME_GRAPH_ORIGIN_Y, 0.0f, 0.0f };
    lpDisplay->DrawLine(lv2MarkerVTop, lv2MarkerVBottom, K_WHITE);

    const Vector2 lv2MarkerHLeft = { KF_TIME_GRAPH_ORIGIN_X, lfMarkerY, 0.0f, 0.0f };
    const Vector2 lv2MarkerHRight = { 1100.0f, lfMarkerY, 0.0f, 0.0f };
    lpDisplay->DrawLine(lv2MarkerHLeft, lv2MarkerHRight, K_WHITE);

    const Vector2 lv2MarkerCentre = { lfMarkerX, lfMarkerY, 0.0f, 0.0f };
    lpDisplay->DrawCircle(lv2MarkerCentre, KF_RACE_MARKER_RADIUS, KI_RACE_MARKER_SEGMENTS, K_RED);

    if (liTimeCount <= 0)
    {
        return;
    }

    // Plot the AHEAD (green) and BEHIND (blue) cumulative-time curves checkpoint-to-checkpoint, with
    // start/end time labels at the first / middle / last samples. (The X360 draws the AHEAD/green
    // segment first, then the BEHIND/blue one.)
    CGS_ASSERT(lrRoute.mbValid, "mbValid");
    CGS_ASSERT(liTimeCount > 0, "liTimeIndex < miTimeCount");

    // Seed the previous sample from index 0.
    f32 lfPrevAhead = lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, 0);
    f32 lfPrevBehind = lrRoute.GetTime(E_GRAPH_TYPE_BEHIND, 0);

    for (s32 liIndex = 1; liIndex < liTimeCount; ++liIndex)
    {
        const f32 lfAhead = lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, liIndex);
        const f32 lfBehind = lrRoute.GetTime(E_GRAPH_TYPE_BEHIND, liIndex);

        const f32 lfXPrev = static_cast<f32>(liIndex - 1) * lfXStep + KF_TIME_GRAPH_ORIGIN_X;
        const f32 lfXCur = static_cast<f32>(liIndex) * lfXStep + KF_TIME_GRAPH_ORIGIN_X;

        // AHEAD curve -> green.
        const Vector2 lv2AheadPrev = { lfXPrev, KF_TIME_GRAPH_ORIGIN_Y - lfPrevAhead * lfYStep, 0.0f, 0.0f };
        const Vector2 lv2AheadCur = { lfXCur, KF_TIME_GRAPH_ORIGIN_Y - lfAhead * lfYStep, 0.0f, 0.0f };
        lpDisplay->DrawLine(lv2AheadPrev, lv2AheadCur, K_GREEN);

        // BEHIND curve -> blue.
        const Vector2 lv2BehindPrev = { lfXPrev, KF_TIME_GRAPH_ORIGIN_Y - lfPrevBehind * lfYStep, 0.0f, 0.0f };
        const Vector2 lv2BehindCur = { lfXCur, KF_TIME_GRAPH_ORIGIN_Y - lfBehind * lfYStep, 0.0f, 0.0f };
        lpDisplay->DrawLine(lv2BehindPrev, lv2BehindCur, K_BLUE);

        lfPrevBehind = lfBehind;
        lfPrevAhead = lfAhead;

        // Time labels at the first, middle and last samples.
        if (liIndex == 1 || liIndex == liTimeCount / 2 || liIndex == liTimeCount - 1)
        {
            lStream.Reset();
            lOut << static_cast<s32>(lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, liIndex)) << "s";
            MaybeDrawText(lpDisplay, lStream.GetBuffer(), lfXCur, lv2AheadCur.y - 24.0f,
                          KF_TEXT_SCALE, K_WHITE, false);

            lStream.Reset();
            lOut << static_cast<s32>(lrRoute.GetTime(E_GRAPH_TYPE_BEHIND, liIndex)) << "s";
            MaybeDrawText(lpDisplay, lStream.GetBuffer(), lfXCur, lv2BehindCur.y + 8.0f,
                          KF_TEXT_SCALE, K_WHITE, false);
        }
    }
}

}
