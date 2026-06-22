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
#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISection / AISectionsData

namespace BrnAI
{
// Separate-TU types referenced by pointer/return only.
struct Route;
struct RaceBalancingGraph;

// The 2D position type used throughout the pathfinder (rw fpu vector).
typedef rw::math::fpu::Vector2Template<float> AStarVector2;

// Route quality presets (number of search iterations / nodes-per-iteration budget).
enum AStarQuality
{
    E_ASTAR_QUALITY_LOW = 0,
    E_ASTAR_QUALITY_MEDIUM,
    E_ASTAR_QUALITY_HIGH,
};

// Index into AStar::KAP_DISTANCE_FUNCTIONS (5 distance heuristics).
enum AStarDistanceFunction
{
    E_ASTAR_DISTANCE_EUCLIDEAN = 0,
    E_ASTAR_DISTANCE_EUCLIDEAN_X_BIASED,
    E_ASTAR_DISTANCE_EUCLIDEAN_Y_BIASED,
    E_ASTAR_DISTANCE_MANHATTAN,
    E_ASTAR_DISTANCE_DIAGONAL,
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

// ---- BrnAI::AStarNodePool  (DWARF BrnAStar.h:134) ----
class AStarNodePool
{
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
    f32  Distance(const AStarVector2& lA, const AStarVector2& lB);

    // ---- bodied here (leaf heuristics) ----
    f32  EuclideanDistance(const AStarVector2& lA, const AStarVector2& lB);
    f32  EuclideanDistanceXBiased(const AStarVector2& lA, const AStarVector2& lB);
    f32  EuclideanDistanceYBiased(const AStarVector2& lA, const AStarVector2& lB);

    f32  ManhattanDistance(const AStarVector2& lA, const AStarVector2& lB);
    f32  DiagonalDistance(const AStarVector2& lA, const AStarVector2& lB);

    // One-axis-amplified Euclidean bias factor (rodata flt_820C478C == 500.0f).
    static constexpr f32 KF_EUCLIDEAN_BIAS = 500.0f;

    AStarNodePool        mAStarNodePool;       // :280
    AStarVector2         mStartPosition;       // :281
    AStarVector2         mEndPosition;         // :282
    f32 (*mpDistanceFunction)(const AStarVector2&, const AStarVector2&); // :283
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

// ---- bodied leaf heuristics (semantic parity with X360 asm) ----

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
