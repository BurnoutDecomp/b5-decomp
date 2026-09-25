// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-N3 partfile 00: construction and the two
// remaining vtable entries.
//   OnlineGameRoomPlayerInfo::OnlineGameRoomPlayerInfo  (the screen-flow pool ctor)
//   GetResourcesToLoad                                  (vtable slot 8)
//   CheckForComponents                                  (vtable slot 13)
//
// The ctor has no statement of its own on the console: it chains CrashNavMap's ctor and
// then only runs the member default constructors (component vptrs, the four menu
// components, the toggle, the toggle group, the route-info panel). The empty body below
// is exactly that -- C++ runs the same member constructors in declaration order.
//
// GetResourcesToLoad is the image-wide folded two-store body (both outputs cleared): the
// screen hands the generic loader an empty list and loads its pages itself through the
// GuiCache (CheckForCompletedLoads).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"                    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase::AppendFormat
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetWorldCameraPosition
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent::GetWorldRect / GetViewRect
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // MapTransform::Transform / GetDeviceRect
#include "rw/math/vpu/vector4_operation.h"                                // per-lane Vector4 product

#include <cmath>     // std::fabs
#include <cstring>   // std::strlen

namespace BrnGui
{
    namespace
    {
        // The apt component whose arrival places the cursor (the console hashes the
        // literal inline, then compares it with the event's component hash).
        const char KAC_CURSOR_COMPONENT[] = "cursor_mc";

        // The screen-space bound beyond which the initial cursor position is reported as
        // broken (console float constant, 10000.0f).
        const f32 KF_CRAZY_CURSOR_POSITION = 10000.0f;

        // The two vector formats the debug report prints with -- the console's
        // StrStreamBase operator<<(Vector2) / operator<<(Vector4) bodies are exactly one
        // AppendFormat with these strings. Those two operators have no home in this tree
        // yet, so the lanes are pushed through AppendFormat with the same format here.
        const char KAC_VECTOR2_FORMAT[] = "(%f, %f)";
        const char KAC_VECTOR4_FORMAT[] = "(%f, %f, %f, %f)";

        // ---- in-queue payload view ----------------------------------------------------
        // FLAG: the event this virtual receives is typed only as CgsModule::Event; the one
        // field the console reads is the apt component's name hash at payload +0x0C.
        // The three leading words are not read.
        struct ComponentEventPayload : public CgsModule::Event
        {
            u32 mauUnread[3];        // +0x00 (not read by this handler)
            u32 muComponentHash;     // +0x0C CalculateHash of the component name
        };
    }

    // ------------------------------------------------------------ ctor
    OnlineGameRoomPlayerInfo::OnlineGameRoomPlayerInfo()
    {
    }

