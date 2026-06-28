#ifndef BRN_ASTAR_H
#define BRN_ASTAR_H

// BrnAI::AStar -- the World/AI route pathfinder. DWARF home: BrnAStar.h.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Only the three Euclidean distance
// heuristics are bodied in this TU (@0x8276E0D8 / @0x8276E100 / @0x8276E138);
// the remaining members (Construct/Prepare/Compute/BuildRoute/AStarNode &
// AStarNodePool methods) live in BrnAStar.cpp and are declared here only.
//
// Member layout follows the DecFIGS DWARF declarations; every access is by name
// (no raw offset casts). The position fields are rwmath Vector2Template<float>
// (the DWARF "Basic2dColouredVertex::Vector2" prefix is a namespace-merge
// artifact -- the underlying type is the rw fpu 2-component vector).

#include <cmath>
#include "types.hpp"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu
#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISection / AISectionsData

namespace BrnAI
{
// Separate-TU types referenced by pointer/return only.
struct Route;
struct RaceBalancingGraph;

// The 2D position type used throughout the pathfinder (rw fpu vector).
typedef rw::math::fpu::Vector2Template<float> AStarVector2;

// Route quality presets (number of search iterations / nodes-per-iteration budget).
// The X360 Prepare guards "leQuality >= 0 && leQuality < E_ASTAR_QUALITY_COUNT" with
// cmpwi ..,2, so the quality preset table KAF_QUALITY_COST_WEIGHTS has exactly 2 entries
// (E_ASTAR_QUALITY_COUNT == 2). E_ASTAR_QUALITY_HIGH is retained as a named alias of the
// upper preset; it is not a third selectable index in this build.
enum AStarQuality
{
    E_ASTAR_QUALITY_LOW = 0,
    E_ASTAR_QUALITY_MEDIUM,
    E_ASTAR_QUALITY_COUNT,
    E_ASTAR_QUALITY_HIGH = E_ASTAR_QUALITY_MEDIUM,
};

// Index into AStar::KAP_DISTANCE_FUNCTIONS (5 distance heuristics).
// The X360 Prepare guards "leDistanceFunction >= 0 && leDistanceFunction <
// E_ASTAR_DISTANCE_FUNCTION_COUNT" with cmpwi ..,5, so KAP_DISTANCE_FUNCTIONS has exactly
// 5 entries in this enum order.
enum AStarDistanceFunction
{
    E_ASTAR_DISTANCE_EUCLIDEAN = 0,
    E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED,
    E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED,
    E_ASTAR_DISTANCE_MANHATTAN,
    E_ASTAR_DISTANCE_DIAGONAL,
    E_ASTAR_DISTANCE_FUNCTION_COUNT,
};

// ---- BrnAI::AStarNode  (DWARF BrnAStar.h:55) ----
class AStarNode
{
public:
    void         Construct(const AStarVector2& lPosition, u16 luSectionIndex,
                           u16 luLinkSectionIndex, u8 luPortalIndex);
    AStarVector2 GetPosition() const;
    f32          GetCost() const;
    f32          GetHeuristic() const;
    u16          GetSectionIndex() const;
    u16          GetLinkSectionIndex() const;
    u8           GetPortalIndex() const;
    u16          GetParentIndex() const;
    bool         IsOpen() const;
    void         SetCost(f32 lfCost);
    void         SetHeuristic(f32 lfHeuristic);
    void         SetParentIndex(u16 luParentIndex);
    void         Close();
    bool         UpdateCost(f32 lfCost, u16 luParentIndex);

private:
    AStarVector2 mPosition;          // :109
    f32          mfCost;             // :110
    f32          mfHeuristic;        // :111
    u16          muParentIndex;      // :112
    u16          muSectionIndex;     // :113
    u16          muLinkSectionIndex; // :114
    u8           muPortalIndex;      // :115
    bool         mbOpen;             // :116
};

class AStar;   // befriended by AStarNodePool (Prepare inlines the pool reset).

// ---- BrnAI::AStarNodePool  (DWARF BrnAStar.h:134) ----
class AStarNodePool
{
    // AStar::Prepare clears mauNodeCount[]/muOpenNodeCount inline (X360 @0x82774BD8..)
    // and Compute reads the four bucket counts directly; befriend it for by-name access.
    friend class AStar;
public:
    static const u16 KU_HASH_MASK          = 3;     // :139
    static const u16 KU_PARTITION_COUNT    = 4;     // :140
    static const u16 KU_MAX_PARTITION_NODES = 256;  // :141
    static const u16 KU_MAX_NODES          = 1024;  // :142
    static const u16 KU_MAX_OPEN_NODES     = 128;   // :143
    static const u16 KU_INVALID_NODE_INDEX = 65535; // :144
    static const u8  KU_INVALID_PORTAL_INDEX = 255; // :145

