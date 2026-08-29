#include "GameSource/Gui/SatNav/BrnMainMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // the one-shot gate log
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"       // CgsGui::GuiAccessPointers::GetGuiCache
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::GetAccessPointers
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"          // BrnGui::MapTransform (device space / world rect)
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"      // BrnGui::GuiEventShowHideSatNav (the 213 payload)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"            // BrnGui::GuiAudioTriggerEvent (SetZoom's zoom cue)

#include "rw/math/vpu/vector4_operation.h"          // rw::math::vpu::Magnitude (the Construct assert)
#include "rw/math/vpu/vector2_operation.h"          // rw::math::vpu::Magnitude (SnapToLocation cpp:405)

#include <cmath>                                    // ::fabsf (ApplyZoom's zoom-delta test)

// BrnGui::MainMapComponent - the sat-nav main-map screen component bodies.
//
// This TU reconstructs Construct @0x8245E228, Prepare @0x8244F4A8,
// SetStandardDefZoomParams @0x82447ED8, RecvEvent @0x82458370 and SetZoom @0x82469A38, and
// defines the two class-static zoom-scale tables the DWARF places inside the class
// (h:218 / h:219).
//
// ⭐ 2026-08-29 (main-menu wave G2) ADDS the SnapToLocation leg, end to end:
//   SnapToLocation             @0x8245EBA0
//   CalculatePositionedWorldRect @0x8245E5F0   (its two private collaborators, previously
//   CalculateViewPaddingOffset   @0x82447D38    listed here as blockers)
//   CalculateOffsetWorldCentre  (no address -- inlined; shape read off SnapToLocation)
// This closes the "still-unrecovered vector pipeline" the old banner named: sub_8245A080 is
// MapTransform::Transform(Vector2, Vector4, Vector4) and is bodied in BrnMapUtils.cpp.
//
// ⭐ 2026-08-29 (main-menu wave, map-pump slice) CLOSES THE CLASS. The last two members are
// bodied at the bottom of this file:
//   ApplyZoom @0x8245EE78   -- the zoom/centre animation step
//   Update    @0x824696E8   -- the per-frame world-rect pump that drives the map world render
// Both blockers the old banner named are gone: ApplyZoom now HAS a body, and
// OutputViewState<GuiEventRenderMainMap> has a mounted instantiation
// (GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface_OutputViewState_RenderMainMap_Inst.cpp,
// X360 @0x82465E50). ⚠️ THE CONDUCTOR MUST NOW DELETE the inert MainMapComponent::Update
// stand-in at BrnMainMapLinkGates.cpp:125-130 -- with it in place the link fails LNK2005 on
// exactly that one symbol.
//
// ⚠️ TWO CLAIMS FROM THE OLD BLOCKED SKELETON WERE WRONG AND ARE CORRECTED AT THE BODIES:
//   * `mMapManager.mWorldRect` is fed from mv4PaddingRect's corners taken normalised->world,
//     NOT from mv4WorldRect. (0x824697C4 loads this+0x630, not this+0x610.)
//   * the returned centre comes from CalculatePositionedWorldRect's RETURN, not from the
//     pre-reposition centre.
//
// ⭐ THE ZOOM-SCALE TABLE VALUES ARE NO LONGER UNKNOWN. The old banner said the two
// file-static float tables "are not present in the dossier or the IDA export and must not
// be guessed". They ARE present -- as .data in the image, which the export naturally does
// not carry. Read out of scratch/postfx_step9_final/envfix/work/image.bin (file offset =
// VA - 0x82000000), big-endian:
//     flt_82F259DC = 45CB2000 455AC000 451C4000 00000000 = { 6500, 3500, 2500, 0 }
//     flt_82F259EC = 459C4000 452BE000 447A0000 00000000 = { 5000, 2750, 1000, 0 }
// flt_82F259DC is the LIVE table (mfZoomScalFactors) and flt_82F259EC the standard-def
// source (mfStandardDefZoomScalFactors) -- pinned by SetStandardDefZoomParams @0x82447ED8,
// which copies EC -> DC element by element, and by Construct's `lfs f0,
// (flt_82F259E4 - 0x82F259DC)(r10)` == mfZoomScalFactors[E_ZOOMFACTOR_HIGH] (index 2).
//
// ⚠️ MOUNT NOTE FOR THE CONDUCTOR (measured, stunt-race UI wave 2026-08-27). This TU is
// UNMOUNTED today and GameSource/Gui/SatNav/BrnMainMapLinkGates.cpp currently carries an
// inert stand-in for MainMapComponent::RecvEvent. Those two must NEVER coexist -- the
// real RecvEvent lives HERE (:below) and the gate would be a second definition (LNK2005).
// Mounting this TU closes MainMapComponent::{Construct, Prepare, RecvEvent} and opens
// exactly ONE new external, BrnGui::MapManager::RecvEvent @0x8244F898 (declared
// BrnMapManager.h:68, bodied only in the unmounted BrnMapManager.cpp). MapManager::Construct
// @0x82458590 -- the OTHER MapManager entry this TU would need -- is neither declared in
// BrnMapManager.h nor bodied anywhere, so it is routed through the local FLAG boundary
// below rather than left as a second hole.

namespace BrnGui
{
    // ---- the two class-static zoom-scale tables (DWARF BrnMainMap.h:218 / :219) --------
    // X360 .data flt_82F259DC / flt_82F259EC; see the banner for the raw bit patterns.
    // mfZoomScalFactors is indexed by ZoomFactor: LOW / MEDIUM / HIGH / CUSTOM, and the
    // CUSTOM slot is a genuine 0.0f on the console (SetZoom overwrites it with the
    // caller's custom scale before use) -- NOT a placeholder.
    f32 MainMapComponent::mfZoomScalFactors[4]            = { 6500.0f, 3500.0f, 2500.0f, 0.0f };
    f32 MainMapComponent::mfStandardDefZoomScalFactors[4] = { 5000.0f, 2750.0f, 1000.0f, 0.0f };

    namespace
    {
        // BrnMainMap.cpp:96 -- the console's own guard constant (flt_820068C0 = 1000000.0f).
        const f32 KF_MAX_WORLD_RECT_MAGNITUDE = 1000000.0f;
        // The vperm/vaddfp rect-centre halving constant in Prepare (flt_82001DA0 = 0.5f).
        const f32 KF_RECT_CENTRE_SCALE = 0.5f;
        // flt_82F25AD4 = 0x3FE38E39 = 1.7777778f, the 16:9 aspect SetZoom folds into the
        // world rect's z lane (and that Update passes to ApplyZoom as its scale argument).
        // Read from the image, not invented -- see SetZoom's banner.
        const f32 KF_WORLD_RECT_ASPECT = 1.7777778f;
        // The GuiAudioTriggerEvent action enum every menu/map cue in the family uses
        // (X360 `li r4, 7` at every Construct call site).
        const s32 KI_AUDIO_ACTION_MENU_CUE = 7;

        // ---- ApplyZoom's animation constants, ALL read from the raw image ----------------
        // (scratch/postfx_step9_final/envfix/work/image.bin, file offset = VA - 0x82000000,
        //  big-endian; none of these is a placeholder or a guess.)
        // flt_82F25C6C = 42C80000. Serves BOTH roles in ApplyZoom: the per-frame zoom step
        // and the "|desired - live| is small enough to stop stepping" threshold.
        const f32 KF_ZOOM_STEP_PER_FRAME = 100.0f;
        // flt_82F25C68 = 3F800000. World-units epsilon for "the centre has arrived".
        const f32 KF_CENTRE_SETTLE_EPSILON = 1.0f;
        // flt_82004EF4 = 40800000. The centre chases the desired centre by 1/4 per frame
        // while the zoom is still stepping...
        const f32 KF_CENTRE_CHASE_DIVISOR_ZOOMING = 4.0f;
        // ...and flt_82001C98 = 3F800000, i.e. the whole way, once it has settled. The
        // console runs its inlined reciprocal helper on this literal 1.0f; see ApplyZoom.
        const f32 KF_CENTRE_CHASE_DIVISOR_SETTLED = 1.0f;

        // The console's `fabs` (X360 `fabs f11, f13` @0x8245EEE8), spelled out so the
        // signed-zero and NaN behaviour is the library's, not a hand-rolled compare.
        inline f32 FAbs(f32 lfValue)
        {
            return ::fabsf(lfValue);
        }

