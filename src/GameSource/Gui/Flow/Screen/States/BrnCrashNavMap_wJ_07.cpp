// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 07: the map pan / scroll pair.
//   UpdateMainMap @0x824D8CB0  (BrnCrashNavMap.cpp:611-621)
//   MoveCursor    @0x824BF100  (BrnCrashNavMap.cpp:1432-1502, assert cpp:1470)
//
// Both bodies are landed, reconstructed instruction by instruction from the raw X360 asm
// (Hex-Rays renders UpdateMainMap as inline-asm blocks and flags MoveCursor "local variable
// allocation has failed" -- it drops the float arguments of both the SetDelta call and the
// tail SetPosition). Six callees were declared nowhere under b5-decomp/src when this group
// ran; those declarations have since been applied and the bodies compile against them:
//
//   BrnCursor.h  -- GuiCursor::GetPosition() (DWARF h:334, BY VALUE),
//                   SetPosition(Vector2, bool)      (h:230, @0x82428AD8),
//                   SetDelta(f32, f32, f32)         (h:100, @0x82416750),
//                   FindClosestSnapIndex(Vector2*, u32) (h:119, @0x824168B8).
//   BrnMapIconManager.h -- MapIconManager::GetSatNavIconPositions(Vector2*, s32*)
//                   (h:165, @0x8250A708), now public.
//   BrnMainMap.h -- an exposure for the single main-map world rect the X360 keeps at .data
//                   0x82FB31F0 (written by MainMapComponent Construct / Prepare /
//                   CalculatePositionedWorldRect, read by MoveCursor and by
//                   RoadSignIconManager::SetupComponent; measured this wave,
//                   scratchpad/waveJ/g07_rect.txt).
//   BrnCrashNavPanel.h -- CrashNavPanel::SetRivalPanelData() (@0x8243AAC8), the
//                   no-argument face one arm of UpdateMainMap needs.
//
// Everything else checked out under the compile gate: the member names, the CursorMode /
// SatNavIconType / EIconDisplayType enumerator spellings, MapTransform::Flatten /
// Unflatten / DeviceToWorld / WorldToDevice overload resolution, rw::math::vpu::Dot over
// Vector2, and the stripped GuiEventControllerAxis payload view.
//
// FINDING for the header owner (comment-only, no code impact): the class banner in
// BrnCrashNavMap.h documents mfMapPanningStopTime at X360 +24900, but every access in
// MoveCursor is at 0x6150 == 24912 (`stfs f1, 0x6150(r30)` @0x824BF1EC, `lfs f13,
// 0x6150(r30)` @0x824BF3F0) -- 24880 + 32, i.e. mSoundData's 16-byte Vector3 lane plus its
// 16-byte-aligned bool tail. The member ORDER is right; only the documented offset is off.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h" // CgsGui::GuiEventControllerAxis
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::Event
#include "GameSource/Gui/BrnGuiCache.h"                     // GuiCache::GetTime
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"             // SatNavIconInfo::SetIconType
#include "GameSource/Gui/SatNav/BrnMainMap.h"               // MainMapComponent::GetWorldRect
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"        // MapIconManager::GetSatNavIconPositions
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"           // BrnGui::MapTransform

#include "rw/math/vpu/vector2_operation.h"                  // rw::math::vpu::Dot (MagnitudeSquared)

namespace BrnGui
{
    namespace
    {
        // Axis ids as emitted by the controller->GUI bridge (see the banner).
        const s32 KI_AXIS_LEFT_STICK      = 0;
        const s32 KI_AXIS_RIGHT_STICK     = 1;
        const s32 KI_AXIS_ALTERNATE_STICK = 2;

