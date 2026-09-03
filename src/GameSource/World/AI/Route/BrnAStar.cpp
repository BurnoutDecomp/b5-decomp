#include "GameSource/World/AI/Route/BrnAStar.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/World/AI/Route/BrnRoute.h"       // BrnAI::Route / RouteNode
#include "GameSource/World/AI/BrnAIPortal.h"          // BrnAI::Portal
#include "GameSource/Math/BrnMathUtils.h"             // BrnMath::Flatten
#include "SharedClasses/World/BrnCollisionTag.h"      // BrnWorld::KI_INVALID_SECTION_INDEX
#include "BrnCommonTypes.h"                           // Vector2 / Vector3 / Vector4

#include <cmath>   // sqrtf / fabsf

// BrnAI::AStar / BrnAI::AStarNodePool -- the .cpp home for the search-engine members
// declared in BrnAStar.h. The three Euclidean leaf heuristics stay inline in the header
// (@0x8276E0D8 / @0x8276E100 / @0x8276E138); everything else is bodied here.
//
// The pathfinder is a classic A* over the AISections *portal* graph: each open-set
// entry (AStarNode) is a portal crossing -- its world (x, z) ground-plane position, the
// section it belongs to (muSectionIndex), the section it links into (muLinkSectionIndex)
// and the portal index within the section (muPortalIndex). g is the accumulated edge
// cost (sum of the chosen distance function between consecutive portals), h is the same
// distance function from the node to the goal end-position. ExtractBestOpenNode pops the
// open node with the lowest f = mfCostWeight * g + h; Compute expands it through its
// section's portals, relaxing/seeding neighbour nodes until the goal section is reached
// or the iteration/node budget is exhausted; BuildRoute back-traces the parent chain.
//
// Every member is accessed by name against the committed BrnAStar.h / BrnRoute.h /
// AISectionsResourceType.h / BrnAIPortal.h layouts (no raw offset casts); the baked
// d:\p4 file/line in the X360 asserts are dropped in favour of CGS_ASSERT's __FILE__/
// __LINE__. PerfMonCpu instrumentation (StartMonitor/StopMonitor + the "A*" /
// "A* Extract best open node" / "A* Find node" AddMonitor handles) is part of the X360
// behaviour and reproduced store-for-store.