        // Same one-shot logger shape as BrnMainMapLinkGates.cpp / BrnFriendsListLinkGates.cpp:
        // an inert stand-in must never be scored as a silent success.
        void LogGateOnce(bool& lrbLogged, const char* lpacSymbol)
        {
            if (lrbLogged)
            {
                return;
            }
            lrbLogged = true;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[mainmap-link-gate] " << lpacSymbol
                    << ": inert stand-in, no body anywhere in the tree [FLAG link scaffold]\n";
            }
        }

    }

    // (The MapManager::Construct @0x82458590 FLAG boundary that stood here is RETIRED
    // 2026-08-29: the method is declared in BrnMapManager.h and bodied in
    // BrnMapManager.cpp, and Construct below calls it directly. Its one factual error is
    // corrected at that body -- the {0,0,1,1} it attributed to mWorldRect is actually
    // mLowResTexture.mBB at +0x24.)

    // -------------------------------------------------------------------------
    // FLAG BOUNDARY -- the GuiCache high-definition byte at X360 GuiCache +0x4B49 (19273).
    //
    // Construct's tail reads it (`lbz r11, 0x4B49(r10)` @0x8245E5D4) and takes the
    // standard-def zoom table when it is CLEAR. BrnGuiCache.h has no member there yet: the
    // byte falls inside `mPad_4B44[6]` (BrnGuiCache.h:1172), and this TU may not carve the
    // cache header, so the read is routed through this boundary.
    //
    // WHAT THE BYTE IS, measured across all four consumers in the export set -- every one of
    // them picks an HD constant when SET and an SD constant when CLEAR:
    //   * MainMapComponent::Construct @0x8245E5D4 -- clear => SetStandardDefZoomParams(),
    //     i.e. the live zoom table becomes {5000, 2750, 1000, 0} instead of {6500, 3500,
    //     2500, 0}. This is the strongest witness: the SD table is literally named.
    //   * CrashNavMapMain::HandleCrashNavInputPressed @0x824CCC74 -- set => SetZoom custom
    //     9000.0f, clear => 12000.0f.
    //   * CrashNavDriverDetails::UpdateWFInit @0x824BFEB8 -- set => LicenseComponent
    //     position unk_82FB4A90, clear => unk_82FB4C00.
    //   * RoadSignIconManager::Update @0x82517014 -- set => the alternate distance-fade pair
    //     at unk_82F27FA8/AC; BootLegal::Update @0x824778D8 gates the HD-composite transin.
    // There is NO writer anywhere in the export set (grepped every `st?` to 0x4B49 across
    // .ida-exports: zero hits), so the producer is a dyn-init or an unexported path -- the
    // recurring "a .bss zero does not mean the console value is zero" trap. The stand-in
    // therefore does NOT read the byte.
    //
    // [FLAG PC-only stand-in] Returns true because this host IS the HD path:
    // GuiModule::Construct installs the HD sat-nav rect and constructs the GUI resource
    // module with HighDef == true (BrnGuiModule.cpp:1062 / :1140). Returning false here
    // would silently retune every map zoom to the SD table.
    // DELETE-WHEN BrnGuiCache.h carves that byte as a named member (suggested
    // `bool mbIsHighDef;  // +0x4B49 (19273)`, out of mPad_4B44, WITHOUT shifting
    // mbInEventColouringGate at +0x4B4A) with an accessor -- then this reads
    // `lpGuiCache->IsHighDef()` and the boundary goes.
    // -------------------------------------------------------------------------
    namespace MainMapCacheBoundary
    {
        bool IsHighDef(const GuiCache* /*lpGuiCache*/)
        {
            static bool sbLogged = false;
            LogGateOnce(sbLogged, "BrnGui::GuiCache high-definition byte (+0x4B49)");
            return true;
        }
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::Construct
    //
    // X360 ARTIST @0x8245E228 (json name field verified: BrnGui::MainMapComponent::Construct).
    // Brings the map component up: the two argument asserts, the GuiComponent base
    // construction under the name "MainMap", the GuiCache latch, the embedded MapManager,
    // the default zoom selection, the caller's view/padding rects and map type, the
    // device-space display rect derived from the padding rect, and the world rect + its
    // sanity assert. Finishes with the flag block and the standard-def zoom fallback.
    //
    // ⭐ THE WORLD RECT. The X360 reads flt_82FB31F0 == MapTransform::smv4WorldRect (the
    // fixed, const world-space rect {-4375.42, -5842.42, 5363.15, 3904.74}, already homed
    // at BrnMapUtils.cpp) and copies it into the instance member the DWARF declares. Every
    // other `flt_82FB31F0` site in the export set (Prepare 0x8244F4D0, this function
    // 0x8245E514, CalculatePositionedWorldRect 0x8245E61C, CrashNavMap::MoveCursor
    // 0x824BF2F4, RoadSignIconManager::SetupComponent 0x8250AEFC) is an `addi` address
    // formation feeding an `lvx`/`lfs` -- i.e. a READ. Nothing writes it. (The old
    // BrnMainMap.h banner called three of them WRITERS; that is corrected there this wave.)
    //
    // ⭐ THE DISPLAY RECT. The 40-instruction VMX block at 0x8245E430..0x8245E518 is two
    // MapTransform::Transform applications, inlined and interleaved: the padding rect's two
    // corners (x,y) and (z,w) are each promoted to the homogeneous (px, py, 1) form by
    // `vrlimi128 v,v13,2,0` (v13 == vcfsx of vspltisw 1 == 1.0f) and multiplied against the
    // three rows of unk_82FB3050 == MapTransform::smm33DeviceSpace, exactly the
    // `p.x*xAxis + p.y*yAxis + zAxis` that BrnMapUtils.cpp's Transform performs. The two
    // results are re-interleaved by the perm masks at unk_82CDA350 / unk_82CDA3C0 plus a
    // `vsldoi ...,8` into {minX, minY, maxX, maxY}. Mask bytes read from the image:
    // 82CDA350 = {0,1,2,3, 20,21,22,23, 0,1,2,3, 0,1,2,3} (word0 of A, word1 of B, word0,
    // word0) and 82CDA3C0 = {0,1,2,3, 0,1,2,3, 0,1,2,3, 20,21,22,23}.
    // -------------------------------------------------------------------------
    void MainMapComponent::Construct(CgsGui::StateInterface* lpStateInterface,
                                     MainMapParameterBundle* lpParameters)
    {
        // cpp:65 / cpp:66 -- both are streamed asserts on the console (BeginAssert +
        // StrStreamBase + FireAssert); the message text is the console's own.
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface pointer");
        CGS_ASSERT(lpParameters != 0, "Invalid data pointer");

        // X360 `bl CgsGui__GuiComponent__Construct` with r6 == 0: no parent name.
        CgsGui::GuiComponent::Construct("MainMap", lpStateInterface, 0);

        // The X360 inlines both accessors (the "mpAccessPointers != NULL" assert at
        // CgsGuiStateInterface.h:344 and the "mpGuiCache" assert at CgsGuiShared.h:201 are
        // theirs); restored as the real calls, which already carry those asserts.
        mpGuiCache = lpStateInterface->GetAccessPointers()->GetGuiCache();

        // X360 `bl BrnGui__MapManager__Construct` @0x8245E3AC on the embedded manager.
        // ⭐ REAL AS OF 2026-08-29 (main-menu wave D1): the FLAG boundary that used to stand
        // here is gone -- MapManager::Construct @0x82458590 is bodied in BrnMapManager.cpp,
        // which is also what makes the real MapManager::RecvEvent safe (it derefs
        // mpAllocator, and this is the only thing that sets it).
        mMapManager.Construct(lpStateInterface);

        // The console's default: E_ZOOMFACTOR_HIGH, with both zoom scalars seeded from that
        // slot of the live table (`lfs f0, (flt_82F259E4 - flt_82F259DC)(r10)` -- index 2).
        meCurrentZoomFactor      = E_ZOOMFACTOR_HIGH;
        mfDesiredWorldZoomFactor = mfZoomScalFactors[E_ZOOMFACTOR_HIGH];
        mfWorldZoomScaleFactor   = mfZoomScalFactors[E_ZOOMFACTOR_HIGH];

        // The caller's bundle, in the console's store order (view rect, padding rect, type).
        mv4ViewRect    = lpParameters->mv4ViewRect;
        mv4PaddingRect = lpParameters->mv4PaddingRect;
        meMapType      = lpParameters->meMapType;

        // The padding rect, taken into device space corner by corner (see the banner).
        const Matrix33 lm33DeviceSpace = MapTransform::GetDeviceSpace();
        const Vector2  lv2PaddingMin   = { mv4PaddingRect.x, mv4PaddingRect.y, 0.0f, 0.0f };
        const Vector2  lv2PaddingMax   = { mv4PaddingRect.z, mv4PaddingRect.w, 0.0f, 0.0f };
        const Vector2  lv2DisplayMin   = MapTransform::Transform(lv2PaddingMin, lm33DeviceSpace);
        const Vector2  lv2DisplayMax   = MapTransform::Transform(lv2PaddingMax, lm33DeviceSpace);

        mv4DisplayRect.x = lv2DisplayMin.x;
        mv4DisplayRect.y = lv2DisplayMin.y;
        mv4DisplayRect.z = lv2DisplayMax.x;
        mv4DisplayRect.w = lv2DisplayMax.y;

        mv4WorldRect = MapTransform::GetWorldRect();

        // cpp:96 -- the console's own sanity check on the world rect it just installed. The
        // vendor Magnitude (vector4_operation_inline.h) carries the console's exact semantics
        // including the vsel zero-dot guard (sqrt(0) == 0).
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        mbIsZooming  = false;
        mpMapManager = 0;

        // The map starts pinned to every screen edge and active; PreRaceFlyByState::OnEnter
        // immediately clears all four via SetStickMapToScreenEdges(false, false, false, false).
        mbStickMapUp    = true;
        mbStickMapDown  = true;
        mbStickMapLeft  = true;
        mbStickMapRight = true;
        mbIsActive      = true;

        // X360 `lbz r11, 0x4B49(mpGuiCache)`: the cache's high-definition byte. Clear ==
        // standard definition, which swaps the whole zoom-scale table for the SD one.
        if (!MainMapCacheBoundary::IsHighDef(mpGuiCache))
        {
            SetStandardDefZoomParams();
        }
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::Prepare
    //
    // X360 ARTIST @0x8244F4A8 (json name field verified: BrnGui::MainMapComponent::Prepare).
    // Marks the component active and parks the desired world centre at the centre of the
    // fixed world rect. Then returns true unconditionally (`li r3, 1` at 0x8244F4DC).
    //
    // The whole VMX body is one rect-centre: `lvx` of flt_82FB31F0 ==
    // MapTransform::smv4WorldRect, four `vspltw` splats of its lanes, `vaddfp` of lane0+lane2
    // and lane1+lane3, and a multiply by the scalar at flt_82001DA0. That scalar is read from
    // the image (file offset 0x1DA0, big-endian 3F000000) == 0.5f -- NOT a placeholder.
    //
    // ⭐ THE STORE IS A FULL QUADWORD WITH A DUPLICATED X LANE, and it is transcribed as such
    // rather than tidied to {x, y, 0, 0}. The final `vperm v0, v0, v13, v7` uses the mask at
    // unk_82CDA350 = {0,1,2,3, 20,21,22,23, 0,1,2,3, 0,1,2,3}: word0 and word1 take the two
    // centre components, and words 2 AND 3 take a SECOND copy of the x component. The
    // `stvx128 v0, r8, 0x650` then commits all four lanes to mv2DesiredCentre. Nothing in the
    // recovered set reads .z/.w of that member, but the console value is x, not 0.
    // -------------------------------------------------------------------------
    bool MainMapComponent::Prepare()
    {
        mbIsActive = true;

        const Vector4& lrv4WorldRect = MapTransform::GetWorldRect();

        const f32 lfCentreX = (lrv4WorldRect.x + lrv4WorldRect.z) * KF_RECT_CENTRE_SCALE;
        const f32 lfCentreY = (lrv4WorldRect.y + lrv4WorldRect.w) * KF_RECT_CENTRE_SCALE;

        mv2DesiredCentre.x = lfCentreX;
        mv2DesiredCentre.y = lfCentreY;
        mv2DesiredCentre.z = lfCentreX;   // vperm lane 2 -- the console's duplicated x
        mv2DesiredCentre.w = lfCentreX;   // vperm lane 3 -- ditto

        return true;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::SetStandardDefZoomParams
    //
    // X360 ARTIST @0x82447ED8. Copies the standard-definition zoom-scale table over the live
    // one, element by element. The loop carries an inlined array-accessor bounds assert
    // (BrnMainMap.h:265) whose counter is pre-incremented, so it is checked against
    // E_ZOOMFACTOR_COUNT AFTER the store and can never fire for the four in-range indices --
    // reproduced as written rather than hoisted, because the console's check order is the
    // observable part.
    // -------------------------------------------------------------------------
    void MainMapComponent::SetStandardDefZoomParams()
    {
        s32 liEnumIndex = 0;
        for (s32 liZoom = 0; liZoom < E_ZOOMFACTOR_COUNT; ++liZoom)
        {
            ++liEnumIndex;
            mfZoomScalFactors[liZoom] = mfStandardDefZoomScalFactors[liZoom];
            CGS_ASSERT(liEnumIndex <= E_ZOOMFACTOR_COUNT,
                       "leEnumIndex <= MainMapComponent::E_ZOOMFACTOR_COUNT");
        }
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::RecvEvent
    //
    // X360 ARTIST @0x82458370. Asserts the event is non-null, then -- on the show/hide-satnav
    // event (BrnGui::GuiEventShowHideSatNav, id 213: `cmpwi cr6, r26, 0xD5`) targeting the
    // MAIN map (`lwz r11,0(event); cmpwi cr6, r11, 0` == GetMapType() == E_MAPTYPE_MAIN) --
    // latches the event's show flag (`lbz r11,8(event)` == GetShow()) into the embedded
    // MapManager's mbEnabled (`stb r11,0x5F0(this)`), and finally forwards the event to
    // MapManager::RecvEvent unconditionally. The 12-byte payload's members are DWARF-attested
    // (BrnGuiDemangledEventTypes.h:751); the wJ tree constructs this exact type by name at
    // BrnPreRaceFlyBy_wJ_04.cpp:287.
    //
    // ⚠️ MapManager::RecvEvent @0x8244F898 is this TU's ONE unresolved external (bodied only
    // in the unmounted BrnMapManager.cpp; an inert gate covers it in BrnMainMapLinkGates.cpp).
    // It is left as a REAL call deliberately -- the mount shape is the conductor's call.
    // -------------------------------------------------------------------------
    void MainMapComponent::RecvEvent(const CgsModule::Event* lpEvent, int32_t liEventType)
    {
        CGS_ASSERT(lpEvent != NULL, " invalid event passed ");

        const GuiEventShowHideSatNav* lpSatNav =
            reinterpret_cast<const GuiEventShowHideSatNav*>(lpEvent);

        // [DIAG-TEMP] NOT IN THE X360 BINARY -- [map-recv] what the component actually sees.
        {
            static s32 siEvents = 0;
            static s32 siLeft213 = 8;
            ++siEvents;
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                if (liEventType == 213 && siLeft213 > 0)
                {
                    --siLeft213;
                    *CgsDev::Log::gpDebugPrint
                        << "[map-recv] 213 SEEN maptype=" << static_cast<s32>(lpSatNav->GetMapType())
                        << " show=" << static_cast<s32>(lpSatNav->GetShow())
                        << " (events so far " << siEvents << ")\n";
                }
                if (siEvents == 1 || siEvents == 500 || siEvents == 5000)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[map-recv] event #" << siEvents << " type=" << liEventType << "\n";
                }
            }
        }

        if (liEventType == lpSatNav->GetEventType() &&
            lpSatNav->GetMapType() == GuiEventShowHideSatNav::E_MAPTYPE_MAIN)
        {
            mMapManager.mbEnabled = lpSatNav->GetShow();
        }

        mMapManager.RecvEvent(lpEvent, liEventType);
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::SetZoom
    //
    // X360 ARTIST @0x82469A38 (json name field verified). Selects a zoom factor; on the
    // CUSTOM factor it also writes the caller's scale into the live table and chirps the
    // zoom-in / zoom-out cue; with lbApplyNow it rebuilds mv4WorldRect immediately and snaps
    // BOTH zoom scalars, otherwise it only sets the DESIRED scalar and lets ApplyZoom
    // animate towards it.
    //
    // ⭐ THE PPC FLOAT-ARG GPR SKIP (THE recurring campaign bug) IS PRESENT HERE. The
    // console signature is `(this=r3, leZoomFactor=r4, lfCustomZoom=f1, lbApplyNow=r6)` --
    // r5 is DEAD because the float consumes its GPR slot, which is why IDA prints a phantom
    // `int a4` between the float and the bool. The DWARF-declared C++ shape is the
    // three-parameter one in BrnMainMap.h:115; do not add a fourth.
    //
    // ⭐ THE AUDIO ARM IS AN EQUALITY TRIPLE, NOT A TWO-WAY BRANCH. `fcmpu f31, table[3]`
    // is tested twice: `blt` -> "CodeMapZoomIn", `bgt` -> "CodeMapZoomOut", and EQUAL falls
    // through to the store with NO event posted. The record is the same
    // (action 7, componentName "", label, movieName "") shape the fly-by's
    // UpdateIconManager posts (BrnPreRaceFlyBy_wJ_01.cpp:418); r5 is the shared empty-string
    // sentinel unk_820046A7 (the NUL terminating "%s%s%s" at 0x820046A0 -- read from the
    // image, and already identified as "" by that TU).
    //
    // ⭐ THE WORLD RECT IS BUILT FROM THE *OLD* SCALE. The `lfs f0, 0x660(r31)` that feeds
    // the multiply at 0x82469B18 is mfWorldZoomScaleFactor read BEFORE the two `stfs f31`
    // stores at 0x82469C3C/0x82469C40 replace it. Transcribed in that order deliberately --
    // hoisting the snap above the rect build would change the frame's rect.
    //
    // ⭐ THE `.z * 16/9` IS A LANE-INSERT vperm, DECODED, NOT GUESSED. `lvx128 v7, r9, 0xA0`
    // with r9 == unk_8327F140 reads the engine-wide lane-insert permute table whose rows sit
    // at [0x8327F140 + lane*0x40 + srcword*0x10] (homed at Wheel.cpp:500-512, recovered by
    // emulating the static-init writer bank 0x82C74000). 0xA0 == lane 2, srcword 2, i.e.
    // "take dest word 2 from src2's word 2" -- so exactly the z lane of (rect * 16/9)
    // survives and x/y/w keep the unscaled product. flt_82F25AD4 == 0x3FE38E39 ==
    // 1.7777778f, read from the image; the identical constant is what Update passes to
    // ApplyZoom, so it is the aspect ratio, not a magic scale.
    // -------------------------------------------------------------------------
    void MainMapComponent::SetZoom(ZoomFactor leZoomFactor, f32 lfCustomZoom, bool lbApplyNow)
    {
        // X360 `stw r4, 0x668(r31)` -- unconditionally, before the CUSTOM test.
        meCurrentZoomFactor = leZoomFactor;

        if (leZoomFactor == E_ZOOMFACTOR_CUSTOM)
        {
            // `lfsx f0, (r4<<2), flt_82F259DC` == mfZoomScalFactors[E_ZOOMFACTOR_CUSTOM].
            const f32 lfExistingZoom = mfZoomScalFactors[leZoomFactor];

            const char* lpacLabel = 0;
            if (lfCustomZoom < lfExistingZoom)
            {
                lpacLabel = "CodeMapZoomIn";     // a SMALLER world scale == closer in
            }
            else if (lfCustomZoom > lfExistingZoom)
            {
                lpacLabel = "CodeMapZoomOut";
            }

            if (lpacLabel != 0)
            {
                GuiAudioTriggerEvent lZoomCue;
                lZoomCue.Construct(KI_AUDIO_ACTION_MENU_CUE, "", lpacLabel, "");
                mpStateInterface->OutputGuiEvent(lZoomCue);
            }

            // `lwz r11, 0x668(r31)` -- the console re-reads the member rather than reusing
            // r4, so the index is meCurrentZoomFactor, which it has just written.
            mfZoomScalFactors[meCurrentZoomFactor] = lfCustomZoom;
        }

        const ZoomFactor leActiveZoom = meCurrentZoomFactor;

        if (!lbApplyNow)
        {
            // The 0x82469C50 tail: desired only, no rect rebuild.
            mfDesiredWorldZoomFactor = mfZoomScalFactors[leActiveZoom];
            return;
        }

        const f32 lfZoom = mfZoomScalFactors[leActiveZoom];

        // cpp:456 -- the console's own cache assert (it derefs nothing here, but the check
        // is part of the body and is reproduced faithfully).
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        // mv4WorldRect = mv4ViewRect * <the still-current scale>, then the z lane alone
        // scaled by the 16:9 aspect (see the vperm note in the banner).
        mv4WorldRect.x = mv4ViewRect.x * mfWorldZoomScaleFactor;
        mv4WorldRect.y = mv4ViewRect.y * mfWorldZoomScaleFactor;
        mv4WorldRect.z = mv4ViewRect.z * mfWorldZoomScaleFactor * KF_WORLD_RECT_ASPECT;
        mv4WorldRect.w = mv4ViewRect.w * mfWorldZoomScaleFactor;

        // cpp:461 -- same guard Construct fires, on the rect this function just installed.
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        // `stfs f31, 0x664` then `stfs f31, 0x660` -- desired first, then live.
        mfDesiredWorldZoomFactor = lfZoom;
        mfWorldZoomScaleFactor   = lfZoom;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::CalculateViewPaddingOffset
    //
    // X360 ARTIST @0x82447D38 (json name field verified). A leaf: no calls, 104 lines of
    // VMX. It returns the 2D offset between the CENTRE of the view rect and the CENTRE of
    // the padding rect -- i.e. how far the padded map view sits off the plain view centre.
    //
    // DECODE. The whole body is the same four-instruction idiom twice (once per rect):
    //   lvsl v,0,{0,4,8,12} + vspltw w0   -> the four lane-SELECT permutes (.x/.y/.z/.w)
    //   vperm/vsubfp                      -> (rect.z - rect.x) and (rect.w - rect.y)
    //   vmaddfp with the splat of flt_82001DA0
    // flt_82001DA0 is read from the image (file offset 0x1DA0, big-endian 3F000000) == 0.5f
    // -- the SAME constant Prepare's rect-centre halving uses. The vmaddfp is
    // `min + 0.5 * (max - min)`, the rect centre; the two centres are then subtracted lane
    // by lane with two scalar `fsubs` and returned through the sret pointer with the z/w
    // lanes explicitly zeroed (`std r11, 0(var_48)`).
    //
    // ⭐ THE vmaddfp OPERAND ORDER IS PINNED, NOT ASSUMED. IDA prints the VMX128 multiply-add
    // with the addend in the THIRD printed slot here (`vmaddfp v13, v4, v11, v13` == 0.5 *
    // (max-min) + min). The cross-check is the rsqrt Newton-Raphson refinement in the sibling
    // SnapToLocation @0x8245EBA0, which uses the identical printed shape: only the
    // addend-third reading yields the textbook `y + 0.5*y*(1 - x*y*y)`; the other reading
    // yields 0.5*y*y + (1 - x*y*y), which is dimensionally impossible for a reciprocal square
    // root. Both readings resolve the same way, and the rect-centre semantics the function's
    // NAME states fall out of it.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::CalculateViewPaddingOffset()
    {
        // `lvx128 v13, r0, this+0x620` == mv4ViewRect; `... this+0x630` == mv4PaddingRect.
        const f32 lfViewCentreX =
            mv4ViewRect.x + (mv4ViewRect.z - mv4ViewRect.x) * KF_RECT_CENTRE_SCALE;
        const f32 lfViewCentreY =
            mv4ViewRect.y + (mv4ViewRect.w - mv4ViewRect.y) * KF_RECT_CENTRE_SCALE;

        const f32 lfPaddingCentreX =
            mv4PaddingRect.x + (mv4PaddingRect.z - mv4PaddingRect.x) * KF_RECT_CENTRE_SCALE;
        const f32 lfPaddingCentreY =
            mv4PaddingRect.y + (mv4PaddingRect.w - mv4PaddingRect.y) * KF_RECT_CENTRE_SCALE;

        Vector2 lv2Offset;
        lv2Offset.x = lfViewCentreX - lfPaddingCentreX;
        lv2Offset.y = lfViewCentreY - lfPaddingCentreY;
        lv2Offset.z = 0.0f;   // the console's explicit `std` of the z/w pair
        lv2Offset.w = 0.0f;
        return lv2Offset;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::CalculatePositionedWorldRect
    //
    // X360 ARTIST @0x8245E5F0 (json name field verified). Re-centres mv4WorldRect on
    // lv2Centre WITHOUT changing its size, clamping it back inside the fixed world rect on
    // whichever screen edges are "stuck", and returns the centre of the rect it installed.
    //
    // ⭐ IT DOES NOT TOUCH mv4PaddingRect. The Update skeleton banner below said "rebuilds
    // mv4WorldRect + mv4PaddingRect"; the asm's only `stvx128 ..., r0, r30` target is
    // r30 == this + 0x610 == mv4WorldRect (nine of them), and this+0x630 is never written.
    // Corrected here rather than left to trip the Update follow-on.
    //
    // DECODE, in the console's order:
    //   CalculateViewPaddingOffset()                       -> the normalised-space pad
    //   sub_8245A080(pad, smv4NormalizedRect, mv4WorldRect) -> that pad in WORLD units
    //     (v2 == from == unk_82FB3660, v3 == to == this+0x610; the from/to register roles
    //      are the ones BrnMapUtils.cpp's rect-pair overload already documents)
    //   width  = mv4WorldRect.z - mv4WorldRect.x            (taken BEFORE .x moves)
    //   mv4WorldRect.x = lv2Centre.x - width * 0.5f         (flt_82001DA0 again)
    //   if (mbStickMapLeft ) .x = max(.x, smv4WorldRect.x + padWorld.x)   // lbz 0x67A
    //   if (mbStickMapRight) .x = min(.x, smv4WorldRect.z - width)        // lbz 0x67B
    //   mv4WorldRect.z = mv4WorldRect.x + width
    //   height = mv4WorldRect.w - mv4WorldRect.y            (AFTER the .z rebuild)
    //   mv4WorldRect.y = lv2Centre.y - height * 0.5f
    //   if (mbStickMapUp  ) .y = max(.y, smv4WorldRect.y + padWorld.y)    // lbz 0x678
    //   if (mbStickMapDown) .y = min(.y, smv4WorldRect.w - height)        // lbz 0x679
    //   mv4WorldRect.w = mv4WorldRect.y + height
    //   assert Magnitude(mv4WorldRect) < 1000000.0f                       // cpp:376
    //   return centre(mv4WorldRect)
    //
    // ⭐ THE LEFT/RIGHT ASYMMETRY IS THE CONSOLE'S. The two "low edge" clamps add the world
    // pad (`vaddfp v11, splat(smv4WorldRect.x), splat(padWorld.x)` @0x8245E698 and the .y
    // twin @0x8245E850) while the two "high edge" clamps subtract only the extent
    // (`vsubfp v13, splat(smv4WorldRect.z), splat(width)` @0x8245E700). Transcribed as
    // written; do not "symmetrise" it.
    //
    // ⭐ EVERY LANE WRITE IS A vperm LANE-INSERT off the engine-wide table at
    // [0x8327F140 + lane*0x40 + srcword*0x10] (homed at Wheel.cpp:500): rows 0x00 / 0x40 /
    // 0x80 / 0xC0 are lanes 0/1/2/3 with source word 0, which is why each scalar is first
    // splatted across a whole stack quadword before the insert. On the host those are plain
    // member-lane assignments, so the other three lanes are preserved exactly as the console
    // preserves them.
    //
    // The `fsel f0, f11, f0, f13` / `fsel f0, f11, f13, f0` pairs are max / min with the
    // console's tie-to-the-first-operand behaviour (fsel takes the second operand when the
    // difference is >= 0), reproduced as >= comparisons rather than std::max/min.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::CalculatePositionedWorldRect(Vector2 lv2Centre)
    {
        const Vector4& lrv4FixedWorldRect = MapTransform::GetWorldRect();

        const Vector2 lv2Padding = CalculateViewPaddingOffset();
        const Vector2 lv2PaddingWorld =
            MapTransform::Transform(lv2Padding, MapTransform::GetNormalisedRect(), mv4WorldRect);

        // ---- the X axis ----
        const f32 lfWidth = mv4WorldRect.z - mv4WorldRect.x;
        mv4WorldRect.x = lv2Centre.x - lfWidth * KF_RECT_CENTRE_SCALE;

        if (mbStickMapLeft)
        {
            const f32 lfMinX = lrv4FixedWorldRect.x + lv2PaddingWorld.x;
            mv4WorldRect.x = (mv4WorldRect.x - lfMinX >= 0.0f) ? mv4WorldRect.x : lfMinX;
        }
        if (mbStickMapRight)
        {
            const f32 lfMaxX = lrv4FixedWorldRect.z - lfWidth;
            mv4WorldRect.x = (mv4WorldRect.x - lfMaxX >= 0.0f) ? lfMaxX : mv4WorldRect.x;
        }
        mv4WorldRect.z = mv4WorldRect.x + lfWidth;

        // ---- the Y axis (the height is measured after the .z rebuild, as the asm does) ----
        const f32 lfHeight = mv4WorldRect.w - mv4WorldRect.y;
        mv4WorldRect.y = lv2Centre.y - lfHeight * KF_RECT_CENTRE_SCALE;

        if (mbStickMapUp)
        {
            const f32 lfMinY = lrv4FixedWorldRect.y + lv2PaddingWorld.y;
            mv4WorldRect.y = (mv4WorldRect.y - lfMinY >= 0.0f) ? mv4WorldRect.y : lfMinY;
        }
        if (mbStickMapDown)
        {
            const f32 lfMaxY = lrv4FixedWorldRect.w - lfHeight;
            mv4WorldRect.y = (mv4WorldRect.y - lfMaxY >= 0.0f) ? lfMaxY : mv4WorldRect.y;
        }
        mv4WorldRect.w = mv4WorldRect.y + lfHeight;

        // cpp:376 -- the same guard Construct / SetZoom fire, on the rect just installed.
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        Vector2 lv2NewCentre;
        lv2NewCentre.x = mv4WorldRect.x + (mv4WorldRect.z - mv4WorldRect.x) * KF_RECT_CENTRE_SCALE;
        lv2NewCentre.y = mv4WorldRect.y + (mv4WorldRect.w - mv4WorldRect.y) * KF_RECT_CENTRE_SCALE;
        lv2NewCentre.z = 0.0f;   // the console's explicit `std` of the z/w pair
        lv2NewCentre.w = 0.0f;
        return lv2NewCentre;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::CalculateOffsetWorldCentre
    //
    // NO OUT-OF-LINE X360 ADDRESS: the console inlines this private helper at every call
    // site (BrnMainMap.h:207 declares it; scratch/func_index.tsv has no entry). Its shape is
    // read straight off SnapToLocation @0x8245EBA0, which expands it TWICE, once per
    // OffsetPadding value, as an identical four-step chain that differs ONLY in the sign of
    // the padding add:
    //     bl CalculateViewPaddingOffset                       -> pad (normalised space)
    //     bl sub_8245A080  (v2 = mv4WorldRect, v3 = normalised) -> centre in normalised space
    //     vaddfp128 v1, v1, v127   /   vsubfp128 v1, v1, v127   -> +pad  /  -pad
    //     bl sub_8245A080  (v2 = normalised, v3 = mv4WorldRect) -> back to world space
    // (@0x8245EDF4..0x8245EE18 for the ADD arm, @0x8245EE38..0x8245EE5C for the SUB arm.)
    // The E_OFFSETPADDING_IN / _OUT enumerator ORDER is the DWARF's (BrnMainMap.h:202) and
    // the add arm is the one the console emits FIRST, i.e. IN == pad the centre inward.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::CalculateOffsetWorldCentre(Vector2 lv2Centre, OffsetPadding lePadding)
    {
        const Vector2 lv2Padding = CalculateViewPaddingOffset();

        Vector2 lv2Normalised =
            MapTransform::Transform(lv2Centre, mv4WorldRect, MapTransform::GetNormalisedRect());

        if (lePadding == E_OFFSETPADDING_IN)
        {
            lv2Normalised.x += lv2Padding.x;
            lv2Normalised.y += lv2Padding.y;
            lv2Normalised.z += lv2Padding.z;   // whole-quadword vaddfp128
            lv2Normalised.w += lv2Padding.w;
        }
        else
        {
            lv2Normalised.x -= lv2Padding.x;
            lv2Normalised.y -= lv2Padding.y;
            lv2Normalised.z -= lv2Padding.z;
            lv2Normalised.w -= lv2Padding.w;
        }

        return MapTransform::Transform(lv2Normalised,
                                       MapTransform::GetNormalisedRect(), mv4WorldRect);
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::SnapToLocation
    //
    // X360 ARTIST @0x8245EBA0 (json name field verified). Jumps the map straight to a world
    // location: the pending zoom is committed immediately (no animation), the world rect is
    // rebuilt around it, and the zoom-in-flight flag is cleared. Called by
    // CrashNavMapMain::Update (BrnCrashNavMapMain.cpp:419) and the online room's map
    // (BrnOnlineGameRoomPlayerInfo_wH_00.cpp:482).
    //
    // ⭐ THE RECT BUILD IS THE SAME ONE SetZoom DOES, including the lane-insert aspect
    // scale: `lvx128 v7, r25, 0xA0` off the engine lane-insert table at 0x8327F140 == lane 2
    // / source word 2 == "dest.z from src2.z", so only the z lane of (rect * 16/9) survives.
    // flt_82F25AD4 == 0x3FE38E39 == 1.7777778f, read from the image.
    //
    // ⭐ THE TWO CalculateOffsetWorldCentre RESULTS ARE DEAD ON THE CONSOLE, AND THAT IS
    // TRANSCRIBED, NOT TIDIED AWAY. At 0x8245EE1C and 0x8245EE5C the returned Vector2 is
    // dropped: the `vmr128 v1, v124` immediately after the first chain reloads the ORIGINAL
    // lv2Location for the CalculatePositionedWorldRect call, and the second chain is the last
    // thing before `stb r31, 0x670`. The calls survive optimisation only because
    // sub_8245A080 is out-of-line, so the console really does run them; the observable
    // effects of the tail are CalculatePositionedWorldRect's writes to mv4WorldRect and the
    // flag clear. [FLAG dead-value] Written as assignments to a discarded local so the shape
    // stays visible -- if a later wave finds the source assigned these to a member, this is
    // where to look. CalculatePositionedWorldRect's own return is discarded the same way
    // (its sret buffer is overwritten by the next CalculateViewPaddingOffset call).
    // -------------------------------------------------------------------------
    void MainMapComponent::SnapToLocation(Vector2 lv2Location)
    {
        // cpp:397 -- the console's cache tripwire (it derefs nothing here).
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        // `lfs f0, 0x664(r30)` then `stfs f0, 0x660(r30)`: the pending zoom is committed
        // outright rather than animated towards.
        mfWorldZoomScaleFactor = mfDesiredWorldZoomFactor;

        // `stvx128 v124, r0, this+0x650` -- a full quadword, all four lanes of the caller's
        // Vector2 (the same whole-register store SetDesiredWorldCentre performs).
        mv2DesiredCentre = lv2Location;

        // mv4WorldRect = mv4ViewRect * <the just-committed scale>, then the z lane alone
        // scaled by the 16:9 aspect (the lane-insert vperm; see the banner).
        mv4WorldRect.x = mv4ViewRect.x * mfWorldZoomScaleFactor;
        mv4WorldRect.y = mv4ViewRect.y * mfWorldZoomScaleFactor;
        mv4WorldRect.z = mv4ViewRect.z * mfWorldZoomScaleFactor * KF_WORLD_RECT_ASPECT;
        mv4WorldRect.w = mv4ViewRect.w * mfWorldZoomScaleFactor;

        // cpp:405 -- a TWO-lane magnitude (the asm splats lanes 0 and 1 of the squared
        // register and adds them; z/w are never read), hence the Vector2 overload.
        CGS_ASSERT(rw::math::vpu::Magnitude(mv2DesiredCentre) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv2DesiredCentre) < 1000000.0f");
        // cpp:406 -- the four-lane one, on the rect just installed.
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        // See the [FLAG dead-value] note above: the console computes all three of these and
        // uses none of the returned values.
        Vector2 lv2Discarded = CalculateOffsetWorldCentre(lv2Location, E_OFFSETPADDING_IN);
        lv2Discarded = CalculatePositionedWorldRect(lv2Location);
        lv2Discarded = CalculateOffsetWorldCentre(lv2Location, E_OFFSETPADDING_OUT);
        (void)lv2Discarded;

        // `stb r31, 0x670(r30)` with r31 == 0 -- a snap ends any zoom animation.
        mbIsZooming = false;
    }


    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::ApplyZoom
    //
    // X360 ARTIST @0x8245EE78 (json name field verified). The per-frame ANIMATION step of
    // the map view: it walks mfWorldZoomScaleFactor towards mfDesiredWorldZoomFactor and the
    // live world centre towards mv2DesiredCentre, rebuilds mv4WorldRect for whatever the
    // scale now is, and reports whether a zoom is still in flight (mbIsZooming). Returns the
    // world centre the caller should adopt this frame. Its ONE caller is Update below.
    //
    // ⭐ THE PPC FLOAT-ARG GPR SKIP AGAIN. The console signature is
    // `(sret=r3, this=r4, lv2WorldCentre=v1, lfZoom=f1)`; Hex-Rays shows a single `int a1`
    // and loses r4 entirely, which is why the pseudocode's `_R30 = v1` reads like nonsense.
    // Decoded off the raw prologue: `mr r30, r4` (this), `mr r20, r3` (the sret buffer),
    // `vmr128 v124, v1` (the centre), `fmr f31, f1` (the zoom). The DWARF C++ shape is the
    // two-parameter one declared in BrnMainMap.h; do not add a third.
    //
    // ⭐ EVERY MAGIC NUMBER HERE WAS READ FROM THE IMAGE, NONE GUESSED
    // (scratch/postfx_step9_final/envfix/work/image.bin, file offset = VA - 0x82000000, BE):
    //     flt_82F25C6C = 42C80000 = 100.0f  -- the per-frame zoom step AND the "the zoom has
    //                                         settled" threshold (the same constant serves both)
    //     flt_82F25C68 = 3F800000 =   1.0f  -- the world-units "close enough" epsilon
    //     flt_82004EF4 = 40800000 =   4.0f  -- the centre-chase divisor while zooming
    //     flt_82001C98 = 3F800000 =   1.0f  -- the centre-chase divisor once zoom has settled
    //     flt_82001CC0 = 00000000 =   0.0f, flt_820037C8 = BF800000 = -1.0f (the sign pick)
    //     flt_820068C0 = 49742400 = 1000000.0f (the shared Magnitude guard)
    //
    // ⭐ THE TWO CHASE DIVISORS ARE THE SAME INLINED HELPER, NOT TWO DIFFERENT MATHS. Both
    // arms emit `centre + (desired - centre) * (1/K)` with the reciprocal computed by
    // vrefp + two Newton-Raphson refinements -- K == 4.0f while the zoom is still stepping
    // and K == 1.0f once it has settled (i.e. the settled arm jumps the centre the whole way
    // in one frame). The console really does run the reciprocal chain on the literal 1.0f;
    // that is the shared helper being inlined twice, and it is transcribed as
    // `* (1.0f / K)` rather than folded away so the shape stays legible.
    //
    // ⭐ vmaddfp / vnmsubfp128 OPERAND ORDER PINNED OFF THE rsqrt REFINEMENT (the recurring
    // campaign bug). At 0x8245F018/0x8245F01C the pair must produce the textbook
    // `y + 0.5*y*(1 - x*y*y)`. Only ONE reading of each does:
    //     vnmsubfp128 vD, vA, vB, vC  ->  vC - vA*vB     (addend printed THIRD)
    //     vmaddfp     vD, vA, vB, vC  ->  vA*vC + vB     (addend printed SECOND)
    // They genuinely differ because the VMX128 form re-orders its encoding fields and the
    // classic Altivec form does not -- exactly the trap the campaign has been burned by.
    // The same reading makes `vmaddcfp128 v123, v11, v13, v124` == v11*v13 + v124, i.e. the
    // chase above, and makes the vrefp refinements `r + r*(1 - c*r)`.
    //
    // ⭐ THE RETURNED CENTRE IS THE *PRE*-REPOSITION ONE, AND THAT IS TRANSCRIBED, NOT
    // TIDIED. In the settled arm the console computes a fully re-padded, re-positioned
    // centre (the pad-in / CalculatePositionedWorldRect / pad-out chain at
    // 0x8245F1B4..0x8245F228) and then uses it for NOTHING except the convergence test at
    // 0x8245F22C -- v123, the return value, was last written at 0x8245F144 and is never
    // updated from it. [FLAG dead-value] If a later wave finds the source returning the
    // repositioned centre, this is the line to revisit.
    //
    // ⭐ THE FOUR-WAY switch REALLY DOES HAVE ONE ARM. The jump table at jpt_8245F340 sends
    // cases 0-3 to the same `SetZoomLevel(E_ZOOM_MEDIUM)` block (IDA even labels it
    // "cases 0-3"); only the out-of-range default reaches the "Unexpected zoom factor"
    // assert at cpp:635. Reproduced as written -- collapsing it to an unconditional call
    // would drop the console's own range tripwire.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::ApplyZoom(Vector2 lv2WorldCentre, f32 lfZoom)
    {
        // v123 -- the value that will be returned. Seeded with the caller's centre
        // (`vmr128 v123, v124` @0x8245EEB0) so every path that does not chase leaves it alone.
        Vector2 lv2Result = lv2WorldCentre;

        // `mr r9, r23` with r23 == 1: the commit block runs unless a later path clears this.
        bool lbCommit = true;

        const f32 lfZoomDelta = mfDesiredWorldZoomFactor - mfWorldZoomScaleFactor;

        if (FAbs(lfZoomDelta) > KF_ZOOM_STEP_PER_FRAME)
        {
            // ---- the zoom is still far from its target: step it and chase the centre ----
            // `fcmpu f13, 0.0 ; ble -> -1.0` -- the sign is picked with a plain compare, so a
            // delta of exactly 0.0f takes the NEGATIVE arm (unreachable here: |delta| > 100).
            const f32 lfStepSign = (lfZoomDelta > 0.0f) ? 1.0f : -1.0f;
            mfWorldZoomScaleFactor += KF_ZOOM_STEP_PER_FRAME * lfStepSign;

            // `stb r23, 0x670(r30)` -- stepping the zoom IS a zoom in flight.
            mbIsZooming = true;

            const f32 lfChase = 1.0f / KF_CENTRE_CHASE_DIVISOR_ZOOMING;
            lv2Result.x = lv2WorldCentre.x + (mv2DesiredCentre.x - lv2WorldCentre.x) * lfChase;
            lv2Result.y = lv2WorldCentre.y + (mv2DesiredCentre.y - lv2WorldCentre.y) * lfChase;
            lv2Result.z = lv2WorldCentre.z + (mv2DesiredCentre.z - lv2WorldCentre.z) * lfChase;
            lv2Result.w = lv2WorldCentre.w + (mv2DesiredCentre.w - lv2WorldCentre.w) * lfChase;

            // `b loc_8245F2C4` with r9 == 0: the commit block is SKIPPED while stepping.
            lbCommit = false;
        }
        else
        {
            // ---- the zoom has settled; only the centre may still need to move ----
            Vector2 lv2ToDesired;
            lv2ToDesired.x = mv2DesiredCentre.x - lv2WorldCentre.x;
            lv2ToDesired.y = mv2DesiredCentre.y - lv2WorldCentre.y;
            lv2ToDesired.z = mv2DesiredCentre.z - lv2WorldCentre.z;
            lv2ToDesired.w = mv2DesiredCentre.w - lv2WorldCentre.w;

            // The two-lane magnitude (the asm splats lanes 0 and 1 of the squared register
            // and adds them; z/w are never read), with the console's vsel zero-dot guard.
            if (rw::math::vpu::Magnitude(lv2ToDesired) > KF_CENTRE_SETTLE_EPSILON)
            {
                // mv4WorldRect = mv4ViewRect * <the current scale>, then the z lane alone
                // scaled by lfZoom -- the SAME lane-insert vperm SetZoom / SnapToLocation do
                // (`lvx128 v7, r25, 0xA0` off the engine table at 0x8327F140 == lane 2 /
                // source word 2 == "dest.z from src2.z").
                mv4WorldRect.x = mv4ViewRect.x * mfWorldZoomScaleFactor;
                mv4WorldRect.y = mv4ViewRect.y * mfWorldZoomScaleFactor;
                mv4WorldRect.z = mv4ViewRect.z * mfWorldZoomScaleFactor * lfZoom;
                mv4WorldRect.w = mv4ViewRect.w * mfWorldZoomScaleFactor;

                // cpp:579 -- the shared four-lane guard on the rect just installed.
                CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                           "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

                // The settled arm's chase divisor is 1.0f, i.e. the centre lands on the
                // desired centre this frame (see the banner -- the reciprocal chain on a
                // literal 1.0f is the shared helper being inlined).
                const f32 lfChase = 1.0f / KF_CENTRE_CHASE_DIVISOR_SETTLED;
                lv2Result.x = lv2WorldCentre.x + lv2ToDesired.x * lfChase;
                lv2Result.y = lv2WorldCentre.y + lv2ToDesired.y * lfChase;
                lv2Result.z = lv2WorldCentre.z + lv2ToDesired.z * lfChase;
                lv2Result.w = lv2WorldCentre.w + lv2ToDesired.w * lfChase;

                // The pad-in / reposition / pad-out chain. Its result feeds ONLY the
                // convergence test below -- see the [FLAG dead-value] note in the banner.
                const Vector2 lv2PadIn = CalculateViewPaddingOffset();
                Vector2 lv2Normalised =
                    MapTransform::Transform(lv2Result, mv4WorldRect,
                                            MapTransform::GetNormalisedRect());
                lv2Normalised.x += lv2PadIn.x;
                lv2Normalised.y += lv2PadIn.y;
                lv2Normalised.z += lv2PadIn.z;   // whole-quadword vaddfp128
                lv2Normalised.w += lv2PadIn.w;
                const Vector2 lv2Padded =
                    MapTransform::Transform(lv2Normalised, MapTransform::GetNormalisedRect(),
                                            mv4WorldRect);

                const Vector2 lv2Positioned = CalculatePositionedWorldRect(lv2Padded);

                const Vector2 lv2PadOut = CalculateViewPaddingOffset();
                Vector2 lv2Unpadded =
                    MapTransform::Transform(lv2Positioned, mv4WorldRect,
                                            MapTransform::GetNormalisedRect());
                lv2Unpadded.x -= lv2PadOut.x;
                lv2Unpadded.y -= lv2PadOut.y;
                lv2Unpadded.z -= lv2PadOut.z;
                lv2Unpadded.w -= lv2PadOut.w;
                const Vector2 lv2Settled =
                    MapTransform::Transform(lv2Unpadded, MapTransform::GetNormalisedRect(),
                                            mv4WorldRect);

                // `vsubfp128 v0, v1, v124` then the two-lane magnitude: how far the settled
                // centre still is from the one the caller came in with.
                Vector2 lv2Remaining;
                lv2Remaining.x = lv2Settled.x - lv2WorldCentre.x;
                lv2Remaining.y = lv2Settled.y - lv2WorldCentre.y;
                lv2Remaining.z = lv2Settled.z - lv2WorldCentre.z;
                lv2Remaining.w = lv2Settled.w - lv2WorldCentre.w;

                // `stb r11, 0x670(r30)` takes the compare result directly, and the commit
                // flag is its complement (`cntlzw` + `extrwi ...,1,26` @0x8245F2B8).
                mbIsZooming =
                    (rw::math::vpu::Magnitude(lv2Remaining) > KF_CENTRE_SETTLE_EPSILON);
                lbCommit = !mbIsZooming;
            }
            // else: the centre is already within the epsilon. lv2Result stays the caller's
            // centre, mbIsZooming is NOT touched, and lbCommit stays true.
        }

        if (lbCommit)
        {
            // `vrlimi128 v13, v123, 3, 2` on both operands before `vcmpeqfp.`: lanes 2 and 3
            // are overwritten with a 2-word rotation of the same register, so both sides
            // become {x, y, x, y} and the all-lanes compare is a pure 2D equality.
            if (mv2DesiredCentre.x != lv2Result.x || mv2DesiredCentre.y != lv2Result.y)
            {
                // The whole-quadword `stvx128 v123, r0, r26` -- i.e. SetDesiredWorldCentre.
                SetDesiredWorldCentre(lv2Result);
            }

            if (mfWorldZoomScaleFactor == mfDesiredWorldZoomFactor)
            {
                mbIsZooming = false;
            }
            else
            {
                mfWorldZoomScaleFactor = mfDesiredWorldZoomFactor;
                mbIsZooming = true;
            }

            switch (meCurrentZoomFactor)
            {
            case E_ZOOMFACTOR_LOW:
            case E_ZOOMFACTOR_MEDIUM:
            case E_ZOOMFACTOR_HIGH:
            case E_ZOOMFACTOR_CUSTOM:
                // Every in-range factor selects the SAME tile zoom level (jpt_8245F340).
                mMapManager.SetZoomLevel(MapManager::E_ZOOM_MEDIUM);
                break;
            default:
                // cpp:635 -- the console's out-of-range tripwire.
                CGS_ASSERT(false, "Unexpected zoom factor");
                break;
            }
        }

        // The unconditional tail: rebuild mv4WorldRect for whatever mfWorldZoomScaleFactor
        // now is (the commit block above may have just replaced it), with the same lane-insert
        // aspect scale on .z. `lfs f0, 0x660(r30)` is a FRESH read -- do not hoist it.
        mv4WorldRect.x = mv4ViewRect.x * mfWorldZoomScaleFactor;
        mv4WorldRect.y = mv4ViewRect.y * mfWorldZoomScaleFactor;
        mv4WorldRect.z = mv4ViewRect.z * mfWorldZoomScaleFactor * lfZoom;
        mv4WorldRect.w = mv4ViewRect.w * mfWorldZoomScaleFactor;

        // cpp:645
        CGS_ASSERT(rw::math::vpu::Magnitude(mv4WorldRect) < KF_MAX_WORLD_RECT_MAGNITUDE,
                   "RwMathVPU::Magnitude(mv4WorldRect) < 1000000.0f");

        return lv2Result;
    }

    // -------------------------------------------------------------------------
    // BrnGui::MainMapComponent::Update
    //
    // X360 ARTIST @0x824696E8 (json name field verified). ⭐ THE PER-FRAME MAP-WORLD PUMP --
    // the last gate between the CrashNav map chrome and the map world actually rendering.
    // In order: animate the view (ApplyZoom), re-pad and re-position the world rect, hand the
    // tile manager the padded world region it must cover, rebuild + refresh the tile working
    // set, emit GuiEventRenderMainMap for the custom map renderer, publish the zoomed world /
    // viewport spaces for the icon layer, and return the caller's next world centre.
    //
    // Its ONE committed caller feedback-assigns the return
    // (`mv2WorldCenterPoint = mMainMapComponent.Update(mv2WorldCenterPoint)` --
    // BrnCrashNavMap_wJ_07.cpp:382 and BrnPreRaceFlyBy_wJ_04.cpp:347), which is why the
    // returned centre is the animation's carrier and not a convenience.
    //
    // ⭐ WHAT THE OLD BLOCKED SKELETON GOT WRONG, corrected here against the raw asm:
    //   (a) `mMapManager.mWorldRect` is NOT mv4WorldRect. It is mv4PaddingRect's two corners
    //       taken from NORMALISED space into the current world rect -- i.e. the padded,
    //       on-screen slice of the world, which is exactly the region that needs tiles.
    //       (0x824697C4 loads this+0x630 == mv4PaddingRect, not this+0x610.)
    //   (b) The tail's input is CalculatePositionedWorldRect's RETURN (stack var_D0 reloaded
    //       at 0x824699FC), not the pre-reposition centre.
    //
    // ⭐ THE TWO PERM BLOCKS ARE DECODED OFF THE IMAGE, NOT GUESSED. Both use the mask at
    // unk_82CDA350 = {0,1,2,3, 20,21,22,23, 0,1,2,3, 0,1,2,3} (read from image.bin: bytes
    // 00 01 02 03 14 15 16 17 00 01 02 03 00 01 02 03), which pairs word0 of operand A with
    // word1 of operand B -- so `vperm vD, splat(a), splat(b), mask` is simply "make the
    // Vector2 (a, b)". The `lvsl v13, 0, N ; vspltw v, v13, 0` idiom in front of each is the
    // lane-SELECT permute for lane N/4 (N == 0/4/8/12 -> .x/.y/.z/.w). So:
    //   * block 1 (0x824697C0..0x8246980C) splits mv4PaddingRect into its (x,y) and (z,w)
    //     corners;
    //   * block 2 (0x8246996C..0x824699D4) splits mv4WorldRect into the three corners
    //     SetZoomedWorldRect wants: origin (x,y), the x-axis point (z,y), the y-axis point
    //     (x,w) -- the MakeCoordSpaceFromPoints triple.
    // The re-interleave that assembles the tile rect uses the second mask
    // unk_82CDA3C0 = {0,1,2,3, 0,1,2,3, 0,1,2,3, 20,21,22,23} plus `vsldoi ...,8`
    // (IDA prints the immediate 8 as a phantom `v8`), which yields {A.x, A.y, B.x, B.y}.
    //
    // ⭐ THE EVENT IS STACK-BUILT FIELD BY FIELD, WITH NO Construct() CALL. Measured stack
    // slots: var_A0 = mv4MapRect (this+0x610), var_90 = mv4ViewRect (this+0x620),
    // var_80 = &mMapManager.mActiveTextures (this+0x340 == 0x8C + 0x2B4),
    // var_7C = mfWorldZoomScaleFactor (this+0x660), var_78 = meMapType (this+0x66C),
    // var_74 = mbIsActive (this+0x67C) -- exactly GuiEventRenderMainMap's declared order, and
    // OutputViewState is handed var_A0. mMapManager.mbEnabled (MapManager +0x564) is read
    // TWICE, once for the tile rebuild and once for the event, and both reads are kept.
    // -------------------------------------------------------------------------
    Vector2 MainMapComponent::Update(Vector2 lv2WorldCentre)
    {
        // cpp:167 -- the console's cache tripwire (it derefs nothing here).
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        // flt_82F25AD4 == 0x3FE38E39 == 1.7777778f, read from the image: the 16:9 aspect the
        // whole family folds into the world rect's z lane (SetZoom and SnapToLocation apply
        // the identical constant inline).
        const Vector2 lv2Zoomed = ApplyZoom(lv2WorldCentre, KF_WORLD_RECT_ASPECT);

        // ---- pad the animated centre, then re-position the world rect around it ----
        const Vector2 lv2PadIn = CalculateViewPaddingOffset();
        Vector2 lv2Normalised =
            MapTransform::Transform(lv2Zoomed, mv4WorldRect, MapTransform::GetNormalisedRect());
        lv2Normalised.x += lv2PadIn.x;
        lv2Normalised.y += lv2PadIn.y;
        lv2Normalised.z += lv2PadIn.z;   // whole-quadword vaddfp128
        lv2Normalised.w += lv2PadIn.w;
        const Vector2 lv2Padded =
            MapTransform::Transform(lv2Normalised, MapTransform::GetNormalisedRect(), mv4WorldRect);

        // Rebuilds mv4WorldRect (only) around the padded centre and returns the centre it
        // actually installed -- that return is what the tail below carries back to the caller.
        const Vector2 lv2Positioned = CalculatePositionedWorldRect(lv2Padded);

        // ---- hand the tile manager the padded on-screen region, in world units ----
        // The padding rect lives in NORMALISED space, so each corner is transformed
        // normalised -> world against the rect CalculatePositionedWorldRect just installed.
        const Vector2 lv2PaddingMin = { mv4PaddingRect.x, mv4PaddingRect.y,
                                        mv4PaddingRect.x, mv4PaddingRect.x };
        const Vector2 lv2PaddingMax = { mv4PaddingRect.z, mv4PaddingRect.w,
                                        mv4PaddingRect.z, mv4PaddingRect.z };
        const Vector2 lv2TileMin = MapTransform::Transform(
            lv2PaddingMin, MapTransform::GetNormalisedRect(), mv4WorldRect);
        const Vector2 lv2TileMax = MapTransform::Transform(
            lv2PaddingMax, MapTransform::GetNormalisedRect(), mv4WorldRect);

        // The {A.x, A.y, B.x, B.y} re-interleave, stored lane by lane through the console's
        // four scalar `stfs` at 0x82469874..0x82469880.
        mMapManager.mWorldRect.mfLeft   = lv2TileMin.x;
        mMapManager.mWorldRect.mfTop    = lv2TileMin.y;
        mMapManager.mWorldRect.mfRight  = lv2TileMax.x;
        mMapManager.mWorldRect.mfBottom = lv2TileMax.y;

        if (mMapManager.mbEnabled)
        {
            mMapManager.CalculateCurrentTileSet();
            mMapManager.RefreshActiveTextureArray();
        }

        // [DIAG-TEMP] NOT IN THE X360 BINARY -- [map-pump] the per-frame map chain state.
        {
            static s32 siLeft = 12;
            static s32 siFrame = 0;
            ++siFrame;
            if (siLeft > 0 && (siFrame % 60) == 1 && CgsDev::Log::gpDebugPrint != 0)
            {
                --siLeft;
                *CgsDev::Log::gpDebugPrint
                    << "[map-pump] f=" << siFrame
                    << " enabled=" << static_cast<s32>(mMapManager.mbEnabled)
                    << " active=" << static_cast<s32>(mbIsActive)
                    << " maptype=" << static_cast<s32>(meMapType)
                    << " lowres=" << (mMapManager.mLowResTexture.mpTextureState != 0 ? 1 : 0)
                    << " texcount=" << static_cast<s32>(mMapManager.mActiveTextures.muTextureCount)
                    << " dirs=" << (mMapManager.mapDirectories[0] != 0 ? 1 : 0)
                    << "/" << (mMapManager.mapDirectories[1] != 0 ? 1 : 0)
                    << " zoom=" << mfWorldZoomScaleFactor
                    << " world=(" << mv4WorldRect.x << "," << mv4WorldRect.y
                    << "," << mv4WorldRect.z << "," << mv4WorldRect.w << ")"
                    << " view=(" << mv4ViewRect.x << "," << mv4ViewRect.y
                    << "," << mv4ViewRect.z << "," << mv4ViewRect.w << ")"
                    << "\n";
            }
        }

        // The console re-reads the same byte rather than reusing the first test's register.
        if (mMapManager.mbEnabled)
        {
            GuiEventRenderMainMap lRenderEvent;
            lRenderEvent.mv4MapRect       = mv4WorldRect;
            lRenderEvent.mv4ViewRect      = mv4ViewRect;
            lRenderEvent.mpActiveTextures = &mMapManager.mActiveTextures;
            lRenderEvent.mfZoomLevel      = mfWorldZoomScaleFactor;
            lRenderEvent.meMapType        = meMapType;
            lRenderEvent.mbIsActive       = mbIsActive;

            // cpp:203 -- a streamed assert on the console (BeginAssert + StrStreamBase +
            // FireAssert); the message text is the console's own.
            CGS_ASSERT(mpStateInterface != 0, "State interface is invalid");

            mpStateInterface->OutputViewState(lRenderEvent);
        }

        // ---- publish the zoomed spaces the icon / sat-nav layer reads ----
        // The three corners of mv4WorldRect, in MakeCoordSpaceFromPoints order.
        const Vector2 lv2ZoomOrigin = { mv4WorldRect.x, mv4WorldRect.y,
                                        mv4WorldRect.x, mv4WorldRect.x };
        const Vector2 lv2ZoomXPoint = { mv4WorldRect.z, mv4WorldRect.y,
                                        mv4WorldRect.z, mv4WorldRect.z };
        const Vector2 lv2ZoomYPoint = { mv4WorldRect.x, mv4WorldRect.w,
                                        mv4WorldRect.x, mv4WorldRect.x };
        MapTransform::SetZoomedWorldRect(lv2ZoomOrigin, lv2ZoomXPoint, lv2ZoomYPoint);
        MapTransform::SetZoomedViewportRect(mv4ViewRect);

        // ---- un-pad the positioned centre and hand it back to the caller ----
        const Vector2 lv2PadOut = CalculateViewPaddingOffset();
        Vector2 lv2Unpadded =
            MapTransform::Transform(lv2Positioned, mv4WorldRect, MapTransform::GetNormalisedRect());
        lv2Unpadded.x -= lv2PadOut.x;
        lv2Unpadded.y -= lv2PadOut.y;
        lv2Unpadded.z -= lv2PadOut.z;
        lv2Unpadded.w -= lv2PadOut.w;

        return MapTransform::Transform(lv2Unpadded, MapTransform::GetNormalisedRect(), mv4WorldRect);
    }
}