    // ------------------------------------------------------------ GetResourcesToLoad
    void OnlineGameRoomPlayerInfo::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                                      u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = 0;
        *lpuNumberOfResources = 0;
    }

    // ------------------------------------------------------------ CheckForComponents
    // When the map cursor's apt component arrives, place the cursor over the player: the
    // cache's world camera position is flattened onto the map plane and taken from the
    // map's world rect into its view rect scaled to the device. A position beyond the
    // screen bound is reported on the debug log (with every rect that fed it) and
    // asserted, but the cursor is placed either way, then switched to icon selection.
    void OnlineGameRoomPlayerInfo::CheckForComponents(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event in OnlineGameRoomPlayerInfo::CheckForComponents");

        const u32 luCursorHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(KAC_CURSOR_COMPONENT),
            static_cast<s32>(std::strlen(KAC_CURSOR_COMPONENT)));

        if (reinterpret_cast<const ComponentEventPayload*>(lpEvent)->muComponentHash != luCursorHash)
        {
            return;
        }

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        const Vector4& lrv4CameraPosition = mpGuiCache->GetWorldCameraPosition();
        const Vector3 lv3InitialCursorPosWorldSpace = { lrv4CameraPosition.x, lrv4CameraPosition.y,
                                                        lrv4CameraPosition.z, lrv4CameraPosition.w };

        // The console streams the lane after the text; lowered to the static text.
        CGS_ASSERT(lv3InitialCursorPosWorldSpace.x == lv3InitialCursorPosWorldSpace.x,
                   "Crazy 3D position for the cursor : \n");
        CGS_ASSERT(lv3InitialCursorPosWorldSpace.z == lv3InitialCursorPosWorldSpace.z,
                   "Crazy 3D position for the cursor : \n");

        // Onto the map plane (world X/Z; the console's lane permute).
        const Vector2 lv2InitialCursorPosMapSpace = MapTransform::Flatten(lv3InitialCursorPosWorldSpace);

        const Vector2 lv2InitialCursorPosScreenSpace = MapTransform::Transform(
            lv2InitialCursorPosMapSpace,
            mMainMapComponent.GetWorldRect(),
            mMainMapComponent.GetViewRect() * MapTransform::GetDeviceRect());

        if (std::fabs(lv2InitialCursorPosScreenSpace.x) > KF_CRAZY_CURSOR_POSITION ||
            std::fabs(lv2InitialCursorPosScreenSpace.y) > KF_CRAZY_CURSOR_POSITION)
        {
            // Each line re-tests the log filter, as the console does.
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "==============================================\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "==============================================\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Dave Addis : Something has gone wrong.!!!!\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Please let me know...\n\n\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "MapTransform::Flatten( lv3InitialCursorPosWorldSpace ):";
                CgsDev::Log::gpDebugPrint->AppendFormat(KAC_VECTOR2_FORMAT,
                                                        lv2InitialCursorPosMapSpace.x,
                                                        lv2InitialCursorPosMapSpace.y);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4WorldRect = mMainMapComponent.GetWorldRect();
                *CgsDev::Log::gpDebugPrint << "mMainMapComponent.GetWorldRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat(KAC_VECTOR4_FORMAT,
                                                        lv4WorldRect.x, lv4WorldRect.y,
                                                        lv4WorldRect.z, lv4WorldRect.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4ViewRect = mMainMapComponent.GetViewRect();
                *CgsDev::Log::gpDebugPrint << "mMainMapComponent.GetViewRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat(KAC_VECTOR4_FORMAT,
                                                        lv4ViewRect.x, lv4ViewRect.y,
                                                        lv4ViewRect.z, lv4ViewRect.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4DeviceRect = MapTransform::GetDeviceRect();
                *CgsDev::Log::gpDebugPrint << "MapTransform::GetDeviceRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat(KAC_VECTOR4_FORMAT,
                                                        lv4DeviceRect.x, lv4DeviceRect.y,
                                                        lv4DeviceRect.z, lv4DeviceRect.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const Vector4 lv4ScaledViewRect =
                    mMainMapComponent.GetViewRect() * MapTransform::GetDeviceRect();
                *CgsDev::Log::gpDebugPrint << "mMainMapComponent.GetViewRect() * MapTransform::GetDeviceRect():";
                CgsDev::Log::gpDebugPrint->AppendFormat(KAC_VECTOR4_FORMAT,
                                                        lv4ScaledViewRect.x, lv4ScaledViewRect.y,
                                                        lv4ScaledViewRect.z, lv4ScaledViewRect.w);
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "lv2InitialCursorPosScreenSpace:";
                CgsDev::Log::gpDebugPrint->AppendFormat(KAC_VECTOR2_FORMAT,
                                                        lv2InitialCursorPosScreenSpace.x,
                                                        lv2InitialCursorPosScreenSpace.y);
                *CgsDev::Log::gpDebugPrint << "\n";
            }

            // The console streams the lane after the text; lowered to the static text.
            CGS_ASSERT(lv2InitialCursorPosScreenSpace.x == lv2InitialCursorPosScreenSpace.x,
                       "Crazy 2D position for the cursor : \nTELL RICH GELDARD and give the output (TTY)\n");
            CGS_ASSERT(lv2InitialCursorPosScreenSpace.y == lv2InitialCursorPosScreenSpace.y,
                       "Crazy 2D position for the cursor : \nTELL RICH GELDARD and give the output (TTY)\n");
            CGS_ASSERT(std::fabs(lv2InitialCursorPosScreenSpace.x) < KF_CRAZY_CURSOR_POSITION,
                       "Crazy position being printed in the cursor : \n");
            CGS_ASSERT(std::fabs(lv2InitialCursorPosScreenSpace.y) < KF_CRAZY_CURSOR_POSITION,
                       "Crazy position being printed in the cursor : \n");
        }

        mCursor.SetPosition(lv2InitialCursorPosScreenSpace, false);
        meCursorMode = E_CURSORMODE_SELECTING_ICONS;
    }
}