    void       Construct();
    AStarNode* NewNode(const AStarVector2& lPosition, u16 luLinkSectionIndex,
                       u16 luSectionIndex, u8 luPortalIndex);
    AStarNode* ExtractBestOpenNode(u16* lpuBestNodeIndex, f32 lfCostWeight);
    AStarNode* FindNode(u16 luSectionIndex, u8 luPortalIndex);
    AStarNode* GetNode(u16 luNodeIndex);
    s32        GetNodeCount() const;

private:
    AStarNode maNodes[KU_MAX_NODES];               // :175
    u16       mauOpenNodes[KU_MAX_OPEN_NODES];      // :176
    u16       mauNodeCount[KU_MAX_PARTITION_NODES]; // :178
    u16       muOpenNodeCount;                       // :179
};

// ---- AStarNode trivial member bodies (additive grow) ----
// The by-name field accessors NewNode / ExtractBestOpenNode / Compute / BuildRoute use.
// Each maps one-for-one onto a single store/load the X360 emits inline (e.g. NewNode
// @0x82774A20: stfs position, sth section/link, stb portal, stb open=1, sth parent=-1,
// stfs cost/heuristic=0). Inline so the layout (24-byte stride) is unchanged; no separate
// TU bodies them. Placed after AStarNodePool so KU_INVALID_NODE_INDEX is complete.
inline void AStarNode::Construct(const AStarVector2& lPosition, u16 luSectionIndex,
                                 u16 luLinkSectionIndex, u8 luPortalIndex)
{
    mPosition          = lPosition;
    mfCost             = 0.0f;
    mfHeuristic        = 0.0f;
    muParentIndex      = AStarNodePool::KU_INVALID_NODE_INDEX;   // -1 (0xFFFF)
    muSectionIndex     = luSectionIndex;
    muLinkSectionIndex = luLinkSectionIndex;
    muPortalIndex      = luPortalIndex;
    mbOpen             = true;
}
inline AStarVector2 AStarNode::GetPosition() const   { return mPosition; }
inline f32          AStarNode::GetCost() const       { return mfCost; }
inline f32          AStarNode::GetHeuristic() const  { return mfHeuristic; }
inline u16          AStarNode::GetSectionIndex() const     { return muSectionIndex; }
inline u16          AStarNode::GetLinkSectionIndex() const { return muLinkSectionIndex; }
inline u8           AStarNode::GetPortalIndex() const      { return muPortalIndex; }
inline u16          AStarNode::GetParentIndex() const      { return muParentIndex; }
inline bool         AStarNode::IsOpen() const              { return mbOpen; }
inline void         AStarNode::SetCost(f32 lfCost)             { mfCost = lfCost; }
inline void         AStarNode::SetHeuristic(f32 lfHeuristic)   { mfHeuristic = lfHeuristic; }
inline void         AStarNode::SetParentIndex(u16 luParentIndex) { muParentIndex = luParentIndex; }
inline void         AStarNode::Close()                        { mbOpen = false; }

// ---- BrnAI::AStar  (DWARF BrnAStar.h:191) ----
// Compile-gate probe (embed check) -- befriended so it can reference the
// private distance heuristics; no effect on layout or release behaviour.
struct AStarEmbedProbe;

class AStar
{
    friend struct AStarEmbedProbe;
public:
    static const s32 KI_MAX_BLOCK_SECTION_COUNT = 16; // :195
    static const s32 KI_MAX_ITERATIONS          = 12; // :196

    void Construct(const AISectionsData* lpAISectionsData);
    bool Prepare(const AStarVector2& lStartPosition, const AStarVector2& lEndPosition,
                 u16 luStartSectionIndex, u16 luEndSectionIndex,
                 AStarQuality leQuality, AStarDistanceFunction leDistanceFunction,
                 bool lbUseAIShortcuts);
    void AddBlockSectionId(u32 lSectionId);   // AISection::AISectionId == u32
    void Compute();
    void BuildRoute(Route* lpOutRoute);
    bool IsInProgress() const;

private:
    void BuildRoute(AStarNode* lpBestNode, Route* lpOutRoute);
    bool IsBlockSection(u32 lSectionId);   // AISection::AISectionId == u32

    // The distance heuristics are CONTEXT-FREE: the X360 calls each through the
    // mpDistanceFunction member pointer with exactly two args and no `this` (the bodies
    // touch only their arguments). They are therefore static, and the dispatch table
    // KAP_DISTANCE_FUNCTIONS holds plain function pointers (off_820C47C8).
    static f32  Distance(const AStarVector2& lA, const AStarVector2& lB);

    // ---- bodied here (leaf heuristics) ----
    static f32  EuclideanDistance(const AStarVector2& lA, const AStarVector2& lB);
    static f32  EuclideanDistanceXBiased(const AStarVector2& lA, const AStarVector2& lB);
    static f32  EuclideanDistanceYBiased(const AStarVector2& lA, const AStarVector2& lB);

