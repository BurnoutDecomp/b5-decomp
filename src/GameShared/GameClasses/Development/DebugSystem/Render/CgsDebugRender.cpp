#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // the immediate renderer Dispatch2D replays into
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"

#include <cmath>

// CgsDev::DebugRender - buffered debug-render bodies. DrawX queues a byte-image event under its
// ARTIST type ID; the dispatchers walk their queues and replay into the immediate renderers.
// Reconstructed from the X360 ARTIST build (see CgsDebugRender.h for addresses). The events are POD and
// passed to the queue as CgsModule::Event* by reinterpret (the queue stores them by byte image).

namespace CgsDev
{
    namespace
    {
        rw::RGBA MakeImmediateColour(RGBA lColour)
        {
            rw::RGBA lResult;
            lResult.m_rgba = lColour;
            return lResult;
        }

        Vector3 MakeVector3(f32 lfX, f32 lfY, f32 lfZ)
        {
            return Vector3{ lfX, lfY, lfZ, 0.0f };
        }

        Matrix44Affine MakeTransform(const Internal::CInEventDrawAxis& lrEvent)
        {
            Matrix44Affine lTransform;
            lTransform.xAxis = Vector3{ lrEvent.mfRtX, lrEvent.mfRtY, lrEvent.mfRtZ, 0.0f };
            lTransform.yAxis = Vector3{ lrEvent.mfUpX, lrEvent.mfUpY, lrEvent.mfUpZ, 0.0f };
            lTransform.zAxis = Vector3{ lrEvent.mfAtX, lrEvent.mfAtY, lrEvent.mfAtZ, 0.0f };
            lTransform.wAxis = Vector3{ lrEvent.mfPosX, lrEvent.mfPosY, lrEvent.mfPosZ, 1.0f };
            return lTransform;
        }

        template <typename BoxEvent>
        Matrix44Affine MakeBoxTransform(const BoxEvent& lrEvent)
        {
            Matrix44Affine lTransform;
            lTransform.xAxis = Vector3{ lrEvent.mfRtX, lrEvent.mfRtY, lrEvent.mfRtZ, 0.0f };
            lTransform.yAxis = Vector3{ lrEvent.mfUpX, lrEvent.mfUpY, lrEvent.mfUpZ, 0.0f };
            lTransform.zAxis = Vector3{ lrEvent.mfAtX, lrEvent.mfAtY, lrEvent.mfAtZ, 0.0f };
            lTransform.wAxis = Vector3{ lrEvent.mfPosX, lrEvent.mfPosY, lrEvent.mfPosZ, 1.0f };
            return lTransform;
        }

        template <typename LineEvent>
        void FillLineEvent(LineEvent& lrEvent, Vector3 lFrom, Vector3 lTo, RGBA lColour)
        {
            lrEvent.mfX1 = lFrom.x;
            lrEvent.mfY1 = lFrom.y;
            lrEvent.mfZ1 = lFrom.z;
            lrEvent.mfX2 = lTo.x;
            lrEvent.mfY2 = lTo.y;
            lrEvent.mfZ2 = lTo.z;
            lrEvent.mColour = lColour;
        }

        template <typename SphereEvent>
        void FillSphereEvent(SphereEvent& lrEvent, Vector3 lCentre, f32 lfRadius, RGBA lColour)
        {
            lrEvent.mfX = lCentre.x;
            lrEvent.mfY = lCentre.y;
            lrEvent.mfZ = lCentre.z;
            lrEvent.mfRadius = lfRadius;
            lrEvent.mColour = lColour;
        }

        template <typename BoxEvent>
        void FillOrientedBoxEvent(BoxEvent& lrEvent, Vector3 lMin, Vector3 lMax,
                                  const Matrix44Affine& lrTransform, RGBA lColour)
        {
            lrEvent.mfPosX = lrTransform.wAxis.x;
            lrEvent.mfPosY = lrTransform.wAxis.y;
            lrEvent.mfPosZ = lrTransform.wAxis.z;
            lrEvent.mfAtX = lrTransform.zAxis.x;
            lrEvent.mfAtY = lrTransform.zAxis.y;
            lrEvent.mfAtZ = lrTransform.zAxis.z;
            lrEvent.mfUpX = lrTransform.yAxis.x;
            lrEvent.mfUpY = lrTransform.yAxis.y;
            lrEvent.mfUpZ = lrTransform.yAxis.z;
            lrEvent.mfRtX = lrTransform.xAxis.x;
            lrEvent.mfRtY = lrTransform.xAxis.y;
            lrEvent.mfRtZ = lrTransform.xAxis.z;
            lrEvent.mfInfX = lMin.x;
            lrEvent.mfInfY = lMin.y;
            lrEvent.mfInfZ = lMin.z;
            lrEvent.mfSupX = lMax.x;
            lrEvent.mfSupY = lMax.y;
            lrEvent.mfSupZ = lMax.z;
            lrEvent.mColour = lColour;
        }

