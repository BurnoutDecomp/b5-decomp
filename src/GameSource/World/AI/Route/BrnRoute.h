#ifndef BRN_ROUTE_H
#define BRN_ROUTE_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include <cstddef>

namespace BrnAI
{
// DecFIGS BrnRoute.h:50..89; ARTIST AddNode @827642A0 and the AStar/
// extrapolated-route producers attest scalar 16-byte records, aligned to four.
// The final byte is ordinary tail padding, not a fourth vector component.
struct RouteNode
{
    f32 mfPositionX;
    f32 mfPositionZ;
    f32 mfDistanceLeft;
    u16 muSectionIndex;
    u8  muPortalIndex;

    // Inlined in ARTIST BuildRoute @8277F930 and ProcessExtrapolatedRoute @8278C4C8.
    void Construct(f32 lfX, f32 lfZ, u16 luSectionIndex, u8 luPortalIndex)
    {
        mfPositionX = lfX;
        mfPositionZ = lfZ;
        mfDistanceLeft = 0.0f;
        muSectionIndex = luSectionIndex;
        muPortalIndex = luPortalIndex;
    }
    f32 GetPositionX() const { return mfPositionX; }
    f32 GetPositionZ() const { return mfPositionZ; }
    Vector2 GetPosition() const { { const Vector2 lResult = {mfPositionX, mfPositionZ, 0.0f, 0.0f}; return lResult; } }
    Vector3 GetPositionWithZeroY() const { { const Vector3 lResult = {mfPositionX, 0.0f, mfPositionZ, 0.0f}; return lResult; } }
    u16 GetSectionIndex() const { return muSectionIndex; }
    u8 GetPortalIndex() const { return muPortalIndex; }
    f32 GetDistanceLeft() const { return mfDistanceLeft; }
    void SetDistanceLeft(f32 lfDistance) { mfDistanceLeft = lfDistance; }

    // Existing native readers use these names for the same original scalars.
    f32 GetX() const { return GetPositionX(); }
    f32 GetY() const { return GetPositionZ(); }
    f32 GetDistanceToCheckpoint() const { return GetDistanceLeft(); }
};

// ARTIST has 320 scalar nodes, then the three words at 1400/1404/1408.
// DecFIGS names RouteNode[320], not an aligned SIMD vector array. The extent
// is 140C: AICar's Aggressiveness and RouteResponse's owner id follow there.
struct Route
{
    static const s32 KI_MAX_NODES = 320;
    enum Status
    {
        E_STATUS_UNINITIALISED = 0,
        E_STATUS_COMPLETE = 1,
        E_STATUS_PARTIAL = 2,
        E_STATUS_BLOCKED = 3,
    };

    void Construct(const Route* lpSource); // ARTIST 82767B28; DecFIGS :124
    bool Prepare(s32 liDefaultStartNode); // ARTIST 82767B98
    bool AddNode(const RouteNode& lrNode); // ARTIST 827642A0; DecFIGS :128

    s32 GetNodeCount() const { return miNodeCount; }
    s32 GetDefaultStartNode() const { return miDefaultStartNode; }
    Status GetStatus() const { return meStatus; }
    const RouteNode* GetNode(s32 liNodeIndex) const { return &maNodes[liNodeIndex]; }
    f32 GetDistance() const { return maNodes[0].GetDistanceLeft(); }

    RouteNode maNodes[KI_MAX_NODES];
    s32 miNodeCount;
    s32 miDefaultStartNode;
    Status meStatus;

private:
    static void _AssertLayout();
};
}

#endif