namespace BrnAI
{
// ---- CPU-perf monitor handles (X360 file-static dword_*; -1 == unregistered) ----
s32 dword_82F3023C = -1;   // "A*"
s32 dword_8300D530 = -1;   // "A* Extract best open node"
s32 dword_8300D534 = -1;   // "A* Find node"

// ---- search tuning tables / constants -------------------------------------------
// Distance-function dispatch table (off_820C47C8), in AStarDistanceFunction enum order.
// These are the only entries whose values are fully recovered (they are pointers to the
// bodied heuristics, attested by the asm xrefs and the enum/guard range cmpwi ..,5).
const AStar::DistanceFunctionPtr AStar::KAP_DISTANCE_FUNCTIONS[E_ASTAR_DISTANCE_FUNCTION_COUNT] =
{
    &AStar::EuclideanDistance,          // E_ASTAR_DISTANCE_EUCLIDEAN
    &AStar::EuclideanDistanceXBiased,   // E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED
    &AStar::EuclideanDistanceYBiased,   // E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED
    &AStar::ManhattanDistance,          // E_ASTAR_DISTANCE_MANHATTAN
    &AStar::DiagonalDistance,           // E_ASTAR_DISTANCE_DIAGONAL
};

// flt_820C47C0 -- per-quality cost weight. RECOVERED 2026-09-03 (aiwave A5) from the image
// bytes (file offset 0xC47C0 = 3E800000 3F000000, big-endian): {0.25f, 0.5f}. Both < 1, so
// the search is deliberately GREEDY (f = 0.25*g + h at LOW quality, 0.5*g + h at MEDIUM) --
// which is why a race route completes inside the 12-iteration budget on a 7,639-section map.
const f32 AStar::KAF_QUALITY_COST_WEIGHTS[E_ASTAR_QUALITY_COUNT] =
{
    0.25f,  // E_ASTAR_QUALITY_LOW
    0.5f,   // E_ASTAR_QUALITY_MEDIUM
};

// dword_820C4790 -- per-iteration live-node budget. RECOVERED 2026-09-03 from the image bytes
// (file offset 0xC4790, twelve big-endian s32). The pool holds KU_MAX_NODES == 1024 nodes, so
// iterations 0..10 cap the live set at 128,256,...,960 and the LAST iteration (11) is
// unbounded (0x7FFFFFFF) -- the search then runs until the pool itself refuses NewNode
// (E_STATUS_PARTIAL) or the open set empties (E_STATUS_BLOCKED).
const s32 AStar::KAI_ITERATION_NODE_BUDGET[KI_MAX_ITERATIONS] =
{
    128, 256, 384, 512, 576, 640, 704, 768, 832, 896, 960, 0x7FFFFFFF,
};

// Construct's default/sentinel cost weight (flt_820037C8). Hex-Rays resolves the rodata
// float to -1.0 (an "unset" sentinel; Prepare overwrites it from the quality preset).
const f32 AStar::KF_DEFAULT_COST_WEIGHT = -1.0f;

// flt_820C3B70 lane 0 == 0x34000000 == FLT_EPSILON (1.1920929e-07), read from the image
// 2026-09-03. Every VMX user (BuildRoute @0x8277FC6C, RacingLineGenerator::GetForwardPortalIndex
// @0x82781840, the Extrapolate* trio) does `lvlx v,flt_820C3B70 ; vspltw v,v,0` and then
// `vandc(abs) ; vcmpgtfp` per lane -- i.e. RwMath::IsZero(v) == (|x| <= eps && |y| <= eps).
// The previous FLT_MAX "reachability extent" was a misread of that idiom.
const f32 AStar::KF_ZERO_EPSILON = 1.1920929e-07f;

// -------------------------------------------------------------------------------------
// BrnAI::AStarNodePool
// -------------------------------------------------------------------------------------

// BrnAI::AStarNodePool::GetNode @0x82765530.
//
// Returns a pointer to the luNodeIndex'th node in the flat maNodes[] array. The pool
// partitions its 1024-node capacity into KU_PARTITION_COUNT buckets of
// KU_MAX_PARTITION_NODES (256) nodes each; mauNodeCount[partition] tracks how many
// nodes are live in each bucket. The X360 build asserts:
//   - luNodeIndex < KU_MAX_NODES                                    (BrnAStar.h:483)
//   - luNodeIndex % KU_MAX_PARTITION_NODES
//        < mauNodeCount[luNodeIndex / KU_MAX_PARTITION_NODES]       (BrnAStar.h:484)
// then returns &maNodes[luNodeIndex] (asm: r3 = 24 * luNodeIndex + this, i.e. the
// 24-byte AStarNode stride). The baked d:\p4 file/line are dropped in favour of
// __FILE__/__LINE__ by CGS_ASSERT. Called by ExtractBestOpenNode and FindNode.
AStarNode* AStarNodePool::GetNode(u16 luNodeIndex)
{
    CGS_ASSERT(luNodeIndex < KU_MAX_NODES, "luIndex < KU_MAX_NODES");
    CGS_ASSERT(luNodeIndex % KU_MAX_PARTITION_NODES
                   < mauNodeCount[luNodeIndex / KU_MAX_PARTITION_NODES],
               "luIndex % KU_MAX_PARTITION_NODES < mauNodeCount[luIndex / KU_MAX_PARTITION_NODES]");
    return &maNodes[luNodeIndex];
}

// BrnAI::AStarNodePool::NewNode @0x82774A20.
//
// Allocate-and-initialise a fresh node for portal luPortalIndex of section
// luSectionIndex, linking into section luLinkSectionIndex. The pool hashes the node into
// one of KU_PARTITION_COUNT buckets by (luSectionIndex & KU_HASH_MASK); a bucket holds at
// most KU_MAX_PARTITION_NODES nodes and the whole open set is capped at KU_MAX_OPEN_NODES.
// Returns NULL (allocation refused) when either the bucket is full
// (mauNodeCount[bucket] >= KU_MAX_PARTITION_NODES) or the open set is full
// (muOpenNodeCount >= KU_MAX_OPEN_NODES) -- the X360 asm tests the *unsigned* literals
// 0x100 and 0x80 respectively. On success the node's flat index is
// (bucket << 8) + mauNodeCount[bucket]; the node is written {position, cost=0, heuristic=0,
// parent=KU_INVALID_NODE_INDEX, section, link, portal, open=true}, registered into
// mauOpenNodes[muOpenNodeCount], and both the bucket count and the open-node count are
// bumped. (No assert in this function.)
AStarNode* AStarNodePool::NewNode(const AStarVector2& lPosition, u16 luLinkSectionIndex,
                                  u16 luSectionIndex, u8 luPortalIndex)
{
    const u16 luBucket = luSectionIndex & KU_HASH_MASK;
    const u16 luBucketCount = mauNodeCount[luBucket];

    AStarNode* lpNode = nullptr;
    if (luBucketCount < KU_MAX_PARTITION_NODES && muOpenNodeCount < KU_MAX_OPEN_NODES)
    {
        const u16 luNodeIndex = static_cast<u16>((luBucket << 8) + luBucketCount);
        lpNode = &maNodes[luNodeIndex];

        lpNode->Construct(lPosition, luSectionIndex, luLinkSectionIndex, luPortalIndex);

        mauOpenNodes[muOpenNodeCount] = luNodeIndex;
        ++mauNodeCount[luBucket];
        ++muOpenNodeCount;
    }
    return lpNode;
}

// BrnAI::AStarNodePool::ExtractBestOpenNode @0x82767788.
//
// Pop (and close) the open node with the lowest weighted f-score. f is computed as
//   f = mfCost * lfCostWeight + mfHeuristic
// (asm fmadds f31, mfCost, lfCostWeight, mfHeuristic), so lfCostWeight in [0,1] trades
// off greediness (0 => pure heuristic / greedy best-first) against optimality (1 => full
// g+h A*). The open set is the index list mauOpenNodes[0 .. muOpenNodeCount). The routine
// linearly scans every open node (asserting each is still open), tracks the minimum,
// then removes the winner from the open list by swapping in the last open entry
// (compaction) and decrementing muOpenNodeCount, finally clearing the winner's mbOpen.
// *lpuBestNodeIndex receives the winning node's *flat pool index* (mauOpenNodes value),
// which Compute records as the parent index of every node it then expands. Returns the
// winning node (NULL when the open set is empty). PerfMon-wrapped (dword_8300D530).
AStarNode* AStarNodePool::ExtractBestOpenNode(u16* lpuBestNodeIndex, f32 lfCostWeight)
{
    CgsDev::PerfMonCpu::StartMonitor(dword_8300D530);

    CGS_ASSERT(lfCostWeight >= 0.0f && lfCostWeight <= 1.0f,
               "lfCostWeight >= 0.0f && lfCostWeight <= 1.0f");

    AStarNode* lpBestNode = nullptr;
    f32        lfBestScore = 3.4028235e38f;          // FLT_MAX (flt_8204F664)
    u16        luBestOpenNodeIndex = KU_INVALID_NODE_INDEX;

    if (muOpenNodeCount)
    {
        u16 luOpenIndex = 0;
        do
        {
            AStarNode* lpNode = GetNode(mauOpenNodes[luOpenIndex]);
            const f32 lfScore = (lpNode->GetCost() * lfCostWeight) + lpNode->GetHeuristic();
            CGS_ASSERT(lpNode->IsOpen(), "lpNode->IsOpen()");
            if (lfScore < lfBestScore)
            {
                lpBestNode = lpNode;
                lfBestScore = lfScore;
                luBestOpenNodeIndex = luOpenIndex;
            }
            ++luOpenIndex;
        }
        while (luOpenIndex < muOpenNodeCount);

        if (lpBestNode)
        {
            *lpuBestNodeIndex = mauOpenNodes[luBestOpenNodeIndex];
            --muOpenNodeCount;

            CGS_ASSERT(lpBestNode->IsOpen(), "mbOpen");
            lpBestNode->Close();

            // Compact: move the last open entry into the freed slot (only when the
            // winner was not already the last entry).
            if (muOpenNodeCount)
            {
                if (luBestOpenNodeIndex < muOpenNodeCount)
                {
                    const u16 luMovedIndex = mauOpenNodes[muOpenNodeCount];
                    mauOpenNodes[luBestOpenNodeIndex] = luMovedIndex;
                    CGS_ASSERT(GetNode(luMovedIndex)->IsOpen(),
                               "GetNode( mauOpenNodes[luBestOpenNodeIndex] )->IsOpen()");
                }
            }
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(dword_8300D530);
    return lpBestNode;
}

// -------------------------------------------------------------------------------------
// BrnAI::AStar
// -------------------------------------------------------------------------------------

// BrnAI::AStarNodePool::FindNode @0x82767978  (ARTIST EXPORT HOLE -- no JSON; decoded
// 2026-09-03 (aiwave A5) from the 50 image words at file offset 0x767978 with capstone PPC64-BE;
// the only callers are Compute @0x82774F5C and the reset-on-track search).
//
//   r30 = this, r29 = luSectionIndex & 0xFFFF, r26 = luPortalIndex
//   0x82767994  StartMonitor(dword_8300D534)                    ("A* Find node")
//   0x827679A4  r11 = section & 3                               (KU_HASH_MASK bucket)
//   0x827679A8  r31 = (bucket << 8) & 0xFFFF                    (first node of the bucket)
//   0x827679AC  r9  = bucket + 0x3080 ; lhzx r9, r9*2, this     (mauNodeCount[bucket] -- the
//               count array sits at this + 0x6100 == 1024 nodes * 24 + 128 open-list u16s)
//   0x827679C4  r28 = (first + count) & 0xFFFF                  (one past the bucket's live set)
//   0x827679CC  if (first >= end) -> result 0
//   0x827679DC  loop: node = GetNode(this, i)                   (the asserting accessor)
//   0x827679E0    if (node->muSectionIndex(+0x12) == section
//   0x827679EC     && node->muPortalIndex(+0x16) == portal) -> result = node ; break
//   0x82767A14  StopMonitor(dword_8300D534) ; return result
//
// The hash is the SAME `section & KU_HASH_MASK` NewNode buckets on, so a node is found in the
// bucket it was created in. Linear over the bucket's live prefix; NULL when absent.
AStarNode* AStarNodePool::FindNode(u16 luSectionIndex, u8 luPortalIndex)
{
    CgsDev::PerfMonCpu::StartMonitor(dword_8300D534);

    AStarNode* lpResult = nullptr;

    const u16 luBucket     = static_cast<u16>(luSectionIndex & KU_HASH_MASK);
    const u16 luFirstIndex = static_cast<u16>(luBucket << 8);
    const u16 luEndIndex   = static_cast<u16>(luFirstIndex + mauNodeCount[luBucket]);

    for (u16 luNodeIndex = luFirstIndex; luNodeIndex < luEndIndex; ++luNodeIndex)
    {
        AStarNode* lpNode = GetNode(luNodeIndex);
        if (lpNode->GetSectionIndex() == luSectionIndex && lpNode->GetPortalIndex() == luPortalIndex)
        {
            lpResult = lpNode;
            break;
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(dword_8300D534);
    return lpResult;
}

// BrnAI::AStar::Construct @0x82767A40.
//
// Bind the section graph and reset the per-search scratch. Stores lpAISectionsData,
// seeds mfCostWeight from rodata flt_820037C8 (the default cost weight), clears
// mpBestNode / mbInProgress / meRouteStatus / mpDistanceFunction, and -- once, on first
// construction (the dword_82F3023C PerfMon handle still -1) -- registers the three CPU
// performance monitors used by Compute / ExtractBestOpenNode / FindNode.
void AStar::Construct(const AISectionsData* lpAISectionsData)
{
    mpAISectionsData   = lpAISectionsData;
    mfCostWeight       = KF_DEFAULT_COST_WEIGHT;   // flt_820037C8
    mpBestNode         = nullptr;
    mbInProgress       = false;
    meRouteStatus      = Route::E_STATUS_UNINITIALISED;
    mpDistanceFunction = nullptr;

    if (dword_82F3023C == -1)
    {
        dword_82F3023C = CgsDev::PerfMonCpu::AddMonitor("A*", 7, 0, 3.0f, 0, 1);
        dword_8300D530 = CgsDev::PerfMonCpu::AddMonitor("A* Extract best open node",
                                                        7, 0, 2.0f, 0, 1);
        dword_8300D534 = CgsDev::PerfMonCpu::AddMonitor("A* Find node", 7, 0, 2.0f, 0, 1);
    }
}

// BrnAI::AStar::Prepare @0x82774AE8.
//
// Seed a fresh search. Records the start/end positions and section indices, resolves the
// quality preset to a cost-weight (KAF_QUALITY_COST_WEIGHTS[leQuality], rodata
// flt_820C47C0) and the distance heuristic to a function pointer
// (KAP_DISTANCE_FUNCTIONS[leDistanceFunction], rodata off_820C47C8), clears the pool
// bucket counts (mauNodeCount[0..KU_PARTITION_COUNT)) plus muOpenNodeCount and
// miBlockSectionCount, then NewNode's the start node (link == section == start, portal ==
// KU_INVALID_PORTAL_INDEX) as mpBestNode. The start node's g is forced to 0 and its h is
// computed straight away (distance from start to end). Returns true. The X360 asserts the
// quality and distance-function enum ranges and mpBestNode != NULL.
bool AStar::Prepare(const AStarVector2& lStartPosition, const AStarVector2& lEndPosition,
                    u16 luStartSectionIndex, u16 luEndSectionIndex,
                    AStarQuality leQuality, AStarDistanceFunction leDistanceFunction,
                    bool lbUseAIShortcuts)
{
    mStartPosition      = lStartPosition;
    mEndPosition        = lEndPosition;
    mbInProgress        = true;
    muStartSectionIndex = luStartSectionIndex;
    muEndSectionIndex   = luEndSectionIndex;
    mbUseAIShortcuts    = lbUseAIShortcuts;
    miIterationCount    = 0;
    meRouteStatus       = Route::E_STATUS_UNINITIALISED;

    CGS_ASSERT(leQuality >= 0 && leQuality < E_ASTAR_QUALITY_COUNT,
               "leQuality >=0 && leQuality < E_ASTAR_QUALITY_COUNT");
    mfCostWeight = KAF_QUALITY_COST_WEIGHTS[leQuality];

    CGS_ASSERT(leDistanceFunction >= 0 && leDistanceFunction < E_ASTAR_DISTANCE_FUNCTION_COUNT,
               "leDistanceFunction >=0 && leDistanceFunction < E_ASTAR_DISTANCE_FUNCTION_COUNT");
    mpDistanceFunction = KAP_DISTANCE_FUNCTIONS[leDistanceFunction];

    for (u32 luBucket = 0; luBucket < AStarNodePool::KU_PARTITION_COUNT; ++luBucket)
        mAStarNodePool.mauNodeCount[luBucket] = 0;
    mAStarNodePool.muOpenNodeCount = 0;
    miBlockSectionCount = 0;

    mpBestNode = mAStarNodePool.NewNode(lStartPosition, luStartSectionIndex,
                                        luStartSectionIndex, AStarNodePool::KU_INVALID_PORTAL_INDEX);
    CGS_ASSERT(mpBestNode != nullptr, "mpBestNode != NULL");

    mpBestNode->SetCost(0.0f);
    mpBestNode->SetHeuristic(mpDistanceFunction(mpBestNode->GetPosition(), lEndPosition));
    return true;
}

// BrnAI::AStar::IsBlockSection (inlined on the console; recovered from its one expansion in
// Compute @0x82774EC0..0x82774EF4 (aiwave A5, 2026-09-03)):
//   lwz  r11, 0x632C(this)   miBlockSectionCount ; ble -> false
//   addi r10, this, 0x62EC   &maBlockSectionIds[0]
//   loop: lwz r9,0(r10) ; cmpw r9, lpLinkSection->mId (+12) ; beq -> true ; addi r10,4 ; blt loop
// A linear scan of the live prefix of maBlockSectionIds for the section id.
bool AStar::IsBlockSection(u32 lSectionId)
{
    for (s32 liIndex = 0; liIndex < miBlockSectionCount; ++liIndex)
    {
        if (maBlockSectionIds[liIndex] == lSectionId)
        {
            return true;
        }
    }
    return false;
}

// BrnAI::AStar::AddBlockSectionId (inlined on the console; its one expansion is
// RouteMapModule::ProcessRaceRoute @0x8278C418..0x8278C45C):
//   lwz r11, 0x636C(mAStar)  miBlockSectionCount ; cmpwi 0x10 ; blt -> skip the assert
//   FireAssert("miBlockSectionCount < KI_MAX_BLOCK_SECTION_COUNT", BrnAStar.h, 522)
//   stwx id, (count + 0x18CB) * 4, mAStar    == maBlockSectionIds[count]  (0x18CB*4 == 0x632C)
//   miBlockSectionCount++
// NON-GATING on the console (the store runs after the assert). [GUARD] the bail below is the
// PC deviation: the 17th id would overwrite miBlockSectionCount itself on the host too.
void AStar::AddBlockSectionId(u32 lSectionId)
{
    CGS_ASSERT(miBlockSectionCount < KI_MAX_BLOCK_SECTION_COUNT,
               "miBlockSectionCount < KI_MAX_BLOCK_SECTION_COUNT");   // BrnAStar.h:522
    if (miBlockSectionCount >= KI_MAX_BLOCK_SECTION_COUNT)
    {
        return;
    }
    maBlockSectionIds[miBlockSectionCount] = lSectionId;
    ++miBlockSectionCount;
}

// BrnAI::AStar::IsInProgress (inlined on the console as `lbz 0x659D(module)` ==
// RouteMapModule+552+25461 == this->mbInProgress; RouteMapModule::Update @0x8279402C and
// ProcessRaceRoute @0x8278C2FC/0x8278C334/0x8278C470 read it that way).
bool AStar::IsInProgress() const
{
    return mbInProgress;
}

// BrnAI::AStar::Compute @0x82774C90.
//
// One A* search pass (called up to KI_MAX_ITERATIONS times). While the live node budget
// for this iteration (sum of the four pool bucket counts) stays within the per-iteration
// allowance KAI_ITERATION_NODE_BUDGET[miIterationCount] (rodata dword_820C4790):
//   1. Pop the best open node (ExtractBestOpenNode by lowest mfCostWeight*g + h). The
//      out-param luBestNodeIndex is the parent index stamped onto every node expanded
//      from it. Empty open set => meRouteStatus = E_STATUS_BLOCKED, stop.
//   2. If the popped node's OWN section (muSectionIndex) is the goal section =>
//      E_STATUS_COMPLETE, stop.
//   3. Otherwise expand through the popped node's LINK section (muLinkSectionIndex): for
//      each portal of that link section, skip the portal we arrived through (its
//      (x, portalZ) equals the popped node's position), then -- unless
//      the link section is blocked, or the no-go / AI-shortcut flag gates reject it --
//      either relax the existing neighbour node (FindNode) or seed a new one (NewNode):
//        g(neighbour) = g(popped) + distfn(poppedPos, portalPos)
//        h(neighbour) = distfn(neighbourPos, mEndPosition)
//      A NewNode failure (pool exhausted) => E_STATUS_PARTIAL, stop this pass.
// On exit meStatus is set, mbInProgress cleared, and miIterationCount incremented.
// PerfMon-wrapped (dword_82F3023C). Reconstructed from the asm store-for-store.
void AStar::Compute()
{
    CgsDev::PerfMonCpu::StartMonitor(dword_82F3023C);

    CGS_ASSERT(muStartSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX,
               "muStartSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX");
    CGS_ASSERT(muEndSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX,
               "muEndSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX");
    CGS_ASSERT(meRouteStatus == Route::E_STATUS_UNINITIALISED,
               "meRouteStatus == Route::E_STATUS_UNINITIALISED");
    CGS_ASSERT(miIterationCount < KI_MAX_ITERATIONS, "miIterationCount < KI_MAX_ITERATIONS");

    while ((mAStarNodePool.mauNodeCount[0] + mAStarNodePool.mauNodeCount[1]
            + mAStarNodePool.mauNodeCount[2] + mAStarNodePool.mauNodeCount[3])
           <= KAI_ITERATION_NODE_BUDGET[miIterationCount])
    {
        u16 luBestNodeIndex = 0;
        mpBestNode = mAStarNodePool.ExtractBestOpenNode(&luBestNodeIndex, mfCostWeight);
        if (!mpBestNode)
        {
            meRouteStatus = Route::E_STATUS_BLOCKED;
            mbInProgress  = false;
            break;
        }

        if (mpBestNode->GetSectionIndex() == muEndSectionIndex)
        {
            meRouteStatus = Route::E_STATUS_COMPLETE;
            mbInProgress  = false;
            break;
        }

        // Expansion fans out through the best node's LINK section (muLinkSectionIndex,
        // node+0x14) -- the section it crosses INTO -- not its own section (the goal
        // test above used the own section, node+0x12).
        const u32 luSectionIndex = mpBestNode->GetLinkSectionIndex();
        CGS_ASSERT(luSectionIndex < mpAISectionsData->muNumSections,
                   "luSectionIndex < muNumSections");
        const AISection* lpSection = mpAISectionsData->GetAISection(luSectionIndex);

        const u8 luNumPortals = lpSection->mu8NumPortals;
        for (u8 luPortalIndex = 0; luPortalIndex < luNumPortals; ++luPortalIndex)
        {
            const Portal* lpPortal = lpSection->GetPortal(luPortalIndex);

            // The expanded neighbour position is the portal's ground-plane (x, z).
            const AStarVector2 lPortalPosition(lpPortal->GetPositionX(), lpPortal->GetPositionZ());

            // Skip the portal we arrived through (its (x, z) equals the popped node's pos).
            const AStarVector2 lBestPosition = mpBestNode->GetPosition();
            if (lpPortal->GetPositionX() == lBestPosition.X()
                && lpPortal->GetPositionZ() == lBestPosition.Y())
                continue;

            const u16 luLinkSectionIndex = lpPortal->GetLinkSectionIndex();
            CGS_ASSERT(luLinkSectionIndex < mpAISectionsData->muNumSections,
                       "luSectionIndex < muNumSections");
            const AISection* lpLinkSection = mpAISectionsData->GetAISection(luLinkSectionIndex);

            // Reject the portal if the link section is on the block list.
            if (IsBlockSection(lpLinkSection->mId))
                continue;

            // No-go gate: if the source section is NOT a no-go section (flag 0x01 clear)
            // then the link section must not be a no-go section either.
            if ((lpSection->mx8Flags & 0x01) == 0 && (lpLinkSection->mx8Flags & 0x01) != 0)
                continue;

            // AI-shortcut gate: unless shortcuts are allowed (mbUseAIShortcuts), a
            // shortcut link (flag 0x40) may only be taken from a shortcut source section.
            if (!mbUseAIShortcuts
                && (lpSection->mx8Flags & 0x40) == 0
                && (lpLinkSection->mx8Flags & 0x40) != 0)
                continue;

            AStarNode* lpNeighbour = mAStarNodePool.FindNode(static_cast<u16>(luSectionIndex),
                                                             luPortalIndex);
            if (lpNeighbour)
            {
                // Relax an existing (still-open) neighbour if the new g is lower.
                if (lpNeighbour->IsOpen())
                {
                    const AStarVector2 lBestPos = mpBestNode->GetPosition();
                    const f32 lfTentativeG = mpBestNode->GetCost()
                        + mpDistanceFunction(lBestPos, lpNeighbour->GetPosition());
                    if (lfTentativeG < lpNeighbour->GetCost())
                    {
                        lpNeighbour->SetCost(lfTentativeG);
                        lpNeighbour->SetParentIndex(luBestNodeIndex);
                    }
                }
            }
            else
            {
                // Seed a new neighbour node.
                lpNeighbour = mAStarNodePool.NewNode(lPortalPosition, lpPortal->GetLinkSectionIndex(),
                                                     static_cast<u16>(luSectionIndex), luPortalIndex);
                if (!lpNeighbour)
                {
                    mbInProgress  = false;
                    meRouteStatus = Route::E_STATUS_PARTIAL;
                    goto next_iteration;
                }
                lpNeighbour->SetParentIndex(luBestNodeIndex);

                const AStarVector2 lBestPos = mpBestNode->GetPosition();
                lpNeighbour->SetCost(mpBestNode->GetCost()
                                     + mpDistanceFunction(lBestPos, lpNeighbour->GetPosition()));
                lpNeighbour->SetHeuristic(mpDistanceFunction(lpNeighbour->GetPosition(),
                                                             mEndPosition));
            }
        }
    next_iteration:;
    }

    CgsDev::PerfMonCpu::StopMonitor(dword_82F3023C);
    ++miIterationCount;
}

// BrnAI::AStar::BuildRoute (public) @0x8278C210.
//
// Emit the finished search into lpOutRoute. Asserts the search has been run
// (meRouteStatus != E_STATUS_UNINITIALISED) and is no longer in progress (!mbInProgress),
// copies the route status into lpOutRoute (Route::meStatus @0x1408), and:
//   - if the search ended BLOCKED (status == 3) the route is emptied (miNodeCount @0x1400
//     forced to 0) and the worker overload is NOT run;
//   - otherwise it asserts mpBestNode != NULL and back-traces it into the route.
// (The X360 emits this as the unnamed dispatcher sub_8278C210, the sole caller of the
// named worker BuildRoute(AStarNode*, Route*); RouteMapModule::ProcessRaceRoute calls it.)
void AStar::BuildRoute(Route* lpOutRoute)
{
    CGS_ASSERT(meRouteStatus != Route::E_STATUS_UNINITIALISED,
               "meRouteStatus != Route::E_STATUS_UNINITIALISED");
    CGS_ASSERT(!mbInProgress, "!mbInProgress");

    lpOutRoute->meStatus = static_cast<Route::Status>(meRouteStatus);
    if (meRouteStatus == Route::E_STATUS_BLOCKED)
        lpOutRoute->miNodeCount = 0;

    if (meRouteStatus != Route::E_STATUS_BLOCKED)
    {
        CGS_ASSERT(mpBestNode != nullptr, "mpBestNode != NULL");
        BuildRoute(mpBestNode, lpOutRoute);
    }
}

// BrnAI::AStar::BuildRoute @0x8277F930  (worker overload).
//
// Back-trace the parent chain from lpBestNode to the start node and emit the route.
//   1. Walk muParentIndex from lpBestNode until KU_INVALID_NODE_INDEX, collecting each
//      node's {x, y, distance=0, section, portal} into a stack buffer (capacity-asserted
//      against KU_MAX_NODES; the indices are re-validated through GetNode's asserts).
//   2. Emit the collected nodes in reverse (start-side -> goal-side) via Route::AddNode,
//      emitting buffer entries [count-1 .. 1] -- which drops buffer[0] (the goal-side best
//      node); the start node was never buffered, and step 3 re-adds a goal exit node.
//   3. If the route ended up with >= 2 nodes, append one extrapolated exit node: take the
//      normalised travel direction of the final segment (lastNode - prevNode), then over
//      the goal section's portals pick the portal whose flattened direction best aligns
//      with travel (largest dot product, seeded at -2.0), skipping a portal whose offset from
//      the last node is zero (the RwMath::IsZero test against FLT_EPSILON == flt_820C3B70
//      lane 0); add a final node at that portal's (x, z) tagging the
//      BEST NODE'S OWN section index (node+0x12, NOT muEndSectionIndex) and chosen portal
//      index into its w lane.
//
// SIMD note: the X360 vrsqrtefp + two Newton-Raphson refinement steps (vnmsubfp/vmaddfp)
// are the hardware reciprocal-sqrt used to normalise the 2D direction; reconstructed here
// as an exact ordinary normalise. The IsZero epsilon (flt_820C3B70 lane 0) was read from the
// image 2026-09-03 (it is FLT_EPSILON); it only excludes a zero-length offset, never the
// picked direction maths.
void AStar::BuildRoute(AStarNode* lpBestNode, Route* lpOutRoute)
{
    CGS_ASSERT(lpBestNode != nullptr, "lpBestNode != NULL");

    // ---- 1. back-trace into a stack buffer (start..goal not yet reversed) ----
    // The X360 frame holds a KU_MAX_NODES-deep scratch (the assert below bounds the trace
    // at KU_MAX_NODES); Route::AddNode applies its own 320-node cap when emitting.
    Vector4 laTraced[AStarNodePool::KU_MAX_NODES];
    s32 liNodeCount = 0;

    // Each iteration buffers the current node then steps to its parent; the loop exits at
    // the start node (parent == KU_INVALID_NODE_INDEX), so the start node is never buffered.
    // Buffer order is goal-side first: [best, parent, ..., node-just-after-start].
    AStarNode* lpNode = lpBestNode;
    while (lpNode->GetParentIndex() != AStarNodePool::KU_INVALID_NODE_INDEX)
    {
        CGS_ASSERT(liNodeCount < AStarNodePool::KU_MAX_NODES,
                   "liNodeCount < AStarNodePool::KU_MAX_NODES");

        const AStarVector2 lPosition = lpNode->GetPosition();
        Vector4& lrSlot = laTraced[liNodeCount];
        lrSlot.x = lPosition.X();
        lrSlot.y = lPosition.Y();
        lrSlot.z = 0.0f;
        lrSlot.w = 0.0f;
        // Pack the section index (low u16 of w) and portal index (next byte) -- the X360
        // stores muSectionIndex as a halfword and muPortalIndex as a byte into the w lane.
        reinterpret_cast<u16*>(&lrSlot.w)[0] = lpNode->GetSectionIndex();
        reinterpret_cast<u8*>(&lrSlot.w)[2]  = lpNode->GetPortalIndex();

        ++liNodeCount;
        lpNode = mAStarNodePool.GetNode(lpNode->GetParentIndex());
    }

    // ---- 2. emit entries [count-1 .. 1] (reverse = start-side -> goal-side), which drops
    //         buffer[0] (the goal-side best node itself); step 3 re-adds a goal exit node.
    for (s32 li = liNodeCount - 1; li >= 1; --li)
        lpOutRoute->AddNode(laTraced[li]);

    // ---- 3. extrapolated exit node (only when the route has >= 2 nodes) ----
    if (lpOutRoute->GetNodeCount() >= 2)
    {
        const RouteNode* lpLast = lpOutRoute->GetNode(lpOutRoute->GetNodeCount() - 1);
        const RouteNode* lpPrev = lpOutRoute->GetNode(lpOutRoute->GetNodeCount() - 2);

        // Normalised travel direction of the final segment.
        const f32 lfDirX = lpLast->GetX() - lpPrev->GetX();
        const f32 lfDirY = lpLast->GetY() - lpPrev->GetY();
        const f32 lfDirLen = sqrtf((lfDirX * lfDirX) + (lfDirY * lfDirY));
        const f32 lfInvLen = (lfDirLen != 0.0f) ? (1.0f / lfDirLen) : 0.0f;
        const f32 lfTravelX = lfDirX * lfInvLen;
        const f32 lfTravelY = lfDirY * lfInvLen;

        // The exit section is the BEST node's OWN section (lpBestNode->muSectionIndex,
        // node+0x12) -- the X360 worker reads *(lpBestNode+0x12) here, NOT muEndSectionIndex
        // (the two coincide on E_STATUS_COMPLETE but the asm authority is the best node).
        const u16 luExitSection = lpBestNode->GetSectionIndex();
        CGS_ASSERT(luExitSection < mpAISectionsData->muNumSections,
                   "luSectionIndex < muNumSections");
        const AISection* lpEndSection = mpAISectionsData->GetAISection(luExitSection);

        u8  luBestPortal = 0;
        f32 lfBestDot    = KF_BUILDROUTE_DOT_SEED;   // -2.0 (flt_820C4358)

        const u8 luNumPortals = lpEndSection->mu8NumPortals;
        for (u8 luPortalIndex = 0; luPortalIndex < luNumPortals; ++luPortalIndex)
        {
            const Portal* lpPortal = lpEndSection->GetPortal(luPortalIndex);

            // Direction from the route's last node to this portal, on the ground plane.
            const Vector3 lPortalPos3{ lpPortal->GetPositionX(),
                                       lpPortal->GetPositionY(),
                                       lpPortal->GetPositionZ(), 0.0f };
            const Vector2 lPortalFlat = BrnMath::Flatten(lPortalPos3);
            const f32 lfDeltaX = lpPortal->GetPositionX() - lpLast->GetX();
            const f32 lfDeltaY = lpPortal->GetPositionZ() - lpLast->GetY();
            (void)lPortalFlat;   // Flatten is called for its X360 side-effect parity.

            // Zero-offset gate (asm 0x8277FC6C..0x8277FCF4): the portal that sits ON the last
            // node -- |dx| <= eps AND |dy| <= eps, the RwMath::IsZero idiom over flt_820C3B70
            // lane 0 -- has no direction to dot against and is skipped (`bne loc_8277FD6C`
            // when both lane tests fail). Any non-zero offset proceeds.
            if (!(fabsf(lfDeltaX) > KF_ZERO_EPSILON) && !(fabsf(lfDeltaY) > KF_ZERO_EPSILON)) continue;

            // Normalised portal direction, then dot against travel direction.
            const f32 lfDeltaLen = sqrtf((lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY));
            const f32 lfDeltaInv = (lfDeltaLen != 0.0f) ? (1.0f / lfDeltaLen) : 0.0f;
            const f32 lfDot = (lfDeltaX * lfDeltaInv * lfTravelX)
                            + (lfDeltaY * lfDeltaInv * lfTravelY);
            if (lfDot > lfBestDot)
            {
                lfBestDot    = lfDot;
                luBestPortal = luPortalIndex;
            }
        }

        // Append the exit node at the chosen portal's (x, z), tagging section + portal.
        const Portal* lpExitPortal = lpEndSection->GetPortal(luBestPortal);
        Vector4 lExitNode;
        lExitNode.x = lpExitPortal->GetPositionX();
        lExitNode.y = lpExitPortal->GetPositionZ();
        lExitNode.z = 0.0f;
        lExitNode.w = 0.0f;
        reinterpret_cast<u16*>(&lExitNode.w)[0] = luExitSection;
        reinterpret_cast<u8*>(&lExitNode.w)[2]  = luBestPortal;
        lpOutRoute->AddNode(lExitNode);
    }
}

// BrnAI::AStar::ManhattanDistance @0x8276E170.
// L1 (taxicab) heuristic on the ground plane: |dx| + |dy|.
f32 AStar::ManhattanDistance(const AStarVector2& lA, const AStarVector2& lB)
{
    return fabsf(lB.Y() - lA.Y()) + fabsf(lB.X() - lA.X());
}

// BrnAI::AStar::DiagonalDistance @0x8276E198.
// Chebyshev-style diagonal heuristic: max(|dx|, |dy|). The X360 computes
//   t = |dx| - |dy| ; result = fsel(t, |dx|, |dy|)
// i.e. select |dx| when (|dx| - |dy|) >= 0 else |dy| -- the larger of the two axes.
f32 AStar::DiagonalDistance(const AStarVector2& lA, const AStarVector2& lB)
{
    const f32 lfAbsDX = fabsf(lB.X() - lA.X());
    const f32 lfAbsDY = fabsf(lB.Y() - lA.Y());
    return ((lfAbsDX - lfAbsDY) >= 0.0f) ? lfAbsDX : lfAbsDY;
}
}