    static f32  ManhattanDistance(const AStarVector2& lA, const AStarVector2& lB);
    static f32  DiagonalDistance(const AStarVector2& lA, const AStarVector2& lB);

    // One-axis-amplified Euclidean bias factor (rodata flt_820C478C == 500.0f).
    static constexpr f32 KF_EUCLIDEAN_BIAS = 500.0f;

    // ---- search tuning tables / constants ----------------------------------------
    // Distance-function pointer type stored in mpDistanceFunction / the dispatch table.
    typedef f32 (*DistanceFunctionPtr)(const AStarVector2&, const AStarVector2&);

    // Resolved by Prepare from leDistanceFunction (off_820C47C8), in enum order.
    static const DistanceFunctionPtr KAP_DISTANCE_FUNCTIONS[E_ASTAR_DISTANCE_FUNCTION_COUNT];

    // Per-quality cost weight (flt_820C47C0; PLACEHOLDER values -- not in the asm).
    static const f32 KAF_QUALITY_COST_WEIGHTS[E_ASTAR_QUALITY_COUNT];

    // Per-iteration live-node budget Compute compares the bucket-count sum against
    // (dword_820C4790; PLACEHOLDER values -- not in the asm).
    static const s32 KAI_ITERATION_NODE_BUDGET[KI_MAX_ITERATIONS];

    // Construct's default/sentinel cost weight (flt_820037C8 == -1.0; asm-attested).
    static const f32 KF_DEFAULT_COST_WEIGHT;

    // BuildRoute exit-portal selection: the dot-product seed (flt_820C4358 == -2.0, the
    // asm-attested literal) and the per-axis reachability extent (flt_820C3B70, a rodata
    // vector; PLACEHOLDER -- not a clean literal in the asm).
    static constexpr f32 KF_BUILDROUTE_DOT_SEED    = -2.0f;
    static const f32     KF_BUILDROUTE_REACH_EXTENT;

    AStarNodePool        mAStarNodePool;       // :280
    AStarVector2         mStartPosition;       // :281
    AStarVector2         mEndPosition;         // :282
    DistanceFunctionPtr  mpDistanceFunction;   // :283
    const AISectionsData* mpAISectionsData;    // :284
    AStarNode*           mpBestNode;           // :285
    s32                  meRouteStatus;        // :286 (BrnAI::Route::Status; opaque here)
    s32                  miIterationCount;     // :287
    f32                  mfCostWeight;         // :288
    u32                  maBlockSectionIds[KI_MAX_BLOCK_SECTION_COUNT]; // :289 (AISection::AISectionId == u32)
    s32                  miBlockSectionCount;  // :290
    u16                  muStartSectionIndex;  // :291
    u16                  muEndSectionIndex;    // :292
    bool                 mbUseAIShortcuts;     // :293
    bool                 mbInProgress;         // :294
};

// ---- CPU-perf monitor handles (X360 file-static; shared across the AStar TUs) ----
// AddMonitor returns a 0-based handle (or -1 when unbuilt); Construct registers them once.
//   dword_82F3023C -- "A*"                      (whole Compute pass)
//   dword_8300D530 -- "A* Extract best open node"
//   dword_8300D534 -- "A* Find node"
extern s32 dword_82F3023C;
extern s32 dword_8300D530;
extern s32 dword_8300D534;

// @0x8276E0D8  straight Euclidean distance: sqrt(dx*dx + dy*dy).
inline f32 AStar::EuclideanDistance(const AStarVector2& lA, const AStarVector2& lB)
{
    const f32 lfDX = lA.X() - lB.X();
    const f32 lfDY = lA.Y() - lB.Y();
    return sqrtf((lfDX * lfDX) + (lfDY * lfDY));
}

// @0x8276E100  X-amplified Euclidean: the x delta is scaled by 500 before the
// magnitude, so paths that diverge along x cost far more (lane-bias heuristic).
inline f32 AStar::EuclideanDistanceXBiased(const AStarVector2& lA, const AStarVector2& lB)
{
    const f32 lfDX = (lA.X() - lB.X()) * KF_EUCLIDEAN_BIAS;
    const f32 lfDY = lA.Y() - lB.Y();
    return sqrtf((lfDX * lfDX) + (lfDY * lfDY));
}

// @0x8276E138  Y-amplified Euclidean: mirror of the X-biased form on the y axis.
inline f32 AStar::EuclideanDistanceYBiased(const AStarVector2& lA, const AStarVector2& lB)
{
    const f32 lfDX = lA.X() - lB.X();
    const f32 lfDY = (lA.Y() - lB.Y()) * KF_EUCLIDEAN_BIAS;
    return sqrtf((lfDX * lfDX) + (lfDY * lfDY));
}
}

#endif