        // Pan / snap constants. Names are DWARF-attested (BrnCrashNavMap.h:32/35/38/80/83);
        // values are the measured big-endian rodata words.
        const f32 KF_PAN_INPUT_FILTER_FACTOR     = 0.40000001f;  // flt_820662EC
        const f32 KF_PAN_Y_BORDER                = 200.0f;       // flt_820662F0
        const f32 FK_PAN_CURSOR_MOVEMENT_RADIUS  = 60.0f;        // flt_820662F4
        const f32 KF_PANNING_STOP_RESETTIME      = 0.20000000f;  // flt_820662F8
        const f32 KF_MAX_SNAP_SCREEN_DISTANCE    = 2500.0f;      // flt_820662FC

        // DWARF BrnCrashNavMap.cpp:1452 -- the on-stack snap-target list this screen hands
        // to the cursor. Corroborated by the X360 frame (see the banner).
        const s32 KI_SNAP_LIST_SIZE = 289;

        // CgsGui::GuiEventControllerAxis, minus the 12-byte GuiEvent header -- the in-queue
        // hands the state the stripped payload. Identical view to the one the committed
        // sibling BrnCrashNavEnterOnline_wI_05.cpp carries for the same event.
        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxis;    // +0x00
            f32 mfXAxis;   // +0x04
            f32 mfYAxis;   // +0x08
        };
    }

    // --------------------------------------------------------------- MoveCursor @0x824BF100
    //
    // Feed one analogue-stick sample into the crash-nav map. Which of the two things it does
    // depends on the stick: the right stick (axis 1) PANS the map -- the cursor is pushed
    // toward a point offset from the map centre and the world position it lands on is clamped
    // to the map's world rect -- while the left stick / alternate pair (axes 0 and 2) simply
    // hand the raw delta to the cursor, and, if the map is already panning, decide when to
    // drop back out of panning.
    //
    // Panning is left in two ways: the player deflects one of the other sticks (immediate),
    // or the pan stick has been idle for KF_PANNING_STOP_RESETTIME and the cursor has drifted
    // within snapping range of an icon.
    //
    // The whole function is inert while an event is being inspected.
    void CrashNavMap::MoveCursor(const CgsModule::Event* lpEvent)
    {
        // `cmpwi cr6, r11, 2` + `beq` straight to the epilogue.
        if (meCursorMode == E_CURSORMODE_INSPECTING_ICONS)
        {
            return;
        }

        // X360 BrnCrashNavMap.cpp:1470. Non-fatal: the payload is dereferenced immediately
        // afterwards either way. The message names the DERIVED screen (CrashNavMapMain) even
        // though the code lives in the base -- kept verbatim.
        CGS_ASSERT(lpEvent != 0, "Invalid event in CrashNavMapMain::MoveCursor");

        // cpp:1432.
        const ControllerAxisPayload* lpAxisData =
            reinterpret_cast<const ControllerAxisPayload*>(lpEvent);

        if (lpAxisData->miAxis == KI_AXIS_LEFT_STICK ||
            lpAxisData->miAxis == KI_AXIS_ALTERNATE_STICK)
        {
            if (meCursorMode == E_CURSORMODE_PANNING)
            {
                // Any deflection on a non-pan stick drops panning immediately.
                bool lbStopPanning = (lpAxisData->mfXAxis != 0.0f) ||
                                     (lpAxisData->mfYAxis != 0.0f);

                // Otherwise the pan stick has to have been idle for the reset time before the
                // cursor is even allowed to look for something to snap back to.
                if (!lbStopPanning &&
                    mfMapPanningStopTime + KF_PANNING_STOP_RESETTIME < mpGuiCache->GetTime())
                {
                    // cpp:1452-1454.
                    Vector2 lSnapList[KI_SNAP_LIST_SIZE];
                    s32     liNumIcons = 0;
                    mpIconManager->GetSatNavIconPositions(lSnapList, &liNumIcons);

                    const u32 luNearestIcon =
                        mCursor.FindClosestSnapIndex(lSnapList, static_cast<u32>(liNumIcons));

                    // `vsubfp` of the winning snap location against the cursor lane, then the
                    // 2-lane squared magnitude -- no square root is taken on either side, the
                    // threshold is already squared (50 device units).
                    //
                    // FAITHFUL, and deliberately unguarded: `slwi r10, r3, 4` @0x824BF444
                    // indexes the list with whatever FindClosestSnapIndex returned, with no
                    // test against its KU_INVALID_SNAP_INDEX (0xFFFFFFFF) sentinel and no
                    // test on liNumIcons. The X360 really does read lSnapList[-1] when the
                    // list is empty; the surrounding reset-time gate is what keeps that from
                    // being reachable in practice. Do not "fix" it without evidence.
                    const Vector2 lv2CursorPos = mCursor.GetPosition();
                    Vector2 lv2ToSnapLocation;
                    lv2ToSnapLocation.x = lSnapList[luNearestIcon].x - lv2CursorPos.x;
                    lv2ToSnapLocation.y = lSnapList[luNearestIcon].y - lv2CursorPos.y;
                    lv2ToSnapLocation.z = 0.0f;   // vsubfp is all-lane; z/w are Vector2 padding
                    lv2ToSnapLocation.w = 0.0f;

                    lbStopPanning = KF_MAX_SNAP_SCREEN_DISTANCE >
                                    rw::math::vpu::Dot(lv2ToSnapLocation, lv2ToSnapLocation);
                }

                if (lbStopPanning)
                {
                    meCursorMode = E_CURSORMODE_SELECTING_ICONS;
                }
            }
            else
            {
                // Not panning: the stick just moves the cursor. See the banner for the PPC
                // float-argument ABI note that recovers these three arguments.
                mCursor.SetDelta(lpAxisData->mfXAxis,
                                 lpAxisData->mfYAxis,
                                 mpGuiCache->GetTime());
            }
        }
        else if (lpAxisData->miAxis == KI_AXIS_RIGHT_STICK)
        {
            // Any deflection on the pan stick ENTERS panning and drops every hover latch, so
            // the panel and the button prompts stop advertising whatever was under the cursor.
            if (lpAxisData->mfXAxis != 0.0f || lpAxisData->mfYAxis != 0.0f)
            {
                meCursorMode           = E_CURSORMODE_PANNING;
                mfMapPanningStopTime   = mpGuiCache->GetTime();
                muHoveredEventID       = 0;
                mHoveredDriveThruID    = 0;
                mHoveringRivalId       = 0;
                mpLockedIconName       = 0;
                mbLocalPlayerSelected  = false;

                // `stb r11, 0x6128(r30)` with r11 == 12: a bare byte store, i.e. the setter is
                // inlined here and its range assert does not fire. Parking the "tire shop"
                // type in the locked-icon record is what makes the panel-repaint switches
                // (UpdateRival / UpdateDrivethru) take their silent no-op arm while panning.
                mLockedIconInfo.SetIconType(
                    GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_TIRE_SHOP);

                UpdateButtonPrompts();
            }

            // Re-read: the block above may have just entered panning, and a pan sample that
            // did NOT enter it still has to be integrated while panning is already active.
            if (meCursorMode == E_CURSORMODE_PANNING)
            {
                // cpp:1493. The map centre is a point on the world XZ plane; Unflatten lifts
                // it to (x, 0, z) and WorldToDevice projects it back onto the screen. `li r3,
                // 0` @0x824BF228 is WorldToDevice's `lbClamp` argument -- the vector travels
                // in v1, so r3 is the FIRST scalar parameter, not a `this`.
                const Vector2 lv2MapCentre =
                    MapTransform::WorldToDevice(MapTransform::Unflatten(mv2WorldCentrePoint), false);

                // cpp:1494. The Y axis is inverted (`fmuls f0, f13, -1.0f`) because device
                // space grows downward.
                Vector2 lv2ControllerOffset;
                lv2ControllerOffset.x = lpAxisData->mfXAxis;
                lv2ControllerOffset.y = lpAxisData->mfYAxis * -1.0f;
                lv2ControllerOffset.z = 0.0f;
                lv2ControllerOffset.w = 0.0f;

                // cpp:1495. The cursor chases a point FK_PAN_CURSOR_MOVEMENT_RADIUS device
                // units from the map centre in the stick's direction, with a first-order
                // filter so the motion eases instead of snapping.
                const Vector2 lv2CursorPos = mCursor.GetPosition();
                Vector2 lv2CursorPosToMoveTo;
                lv2CursorPosToMoveTo.x =
                    (lv2ControllerOffset.x * FK_PAN_CURSOR_MOVEMENT_RADIUS + lv2MapCentre.x -
                     lv2CursorPos.x) * KF_PAN_INPUT_FILTER_FACTOR + lv2CursorPos.x;
                lv2CursorPosToMoveTo.y =
                    (lv2ControllerOffset.y * FK_PAN_CURSOR_MOVEMENT_RADIUS + lv2MapCentre.y -
                     lv2CursorPos.y) * KF_PAN_INPUT_FILTER_FACTOR + lv2CursorPos.y;
                lv2CursorPosToMoveTo.z = 0.0f;
                lv2CursorPosToMoveTo.w = 0.0f;

                // cpp:1501/1502. Round-trip through world space so the cursor can be held
                // inside the map's own bounds: x against the rect outright, z inset by
                // KF_PAN_Y_BORDER at both ends so the cursor never reaches the top/bottom
                // edge of the map.
                Vector3 lv3CursorWorldPos = MapTransform::DeviceToWorld(lv2CursorPosToMoveTo);
                // The X360 reads the world rect as a PROCESS-WIDE quad: `lis/addi r11,
                // flt_82FB31F0; lvx128 v0, r0, r11` @0x824BF2EC..0x824BF2FC -- an absolute
                // .data address, not `this`. That global is written only by
                // MainMapComponent::Construct/Prepare/CalculatePositionedWorldRect (see the
                // FLAG in BrnMainMap.h), and on this screen the component that wrote it IS
                // mMainMapComponent, whose DWARF instance member mv4WorldRect holds the same
                // quad. So the console's global read is spelled here as the DWARF-attested
                // instance accessor on that component; it is the same value, by name.
                const Vector4 lv4WorldRect = mMainMapComponent.GetWorldRect();

                // Four `fsel`s, transcribed sign for sign (see the NaN note in the banner).
                // fsel(a, b, c) == (a >= 0.0f) ? b : c.
                f32 lfClampedX =
                    (lv4WorldRect.x - lv3CursorWorldPos.x >= 0.0f) ? lv4WorldRect.x
                                                                   : lv3CursorWorldPos.x;
                f32 lfClampedZ =
                    ((lv4WorldRect.y + KF_PAN_Y_BORDER) - lv3CursorWorldPos.z >= 0.0f)
                        ? (lv4WorldRect.y + KF_PAN_Y_BORDER)
                        : lv3CursorWorldPos.z;

                lfClampedX = (lv4WorldRect.z - lfClampedX >= 0.0f) ? lfClampedX
                                                                   : lv4WorldRect.z;
                lfClampedZ = ((lv4WorldRect.w - KF_PAN_Y_BORDER) - lfClampedZ >= 0.0f)
                                 ? lfClampedZ
                                 : (lv4WorldRect.w - KF_PAN_Y_BORDER);

                lv3CursorWorldPos.x = lfClampedX;
                lv3CursorWorldPos.z = lfClampedZ;

                // `li r4, 0` -- second argument false; the vector goes in v1.
                mCursor.SetPosition(MapTransform::WorldToDevice(lv3CursorWorldPos, false), false);
            }
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"        // SatNavIconInfo / EIconDisplayType
#include "GameSource/Gui/SatNav/BrnMainMap.h"          // MainMapComponent::Update / SetDesiredWorldCentre
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"      // BrnGui::MapTransform
#include "rw/math/vpu/vector2_operation.h"             // rw::math::vpu::Dot (Vector2)

#include <cmath>                                       // sqrtf (the de-optimised vrsqrtefp chain)

namespace BrnGui
{
    namespace
    {
        // Scroll integrator constants. Names are DWARF-attested (BrnCrashNavMap.h:59/62/65);
        // values are the measured big-endian rodata words (see the banner).
        const f32 KF_MAP_SCROLL_TOLERANCE = 10.0f;          // flt_82065B68
        const f32 KF_MAP_SCROLL_ACCEL     = 0.029999999f;   // flt_820662E0 (0x3CF5C28F)
        const f32 KF_MAX_SCROLL_SPEED     = 0.80000001f;    // unk_820662E4 (0x3F4CCCCD),
                                                            // negated form flt_8200D564
    }

    // ----------------------------------------------------------- UpdateMainMap @0x824D8CB0
    //
    // The per-frame map driver: pull the map view toward the cursor while the player is
    // selecting or panning, hand the resulting scroll state to the audio debouncer, advance
    // the map component, refresh the icons and the cursor, then repaint whichever panel face
    // the current cursor mode owns.
    //
    // The scroll model is a spring-less integrator: once the cursor sits further than
    // KF_MAP_SCROLL_TOLERANCE (10 world units) from the map centre, the centre-to-cursor
    // vector feeds an acceleration into mv2MapScrollVelocity every frame and the map is asked
    // to chase mv2WorldCentrePoint + that velocity. Inside the tolerance the velocity is
    // dropped to zero outright (no decay) and the map stops.
    void CrashNavMap::UpdateMainMap()
    {
        // cpp:611. Also the argument UpdateSoundEvents needs, which is why it is hoisted
        // out of the branch (`li r30, 0` @0x824D8CC8, `li r30, 1` @0x824D8DD8).
        bool lbMapIsScrolling = false;

        // `lwz r11, 0x5F50(r31)` + two cmpwi: only the two cursor modes that let the player
        // drive the map scroll it.
        if (meCursorMode == E_CURSORMODE_SELECTING_ICONS ||
            meCursorMode == E_CURSORMODE_PANNING)
        {
            // cpp:618 / cpp:621. The cursor lives in device space; the map centre lives in
            // the (x,z) world plane, so the cursor is projected through DeviceToWorld and
            // flattened before the two can be subtracted.
            const Vector2 lv2CursorPos = mCursor.GetPosition();
            const Vector2 lv2CursorWorld = MapTransform::Flatten(MapTransform::DeviceToWorld(lv2CursorPos));

            Vector2 lv2MapCentreToCursorWorld;
            lv2MapCentreToCursorWorld.x = lv2CursorWorld.x - mv2WorldCentrePoint.x;
            lv2MapCentreToCursorWorld.y = lv2CursorWorld.y - mv2WorldCentrePoint.y;
            lv2MapCentreToCursorWorld.z = 0.0f;   // vsubfp is all-lane; z/w are Vector2 padding
            lv2MapCentreToCursorWorld.w = 0.0f;   // and are never read back (types.h)

            // cpp:616. The X360 spells this vrsqrtefp + two Newton-Raphson steps + a
            // zero-length vsel; sqrtf of the 2-lane dot product is the sanctioned scalar
            // de-optimisation and yields the same value (including 0 for a zero delta).
            const f32 lfMagnitude =
                sqrtf(rw::math::vpu::Dot(lv2MapCentreToCursorWorld, lv2MapCentreToCursorWorld));

            if (lfMagnitude > KF_MAP_SCROLL_TOLERANCE)
            {
                // `vmaddfp v0, v9, v13, v0` -- velocity += delta * accel, then stored back
                // BEFORE the clamp (@0x824D8DF4), so the magnitude below is taken from the
                // freshly accumulated velocity.
                mv2MapScrollVelocity.x += lv2MapCentreToCursorWorld.x * KF_MAP_SCROLL_ACCEL;
                mv2MapScrollVelocity.y += lv2MapCentreToCursorWorld.y * KF_MAP_SCROLL_ACCEL;

                // cpp:617. vmaxfp(a,b) = a > b ? a : b, vminfp(a,b) = a < b ? a : b -- kept in
                // the console's operand order. The lower bound can never bite (a magnitude is
                // non-negative) but the X360 emits it, so it stays.
                f32 lfClampedMagnitude =
                    sqrtf(rw::math::vpu::Dot(mv2MapScrollVelocity, mv2MapScrollVelocity));
                if (-KF_MAX_SCROLL_SPEED > lfClampedMagnitude)
                {
                    lfClampedMagnitude = -KF_MAX_SCROLL_SPEED;
                }
                if (KF_MAX_SCROLL_SPEED < lfClampedMagnitude)
                {
                    lfClampedMagnitude = KF_MAX_SCROLL_SPEED;
                }

                // See the banner: the velocity is scaled BY the clamped magnitude, not by
                // clamped/magnitude. Measured, not inferred.
                mv2MapScrollVelocity.x *= lfClampedMagnitude;
                mv2MapScrollVelocity.y *= lfClampedMagnitude;

                // `stvx128 v0, r31, 0x6B0` -- the store lands on mMainMapComponent's own
                // mv2DesiredCentre (component-relative +1616), i.e. the inlined setter.
                Vector2 lv2DesiredCentre;
                lv2DesiredCentre.x = mv2WorldCentrePoint.x + mv2MapScrollVelocity.x;
                lv2DesiredCentre.y = mv2WorldCentrePoint.y + mv2MapScrollVelocity.y;
                lv2DesiredCentre.z = 0.0f;
                lv2DesiredCentre.w = 0.0f;
                mMainMapComponent.SetDesiredWorldCentre(lv2DesiredCentre);

                lbMapIsScrolling = true;
            }
            else
            {
                // `stvx128 v12, r31, 0x6070` with v12 == vspltisw 0: the whole lane, not a
                // decay.
                mv2MapScrollVelocity.SetZero();
            }
        }

        UpdateSoundEvents(lbMapIsScrolling);

        // The map component consumes the current centre and returns the interpolated one
        // (sret in the asm: `addi r3, r1, <buf>` + `addi r4, r31, 0x60` with the argument
        // lane in v1, then the returned lane is stored straight back).
        mv2WorldCentrePoint = mMainMapComponent.Update(mv2WorldCentrePoint);

        UpdateIconManager();
        UpdateCursorStatus();

        // The panel-repaint dispatch. Three cursor modes own a panel face; NONE and PANNING
        // leave the panel to UpdateIconManager.
        if (meCursorMode == E_CURSORMODE_SELECTING_ICONS ||
            meCursorMode == E_CURSORMODE_INSPECTING_ICONS ||
            meCursorMode == E_CURSORMODE_ZOOMEDOUT)
        {
            // `lwz r11, 0x6084` == 5 (the display-type sentinel) OR `lwz r11, 0x48` == 0
            // (no cache yet) routes away from the event panel. Kept in the console's order.
            if (meEventIconDisplayType == GuiEventEnableSatNavIcons::E_ICON_DISPLAY_TYPE_COUNT ||
                mpGuiCache == 0)
            {
                // Each of the three flags is `lbz` + `cmplwi ..., 1`, i.e. compared against
                // exactly 1; the members are bool, so the plain test is equivalent.
                if (mbSelectDriveThrus)
                {
                    UpdateDrivethru();
                }
                else if (mbSelectRivals)
                {
                    UpdateRival();
                }
                else if (mbUseRoadSigns &&
                         mpLockedIconName == 0 &&
                         mLockedIconInfo.GetIconType() ==
                             GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR)
                {
                    // Road-sign screens with nothing locked fall back to the player face of
                    // the rival panel. GetIconType is a real `bl` @0x824D8F38 (it fires the
                    // two range asserts), so the inline GetIconTypeByte() accessor would drop
                    // them -- call GetIconType().
                    mCrashNavPanel.SetRivalPanelData();
                }
            }
            else
            {
                UpdateEvent();
            }
        }
    }
}