        template <typename BoxEvent>
        void FillAxisAlignedBoxEvent(BoxEvent& lrEvent, Vector3 lMin, Vector3 lMax, RGBA lColour)
        {
            lrEvent.mfInfX = lMin.x;
            lrEvent.mfInfY = lMin.y;
            lrEvent.mfInfZ = lMin.z;
            lrEvent.mfSupX = lMax.x;
            lrEvent.mfSupY = lMax.y;
            lrEvent.mfSupZ = lMax.z;
            lrEvent.mColour = lColour;
        }
    }

    // X360 DebugManager::Construct @0x828332C0 constructs the pair inline, 2D queue (+0x4010)
    // first, then the 3D queue (+0).
    void DebugRender::Construct()
    {
        m2DQueue.Construct();
        m3DQueue.Construct();
    }

    void DebugRender::Clear()
    {
        m3DQueue.Clear();
        m2DQueue.Clear();
    }

    void DebugRender::Destruct()
    {
        m3DQueue.Destruct();
        m2DQueue.Destruct();
    }

    // --- Buffered 3D publishers ---------------------------------------------------------------

    void DebugRender::DrawText(Vector3 lPosition, const char* lpcText, f32 lfScale, RGBA lColour)
    {
        m3DQueue.AddStringEventSafe(lpcText, Internal::E_INEVENT_3D_STRING);
        Internal::CInEventDrawText lEvent;
        lEvent.mfX = lPosition.x;
        lEvent.mfY = lPosition.y;
        lEvent.mfZ = lPosition.z;
        lEvent.mfSize = lfScale;
        lEvent.mColour = lColour;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_TEXT);
    }

    void DebugRender::DrawLine(Vector3 lFrom, Vector3 lTo, RGBA lColour)
    {
        Internal::CInEventDrawLine lEvent;
        FillLineEvent(lEvent, lFrom, lTo, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_LINE);
    }

    void DebugRender::DrawPoint(Vector3 lPosition, RGBA lColour)
    {
        Internal::CInEventDrawPoint lEvent;
        lEvent.mfX = lPosition.x;
        lEvent.mfY = lPosition.y;
        lEvent.mfZ = lPosition.z;
        lEvent.mColour = lColour;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_POINT);
    }

    void DebugRender::DrawQuad(Vector3 lA, Vector3 lB, Vector3 lC, Vector3 lD, RGBA lColour)
    {
        Internal::CInEventDrawQuad lEvent;
        lEvent.mfX1 = lA.x; lEvent.mfY1 = lA.y; lEvent.mfZ1 = lA.z;
        lEvent.mfX2 = lB.x; lEvent.mfY2 = lB.y; lEvent.mfZ2 = lB.z;
        lEvent.mfX3 = lC.x; lEvent.mfY3 = lC.y; lEvent.mfZ3 = lC.z;
        lEvent.mfX4 = lD.x; lEvent.mfY4 = lD.y; lEvent.mfZ4 = lD.z;
        lEvent.mColour = lColour;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_QUAD);
    }

    void DebugRender::DrawSolidQuad(Vector3 lA, Vector3 lB, Vector3 lC, Vector3 lD, RGBA lColour)
    {
        Internal::CInEventDrawQuad lEvent;
        lEvent.mfX1 = lA.x; lEvent.mfY1 = lA.y; lEvent.mfZ1 = lA.z;
        lEvent.mfX2 = lB.x; lEvent.mfY2 = lB.y; lEvent.mfZ2 = lB.z;
        lEvent.mfX3 = lC.x; lEvent.mfY3 = lC.y; lEvent.mfZ3 = lC.z;
        lEvent.mfX4 = lD.x; lEvent.mfY4 = lD.y; lEvent.mfZ4 = lD.z;
        lEvent.mColour = lColour;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_SOLID_QUAD);
    }

    void DebugRender::DrawAngleDeg(Vector3 lPosition, f32 lfAngle, RGBA lColour)
    {
        DrawAngleRad(lPosition, lfAngle * 0.01745329251994329577f, lColour);
    }

    void DebugRender::DrawAngleRad(Vector3 lPosition, f32 lfAngle, RGBA lColour)
    {
        Internal::CInEventDrawAngle lEvent;
        lEvent.mfX = lPosition.x;
        lEvent.mfY = lPosition.y;
        lEvent.mfZ = lPosition.z;
        lEvent.mfAngle = lfAngle;
        lEvent.mColour = lColour;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_ANGLE);
    }

    void DebugRender::DrawAxis(Matrix44Affine lTransform)
    {
        Internal::CInEventDrawAxis lEvent;
        lEvent.mfAtX = lTransform.zAxis.x;
        lEvent.mfAtY = lTransform.zAxis.y;
        lEvent.mfAtZ = lTransform.zAxis.z;
        lEvent.mfUpX = lTransform.yAxis.x;
        lEvent.mfUpY = lTransform.yAxis.y;
        lEvent.mfUpZ = lTransform.yAxis.z;
        lEvent.mfRtX = lTransform.xAxis.x;
        lEvent.mfRtY = lTransform.xAxis.y;
        lEvent.mfRtZ = lTransform.xAxis.z;
        lEvent.mfPosX = lTransform.wAxis.x;
        lEvent.mfPosY = lTransform.wAxis.y;
        lEvent.mfPosZ = lTransform.wAxis.z;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_AXIS);
    }

    void DebugRender::DrawSphere(Vector3 lCentre, f32 lfRadius, RGBA lColour)
    {
        Internal::CInEventDrawSphere lEvent;
        FillSphereEvent(lEvent, lCentre, lfRadius, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_SPHERE);
    }

    void DebugRender::DrawSolidSphere(Vector3 lCentre, f32 lfRadius, RGBA lColour)
    {
        Internal::CInEventDrawSolidSphere lEvent;
        FillSphereEvent(lEvent, lCentre, lfRadius, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_SOLID_SPHERE);
    }

    void DebugRender::DrawHollowSphere(Vector3 lCentre, f32 lfRadius, RGBA lColour)
    {
        Internal::CInEventDrawHollowSphere lEvent;
        FillSphereEvent(lEvent, lCentre, lfRadius, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_HOLLOW_SPHERE);
    }

    void DebugRender::DrawCircle(Matrix44Affine lTransform, f32 lfRadius, RGBA lColour)
    {
        DrawCircle(MakeVector3(lTransform.wAxis.x, lTransform.wAxis.y, lTransform.wAxis.z),
                   MakeVector3(lTransform.zAxis.x, lTransform.zAxis.y, lTransform.zAxis.z),
                   lfRadius, lColour);
    }

    void DebugRender::DrawCircle(Vector3 lCentre, Vector3 lNormal, f32 lfRadius, RGBA lColour)
    {
        Internal::CInEventDrawCircle lEvent;
        lEvent.mfPosX = lCentre.x;
        lEvent.mfPosY = lCentre.y;
        lEvent.mfPosZ = lCentre.z;
        lEvent.mfDirX = lNormal.x;
        lEvent.mfDirY = lNormal.y;
        lEvent.mfDirZ = lNormal.z;
        lEvent.mfRadius = lfRadius;
        lEvent.mColour = lColour;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_CIRCLE);
    }

    void DebugRender::DrawBox(Vector3 lMin, Vector3 lMax, Matrix44Affine lTransform, RGBA lColour)
    {
        Internal::CInEventDrawBox lEvent;
        FillOrientedBoxEvent(lEvent, lMin, lMax, lTransform, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_BOX);
    }

    void DebugRender::DrawBox(Vector3 lMin, Vector3 lMax, RGBA lColour)
    {
        Internal::CInEventDrawBoxAA lEvent;
        FillAxisAlignedBoxEvent(lEvent, lMin, lMax, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_BOX_AA);
    }

    void DebugRender::DrawSolidBox(Vector3 lMin, Vector3 lMax,
                                   Matrix44Affine lTransform, RGBA lColour)
    {
        Internal::CInEventDrawSolidBox lEvent;
        FillOrientedBoxEvent(lEvent, lMin, lMax, lTransform, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_SOLID_BOX);
    }

    void DebugRender::DrawSolidBox(Vector3 lMin, Vector3 lMax, RGBA lColour)
    {
        Internal::CInEventDrawSolidBoxAA lEvent;
        FillAxisAlignedBoxEvent(lEvent, lMin, lMax, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_SOLID_BOX_AA);
    }

    void DebugRender::DrawArrow(Vector3 lFrom, Vector3 lTo, RGBA lColour)
    {
        Internal::CInEventDrawArrow lEvent;
        FillLineEvent(lEvent, lFrom, lTo, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_ARROW);
    }

    void DebugRender::DrawSolidArrow(Vector3 lFrom, Vector3 lTo, RGBA lColour)
    {
        Internal::CInEventDrawSolidArrow lEvent;
        FillLineEvent(lEvent, lFrom, lTo, lColour);
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_SOLID_ARROW);
    }

    void DebugRender::DrawCapsule(Vector3 lStart, Vector3 lEnd, f32 lfRadius, RGBA lColour)
    {
        Internal::CInEventDrawCapsule lEvent;
        FillLineEvent(lEvent, lStart, lEnd, lColour);
        lEvent.mfRadius = lfRadius;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_CAPSULE);
    }

    void DebugRender::DrawCylinder(Vector3 lStart, Vector3 lEnd, f32 lfRadius, RGBA lColour)
    {
        Internal::CInEventDrawCylinder lEvent;
        FillLineEvent(lEvent, lStart, lEnd, lColour);
        lEvent.mfRadius = lfRadius;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_CYLINDER);
    }

    void DebugRender::DrawTriangle(Vector3 lA, Vector3 lB, Vector3 lC, RGBA lColour)
    {
        Internal::CInEventDrawTriangle lEvent;
        lEvent.mfX1 = lA.x; lEvent.mfY1 = lA.y; lEvent.mfZ1 = lA.z;
        lEvent.mfX2 = lB.x; lEvent.mfY2 = lB.y; lEvent.mfZ2 = lB.z;
        lEvent.mfX3 = lC.x; lEvent.mfY3 = lC.y; lEvent.mfZ3 = lC.z;
        lEvent.mColour = lColour;
        m3DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_3D_TRIANGLE);
    }

    void DebugRender::DrawWireTriangle(Vector3 lA, Vector3 lB, Vector3 lC, RGBA lColour)
    {
        DrawLine(lA, lB, lColour);
        DrawLine(lB, lC, lColour);
        DrawLine(lA, lC, lColour);
    }

    void DebugRender::DrawCross(Vector3 lPosition, f32 lfSize, RGBA lColour)
    {
        const f32 lfHalfSize = lfSize * 0.5f;
        DrawLine(MakeVector3(lPosition.x - lfHalfSize, lPosition.y, lPosition.z),
                 MakeVector3(lPosition.x + lfHalfSize, lPosition.y, lPosition.z), lColour);
        DrawLine(MakeVector3(lPosition.x, lPosition.y - lfHalfSize, lPosition.z),
                 MakeVector3(lPosition.x, lPosition.y + lfHalfSize, lPosition.z), lColour);
        DrawLine(MakeVector3(lPosition.x, lPosition.y, lPosition.z - lfHalfSize),
                 MakeVector3(lPosition.x, lPosition.y, lPosition.z + lfHalfSize), lColour);
    }

    void DebugRender::DrawBox(const f32* lpTransform, RGBA lColour,
                              Vector4 lMin, Vector4 lMax)
    {
        Matrix44Affine lTransform;
        lTransform.xAxis = Vector3{ lpTransform[0], lpTransform[1], lpTransform[2], lpTransform[3] };
        lTransform.yAxis = Vector3{ lpTransform[4], lpTransform[5], lpTransform[6], lpTransform[7] };
        lTransform.zAxis = Vector3{ lpTransform[8], lpTransform[9], lpTransform[10], lpTransform[11] };
        lTransform.wAxis = Vector3{ lpTransform[12], lpTransform[13], lpTransform[14], lpTransform[15] };
        DrawBox(MakeVector3(lMin.x, lMin.y, lMin.z), MakeVector3(lMax.x, lMax.y, lMax.z),
                lTransform, lColour);
    }

    void DebugRender::DrawLine(RGBA lColour, Vector3 lFrom, Vector3 lTo)
    {
        DrawLine(lFrom, lTo, lColour);
    }

    void DebugRender::DrawAxis(const f32* lpTransform)
    {
        Matrix44Affine lTransform;
        lTransform.xAxis = Vector3{ lpTransform[0], lpTransform[1], lpTransform[2], lpTransform[3] };
        lTransform.yAxis = Vector3{ lpTransform[4], lpTransform[5], lpTransform[6], lpTransform[7] };
        lTransform.zAxis = Vector3{ lpTransform[8], lpTransform[9], lpTransform[10], lpTransform[11] };
        lTransform.wAxis = Vector3{ lpTransform[12], lpTransform[13], lpTransform[14], lpTransform[15] };
        DrawAxis(lTransform);
    }

    void DebugRender::DrawSolidQuad(RGBA lColour, Vector3 lA, Vector3 lB, Vector3 lC, Vector3 lD)
    {
        DrawSolidQuad(lA, lB, lC, lD, lColour);
    }

    // ARTIST 0x8282A6F0: complete replay switch for all world-space event IDs.
    void DebugRender::Dispatch3D(Debug3DImmediateRender* lpRenderer, bool lbClear)
    {
        const char* lpcPendingString = "";
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liType = m3DQueue.GetFirstEvent(&lpEvent, &liSize);

        while (liType >= 0)
        {
            switch (liType)
            {
                case Internal::E_INEVENT_3D_STRING:
                    lpcPendingString = reinterpret_cast<const char*>(lpEvent);
                    break;
                case Internal::E_INEVENT_3D_TEXT:
                {
                    const Internal::CInEventDrawText& e = *reinterpret_cast<const Internal::CInEventDrawText*>(lpEvent);
                    lpRenderer->DrawText(MakeVector3(e.mfX, e.mfY, e.mfZ), lpcPendingString,
                                         e.mfSize, MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_LINE:
                {
                    const Internal::CInEventDrawLine& e = *reinterpret_cast<const Internal::CInEventDrawLine*>(lpEvent);
                    lpRenderer->DrawLine(MakeVector3(e.mfX1, e.mfY1, e.mfZ1),
                                         MakeVector3(e.mfX2, e.mfY2, e.mfZ2), MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_POINT:
                {
                    const Internal::CInEventDrawPoint& e = *reinterpret_cast<const Internal::CInEventDrawPoint*>(lpEvent);
                    lpRenderer->DrawPoint(MakeVector3(e.mfX, e.mfY, e.mfZ), MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_QUAD:
                case Internal::E_INEVENT_3D_SOLID_QUAD:
                {
                    const Internal::CInEventDrawQuad& e = *reinterpret_cast<const Internal::CInEventDrawQuad*>(lpEvent);
                    const Vector3 a = MakeVector3(e.mfX1, e.mfY1, e.mfZ1);
                    const Vector3 b = MakeVector3(e.mfX2, e.mfY2, e.mfZ2);
                    const Vector3 c = MakeVector3(e.mfX3, e.mfY3, e.mfZ3);
                    const Vector3 d = MakeVector3(e.mfX4, e.mfY4, e.mfZ4);
                    if (liType == Internal::E_INEVENT_3D_QUAD)
                        lpRenderer->DrawQuad(a, b, c, d, MakeImmediateColour(e.mColour));
                    else
                        lpRenderer->DrawSolidQuad(a, b, c, d, MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_ANGLE:
                {
                    const Internal::CInEventDrawAngle& e = *reinterpret_cast<const Internal::CInEventDrawAngle*>(lpEvent);
                    lpRenderer->DrawAngleRad(MakeVector3(e.mfX, e.mfY, e.mfZ), e.mfAngle,
                                             MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_AXIS:
                {
                    const Internal::CInEventDrawAxis& e = *reinterpret_cast<const Internal::CInEventDrawAxis*>(lpEvent);
                    lpRenderer->DrawAxis(MakeTransform(e));
                    break;
                }
                case Internal::E_INEVENT_3D_SPHERE:
                case Internal::E_INEVENT_3D_SOLID_SPHERE:
                case Internal::E_INEVENT_3D_HOLLOW_SPHERE:
                {
                    const Internal::CInEventDrawSphere& e = *reinterpret_cast<const Internal::CInEventDrawSphere*>(lpEvent);
                    const Vector3 lCentre = MakeVector3(e.mfX, e.mfY, e.mfZ);
                    const rw::RGBA lColour = MakeImmediateColour(e.mColour);
                    if (liType == Internal::E_INEVENT_3D_SPHERE)
                        lpRenderer->DrawSphere(lCentre, e.mfRadius, lColour);
                    else if (liType == Internal::E_INEVENT_3D_SOLID_SPHERE)
                        lpRenderer->DrawSolidSphere(lCentre, e.mfRadius, lColour);
                    else
                        lpRenderer->DrawHollowSphere(lCentre, e.mfRadius, lColour);
                    break;
                }
                case Internal::E_INEVENT_3D_CIRCLE:
                {
                    const Internal::CInEventDrawCircle& e = *reinterpret_cast<const Internal::CInEventDrawCircle*>(lpEvent);
                    lpRenderer->DrawCircle(MakeVector3(e.mfPosX, e.mfPosY, e.mfPosZ),
                                           MakeVector3(e.mfDirX, e.mfDirY, e.mfDirZ), e.mfRadius,
                                           MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_BOX:
                case Internal::E_INEVENT_3D_SOLID_BOX:
                {
                    const Internal::CInEventDrawBox& e = *reinterpret_cast<const Internal::CInEventDrawBox*>(lpEvent);
                    const Vector3 lMin = MakeVector3(e.mfInfX, e.mfInfY, e.mfInfZ);
                    const Vector3 lMax = MakeVector3(e.mfSupX, e.mfSupY, e.mfSupZ);
                    if (liType == Internal::E_INEVENT_3D_BOX)
                        lpRenderer->DrawBox(lMin, lMax, MakeBoxTransform(e), MakeImmediateColour(e.mColour));
                    else
                        lpRenderer->DrawSolidBox(lMin, lMax, MakeBoxTransform(e), MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_BOX_AA:
                case Internal::E_INEVENT_3D_SOLID_BOX_AA:
                {
                    const Internal::CInEventDrawBoxAA& e = *reinterpret_cast<const Internal::CInEventDrawBoxAA*>(lpEvent);
                    const Vector3 lMin = MakeVector3(e.mfInfX, e.mfInfY, e.mfInfZ);
                    const Vector3 lMax = MakeVector3(e.mfSupX, e.mfSupY, e.mfSupZ);
                    if (liType == Internal::E_INEVENT_3D_BOX_AA)
                        lpRenderer->DrawBox(lMin, lMax, MakeImmediateColour(e.mColour));
                    else
                        lpRenderer->DrawSolidBox(lMin, lMax, MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_ARROW:
                case Internal::E_INEVENT_3D_SOLID_ARROW:
                {
                    const Internal::CInEventDrawArrow& e = *reinterpret_cast<const Internal::CInEventDrawArrow*>(lpEvent);
                    const Vector3 lFrom = MakeVector3(e.mfX1, e.mfY1, e.mfZ1);
                    const Vector3 lTo = MakeVector3(e.mfX2, e.mfY2, e.mfZ2);
                    if (liType == Internal::E_INEVENT_3D_ARROW)
                        lpRenderer->DrawArrow(lFrom, lTo, MakeImmediateColour(e.mColour));
                    else
                        lpRenderer->DrawSolidArrow(lFrom, lTo, MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_TRIANGLE:
                {
                    const Internal::CInEventDrawTriangle& e = *reinterpret_cast<const Internal::CInEventDrawTriangle*>(lpEvent);
                    lpRenderer->DrawTriangle(MakeVector3(e.mfX1, e.mfY1, e.mfZ1),
                                             MakeVector3(e.mfX2, e.mfY2, e.mfZ2),
                                             MakeVector3(e.mfX3, e.mfY3, e.mfZ3),
                                             MakeImmediateColour(e.mColour));
                    break;
                }
                case Internal::E_INEVENT_3D_CYLINDER:
                case Internal::E_INEVENT_3D_CAPSULE:
                {
                    const Internal::CInEventDrawCylinder& e = *reinterpret_cast<const Internal::CInEventDrawCylinder*>(lpEvent);
                    const Vector3 lFrom = MakeVector3(e.mfX1, e.mfY1, e.mfZ1);
                    const Vector3 lTo = MakeVector3(e.mfX2, e.mfY2, e.mfZ2);
                    if (liType == Internal::E_INEVENT_3D_CYLINDER)
                        lpRenderer->DrawCylinder(lFrom, lTo, e.mfRadius, MakeImmediateColour(e.mColour));
                    else
                        lpRenderer->DrawCapsule(lFrom, lTo, e.mfRadius, MakeImmediateColour(e.mColour));
                    break;
                }
                default:
                    CGS_ASSERT(false, "unknown event");
                    break;
            }

            liType = m3DQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        if (lbClear)
            m3DQueue.Clear();
    }

    void DebugRender::Draw2DText(const char* lpcText, Vector2 lPosition,
                                 f32 lfScale, RGBA lColour)
    {
        Draw2DText(lpcText, lPosition.x, lPosition.y, lfScale, lColour);
    }

    void DebugRender::Draw2DLine(Vector2 lStart, Vector2 lEnd, RGBA lColour)
    {
        Draw2DLine(lStart.x, lStart.y, lEnd.x, lEnd.y, lColour);
    }

    void DebugRender::Draw2DBox(Vector2 lMin, Vector2 lMax, RGBA lColour)
    {
        Draw2DBox(lMin.x, lMin.y, lMax.x, lMax.y, lColour);
    }

    void DebugRender::Draw2DFrame(Vector2 lMin, Vector2 lMax, RGBA lColour)
    {
        Draw2DFrame(lMin.x, lMin.y, lMax.x, lMax.y, lColour);
    }

    // X360 Draw2DText 0x8282B1D0: queue the string (STRING event) then the text record (TEXT event).
    void DebugRender::Draw2DText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        if (!lpcText)
            return;

        m2DQueue.AddStringEventSafe(lpcText, Internal::E_INEVENT_2D_STRING);

        Internal::CInEventDrawText2D lEvent;
        lEvent.mfX     = lfX;
        lEvent.mfY     = lfY;
        lEvent.mfSize  = lfScale;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_TEXT, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DTextJustified 0x8282BAC0 (DWARF CgsDebugRender.cpp:672). Measure the string at
    // lfSize, shift the position LEFT by 0 / half / all of that width for LEFT / CENTRE / RIGHT,
    // then queue it exactly like Draw2DText (STRING event id 0, then the TEXT record id 1).
    //
    // The width model is the console's own and is a flat monospace estimate, not a font metric:
    //   width = strlen(text) * lfSize * 0.65        (flt_82097F40 == 0.65f, read off the image)
    //   CENTRE: x -= width * 0.5                    (flt_82001DA0 == 0.5f)
    //   RIGHT:  x -= width
    // The X360 inlines the strlen (a `lbz`/`addi`/`cmplwi` loop, no call), so it is spelled inline
    // here too rather than pulling rw::core::stdc::StringLength into this TU. Only the X lane is
    // written back (`vrlimi128 v0, v13, 8, 0` merges lane 0 alone), and the LEFT arm skips the
    // merge entirely -- so Y/Z/W ride through untouched in every case.
    //
    // ⚠️ NO NULL GUARD, deliberately: the console's first act is to dereference lpcText in the
    // length loop. Draw2DText above does guard, because its own X360 body does.
    //
    // This is the entry point BrnDirector::DebugPrinter::ActualPrint @0x821F71D8 draws every
    // Director debug line through, and the one Camera::Utils::Tweaker's readout uses.
    void DebugRender::Draw2DTextJustified(const char* lpcText, Vector2 lv2Position,
                                          Justification leJustification, f32 lfSize, RGBA lColour)
    {
        // Inlined string length (the console's own loop).
        u32 luLength = 0;
        while (lpcText[luLength] != '\0')
        {
            ++luLength;
        }

        const f32 lfWidth = static_cast<f32>(luLength) * lfSize * 0.65f;

        if (leJustification == E_JUSTIFY_CENTRE)
        {
            lv2Position.x -= lfWidth * 0.5f;
        }
        else if (leJustification == E_JUSTIFY_RIGHT)
        {
            lv2Position.x -= lfWidth;
        }

        m2DQueue.AddStringEventSafe(lpcText, Internal::E_INEVENT_2D_STRING);

        Internal::CInEventDrawText2D lEvent;
        lEvent.mfX     = lv2Position.x;
        lEvent.mfY     = lv2Position.y;
        lEvent.mfSize  = lfSize;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_TEXT, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DLine (CInEventDrawLine2D, ID 2).
    void DebugRender::Draw2DLine(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour)
    {
        Internal::CInEventDrawLine2D lEvent;
        lEvent.mfX1    = lfX0;
        lEvent.mfY1    = lfY0;
        lEvent.mfX2    = lfX1;
        lEvent.mfY2    = lfY1;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_LINE, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DBox 0x8282B2D8 (CInEventDrawBox2D, ID 3): screen rect as origin + extent + colour.
    // The record stores {mfX, mfY, mfWidth, mfHeight} (DWARF); the min/max interface converts here.
    void DebugRender::Draw2DBox(f32 lfMinX, f32 lfMinY, f32 lfMaxX, f32 lfMaxY, RGBA lColour)
    {
        Internal::CInEventDrawBox2D lEvent;
        lEvent.mfX      = lfMinX;
        lEvent.mfY      = lfMinY;
        lEvent.mfWidth  = lfMaxX - lfMinX;
        lEvent.mfHeight = lfMaxY - lfMinY;
        lEvent.mColour  = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_BOX, static_cast<s32>(sizeof(lEvent)));
    }

    void DebugRender::Draw2DFrame(f32 lfMinX, f32 lfMinY, f32 lfMaxX, f32 lfMaxY, RGBA lColour)
    {
        Internal::CInEventDrawFrame2D lEvent;
        lEvent.mfX = lfMinX;
        lEvent.mfY = lfMinY;
        lEvent.mfWidth = lfMaxX - lfMinX;
        lEvent.mfHeight = lfMaxY - lfMinY;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(&lEvent, Internal::E_INEVENT_2D_FRAME);
    }

    // X360 Dispatch2D 0x8282A4B8: walk the queue, replaying each event into lpRenderer. A STRING event
    // is held and consumed by the next TEXT event (the X360 pairs them). Clears the queue when done.
    void DebugRender::Dispatch2D(Debug2DImmediateRender* lpRenderer, bool lbClear)
    {
        if (!lpRenderer)
            return;

        const char*             lpcPendingString = "";
        const CgsModule::Event* lpEvent          = nullptr;
        s32                     liSize           = 0;
        s32                     liType           = m2DQueue.GetFirstEvent(&lpEvent, &liSize);

        while (liType >= 0)
        {
            switch (liType)
            {
                case Internal::E_INEVENT_2D_STRING:
                    lpcPendingString = reinterpret_cast<const char*>(lpEvent);
                    break;

                case Internal::E_INEVENT_2D_TEXT:
                {
                    const Internal::CInEventDrawText2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawText2D*>(lpEvent);
                    lpRenderer->DrawText(lpcPendingString, lpEv->mfX, lpEv->mfY, lpEv->mfSize, lpEv->mColour);
                    break;
                }

                case Internal::E_INEVENT_2D_LINE:
                {
                    const Internal::CInEventDrawLine2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawLine2D*>(lpEvent);
                    Vector2 lv2Start = { lpEv->mfX1, lpEv->mfY1, 0.0f, 0.0f };
                    Vector2 lv2End   = { lpEv->mfX2, lpEv->mfY2, 0.0f, 0.0f };
                    lpRenderer->DrawLine(lv2Start, lv2End, lpEv->mColour);
                    break;
                }

                case Internal::E_INEVENT_2D_BOX:
                {
                    const Internal::CInEventDrawBox2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawBox2D*>(lpEvent);
                    lpRenderer->DrawBox(lpEv->mfX, lpEv->mfY, lpEv->mfWidth, lpEv->mfHeight, lpEv->mColour);
                    break;
                }

                case Internal::E_INEVENT_2D_FRAME:
                {
                    const Internal::CInEventDrawFrame2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawFrame2D*>(lpEvent);
                    const Vector2 lv2Min = { lpEv->mfX, lpEv->mfY, 0.0f, 0.0f };
                    const Vector2 lv2Max = { lpEv->mfX + lpEv->mfWidth,
                                             lpEv->mfY + lpEv->mfHeight, 0.0f, 0.0f };
                    lpRenderer->DrawFrame(lv2Min, lv2Max, lpEv->mColour);
                    break;
                }

                default:
                    CGS_ASSERT(false, "unknown event");
                    break;
            }

            liType = m2DQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        if (lbClear)
            m2DQueue.Clear();
    }
}
