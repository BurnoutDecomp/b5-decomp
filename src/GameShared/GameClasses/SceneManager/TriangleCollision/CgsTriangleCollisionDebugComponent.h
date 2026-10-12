#pragma once

// CgsSceneManager::TriangleCollisionDebugComponent. The CgsDev debug-menu controls and
// member names follow DecFIGS; stored offsets and registrations are checked against ARTIST.

#include "types.hpp"
#include "BrnCommonTypes.h" // rw::math::vpu::Vector3 in the collision callback signature
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"
#include "vendor/renderware/collision/Frustum.hpp" // rw::collision::Frustum: six Plane lanes

namespace CgsDev { struct Debug3DImmediateRender; }

namespace CgsSceneManager
{
    class TriangleCollisionManager;

    static const s32 KI_MAX_NUM_COLLISION_POLY_CALLBACKS = 4;

    // One callback slot in the component's static collision-poly draw registry.
    // DecFIGS CgsTriangleCollisionDebugComponent.h:72, :91-92.
    struct DrawCollisionPolyCallbackData
    {
        typedef void DrawCollisionPolyCallback(CgsDev::Debug3DImmediateRender*, void*,
                                               const Vector3*, s32, Vector3, u32, s32);

        void Construct(DrawCollisionPolyCallback* lpfCallback, void* lpUserData);
        DrawCollisionPolyCallback* GetCallback();
        void* GetUserData();

    private:
        DrawCollisionPolyCallback* mpCallbackFunc;
        void*                      mpUserData;
    };

    class TriangleCollisionDebugComponent : public CgsDev::DebugComponent
    {
    public:
        static const s32 KI_MAX_NUM_LEVELS = 8;
        static const s32 KI_SPACIAL_BUFFER_SIZE = 2097152;

        // ARTIST 0x828B3340: bind the owning manager and initialize collision-test/menu state.
        void Construct(const TriangleCollisionManager* lpTriCollisionManager);

        // ARTIST 0x828AAF18: add one callback to the four-slot static registry.
        bool RegisterDrawCollisionPolyCallback(
            DrawCollisionPolyCallbackData::DrawCollisionPolyCallback* lpfCallback,
            void* lpUserData);

        // ARTIST 0x828AA458: bounds-check an existing registry index and return its slot.
        static DrawCollisionPolyCallbackData& GetDrawCollisionPolyCallback(s32 liIndex)
        {
            CGS_ASSERT(liIndex < siNumDrawPolyCallbacks,
                       "liIndex < siNumDrawPolyCallbacks");
            return saDrawPolyCallbacks[liIndex];
        }

        // Inherited Register() is the actual CgsDev::DebugComponent::Register path.
        const char* GetName() const override { return "Collision"; }

    protected:
        // ARTIST 0x828AAA68: register the collision controls and seed live spatial-level state.
        void OnActivate() override;

    private:
        static DrawCollisionPolyCallbackData saDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS];
        static s32 siNumDrawPolyCallbacks;

        const TriangleCollisionManager* mpTriCollisionManager; // X360 +0x0C
        rw::collision::Frustum mViewFrustum;                   // X360 +0x10, six x 16-byte planes
        bool mbDrawPolySoups;                                  // X360 +0x70
        bool mbDrawSoupAabbs;                                  // X360 +0x71
        bool mbDrawPolyNormals;                                // X360 +0x72
        bool mbUpdateViewFrustum;                              // X360 +0x73
        bool mbDrawEdgeCosines;                                // X360 +0x74
        bool mbTestSphere;                                      // X360 +0x75
        rw::math::fpu::Vector3Template<f32> mSpherePos;         // X360 +0x78
        f32 mfSphereRad;                                        // X360 +0x84
        bool mbNeedToInitSpherePos;                             // X360 +0x88
        u32 muSphereTestCycles;                                 // X360 +0x8C
        f32 mfSphereTestTimeSecs;                               // X360 +0x90
        f32 mfSphereTestTimePercentFrame;                       // X360 +0x94
        s32 miSphereTestResultCount;                            // X360 +0x98
        s32 miSphereTestPerfMon;                                // X360 +0x9C
        s32 miSphereTestGetListsPerfMon;                        // X360 +0xA0
        s32 miSphereTestProcessSoupsPerfMon;                    // X360 +0xA4
        bool mbTestLine;                                        // X360 +0xA8
        bool mbNeedToInitLinePosition;                          // X360 +0xA9
        rw::math::fpu::Vector3Template<f32> mLineStartPosition; // X360 +0xAC
        rw::math::fpu::Vector3Template<f32> mLineEndPosition;   // X360 +0xB8
        s32 miLineTestPerfMon;                                  // X360 +0xC4
        s32 miLineTestGetListsPerfMon;                          // X360 +0xC8
        s32 miLineTestProcessSoupsPerfMon;                      // X360 +0xCC
        bool mbTestBox;                                         // X360 +0xD0
        bool mbNeedToInitBoxPos;                                // X360 +0xD1
        rw::math::fpu::Vector3Template<f32> mBoxPos;            // X360 +0xD4
        rw::math::fpu::Vector3Template<f32> mBoxSize;           // X360 +0xE0
        rw::math::fpu::Vector3Template<f32> mBoxOrientation;    // X360 +0xEC
        u32 muBoxTestCycles;                                    // X360 +0xF8
        s32 miNumBoxTests;                                      // X360 +0xFC
        u32 muBoxTestCyclesPerTriangle;                         // X360 +0x100
        f32 mfBoxTestTime;                                      // X360 +0x104
        f32 mfBoxTestPercent;                                   // X360 +0x108
        bool mbTestSweptSphere;                                 // X360 +0x10C
        bool mbNeedToInitSweptSpherePos;                        // X360 +0x10D
        rw::math::fpu::Vector3Template<f32> mSweptSpherePos;    // X360 +0x110
        f32 mSweptSphereSize;                                   // X360 +0x11C
        rw::math::fpu::Vector3Template<f32> mSweptSphereVelocity; // X360 +0x120
        s32 miMaxLevelToDraw;                                   // X360 +0x12C
        s32 miMinLevelToDraw;                                   // X360 +0x130
        char* mpcSpacialBuffer;                                 // X360 +0x134
        s32 miNumLevels;                                        // X360 +0x138
        s32 miLastNumLevels;                                    // X360 +0x13C
        s32 miMemUse;                                           // X360 +0x140
        s32 miNumListsSearched;                                 // X360 +0x144
        CgsMemory::LinearMalloc mMalloc;                        // X360 +0x148
        f32 mfSoupDrawDistance;                                 // X360 +0x164
    };
}
