#pragma once
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "vendor/renderware/collision/Frustum.hpp"

namespace CgsGeometric { struct PolygonSoup; }
namespace CgsSceneManager
{
class TriangleCollisionManager;
const s32 KI_MAX_NUM_COLLISION_POLY_CALLBACKS = 4;
// DecFIGS header:59/91/92; console callback/context stride 8, two native pointers on PC.
struct DrawCollisionPolyCallbackData
{
    typedef void (*DrawCollisionPolyCallback)(CgsDev::Debug3DImmediateRender*, void*, const Vector3*, s32, Vector3, u32, s32);
    DrawCollisionPolyCallback mpCallbackFunc;
    void* mpUserData;
};
typedef DrawCollisionPolyCallbackData DrawCollisionPolyCallback;

// ARTIST 828B3340 + DecFIGS declaration order. Drawing is recovered; the
// separate sphere/box/swept query diagnostics remain unreconstructed.
class TriangleCollisionDebugComponent : public CgsDev::DebugComponent
{
public:
    void Construct(const TriangleCollisionManager* lpTriCollisionManager);
    void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay) override;
    const char* GetName() const override { return "Collision"; } // 827DD828
    const char* GetPath() const override { return "World"; } // vtable 820CEEBC -> 827DD250
    static bool RegisterDrawCollisionPolyCallback(DrawCollisionPolyCallbackData::DrawCollisionPolyCallback lpHandlerFunc, void* lpUserData);
    static DrawCollisionPolyCallback& GetDrawCollisionPolyCallback(s32 liIndex)
    {
        CGS_ASSERT(liIndex < siNumDrawPolyCallbacks, "liIndex < siNumDrawPolyCallbacks");
        return siDrawPolyCallbacks[liIndex];
    }
    static void DrawCollisionPoly(CgsDev::Debug3DImmediateRender* lpDisplay,
        const Vector3* lpaPoints, s32 liNumPoints, Vector3 lNormal, u32 luTag, s32 liSoupIndex);
private:
    void OnActivate() override;
    void UpdateViewFrustum(CgsDev::Debug3DImmediateRender* lpDisplay);
    bool IsPolyVisible(const Vector3* lpaPoints, s32 liNumPoints, CgsDev::Debug3DImmediateRender* lpDisplay) const;
    void DrawPolySoups(CgsDev::Debug3DImmediateRender* lpDisplay);
    void DrawPolySoupPoly(CgsDev::Debug3DImmediateRender* lpDisplay,
        const CgsGeometric::PolygonSoup* lpSoup, s32 liSoupIndex, u8 luPolyIndex, bool lbDistanceCull);
    void BuildSpacialMap();

    const TriangleCollisionManager* mpTriCollisionManager;
    rw::collision::Frustum mViewFrustum;
    bool mbDrawPolySoups, mbDrawSoupAabbs, mbDrawPolyNormals, mbUpdateViewFrustum, mbDrawEdgeCosines;
    bool mbTestSphere;
    f32 mSpherePos[3], mfSphereRad;
    bool mbNeedToInitSpherePos;
    u32 muSphereTestCycles;
    f32 mfSphereTestTimeSecs, mfSphereTestTimePercentFrame;
    s32 miSphereTestResultCount, miSphereTestPerfMon, miSphereTestGetListsPerfMon, miSphereTestProcessSoupsPerfMon;
    bool mbTestLine, mbNeedToInitLinePosition;
    f32 mLineStartPosition[3], mLineEndPosition[3];
    s32 miLineTestPerfMon, miLineTestGetListsPerfMon, miLineTestProcessSoupsPerfMon;
    bool mbTestBox, mbNeedToInitBoxPos;
    f32 mBoxPos[3], mBoxSize[3], mBoxOrientation[3];
    u32 muBoxTestCycles;
    s32 miNumBoxTests;
    u32 muBoxTestCyclesPerTriangle;
    f32 mfBoxTestTime, mfBoxTestPercent;
    bool mbTestSweptSphere, mbNeedToInitSweptSpherePos;
    f32 mSweptSpherePos[3], mSweptSphereSize, mSweptSphereVelocity[3];
    s32 miMaxLevelToDraw, miMinLevelToDraw;
    char* mpcSpacialBuffer;
    s32 miNumLevels, miLastNumLevels, miMemUse, miNumListsSearched;
    CgsMemory::LinearMalloc mMalloc;
    f32 mfSoupDrawDistance;
    static DrawCollisionPolyCallback siDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS];
    static s32 siNumDrawPolyCallbacks;
};
}
