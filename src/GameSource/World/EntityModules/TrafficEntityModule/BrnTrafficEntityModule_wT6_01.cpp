// ============================================================================
// BrnTrafficEntityModule_wT6_01.cpp -- the traffic-jam relief valve.
//
//   TrafficEntityModule::NukeTrafficJams  @0x827353E8  (DWARF :1551, .cpp 8056)  1104 insns
//
// The console's answer to a self-inflicted jam. The reactive AI (swerve / give-up /
// stop-for-obstruction) can wedge a run of cars nose-to-tail at walking pace, and once that
// happens nothing else in the module can reach them: RemoveVehicle is driven by the junction
// FUP score, which for a stalled junction pins below its 65 threshold and stays there.
// NukeTrafficJams NEVER CONSULTS THAT SCORE. That is the whole point of it -- it is a second,
// independent path to Param::SetShouldBeRemoved that a stuck junction cannot starve.
//
// The consumer half of the valve is ALREADY LIVE: UpdateParams (_wT2_02.cpp:223) tests
// E_FLAG_SHOULD_BE_REMOVED and calls the bodied KillParam. So this one function closes it.
//
// ---- WHAT THE ASM ACTUALLY DOES (read 2026-08-29; Hex-Rays FAILED on this body) ------------
// The IDA pseudocode carries "local variable allocation has failed, the output may be wrong!"
// and it is wrong in ways that matter -- it aliases the stack iterator with the Vector3 out-
// slot, renders the run-walk links through a u16* it never scales consistently, and drops the
// early-outs. Every statement below is off the DISASSEMBLY, with the address range in-line.
//
// Outer walk: FastBitArray<600>::Iterator over mParamSoaData.mAliveParams (this+0x3D700).
// For each alive param not already swallowed by an earlier run:
//   1. walk the doubly-linked param list BOTH ways from it, collecting the maximal run of
//      consecutive params that are (a) miBehaviour == 5 and (b) mfSpeed < 5.0 m/s;
//   2. if the run is LONGER THAN FOUR params, it is a jam -- drain it;
//   3. draining marks EVERY THIRD collected param SetShouldBeRemoved, skipping any the player
//      can see (rendered last frame, or within 40 m of the camera).
//
// ⭐ THREE THINGS THE PROSE DESCRIPTION OF THIS FUNCTION HAS BEEN GETTING WRONG, all measured:
//   * THE DRAIN STRIDE IS THREE, NOT ONE (`addi r25, r25, 3` @0x827361FC). The console thins a
//     jam to a third of its cars; it does not delete it. A stride-1 reconstruction would empty
//     a whole queue in one frame and read as a spawn bug.
//   * A JAM IS FIVE CARS, NOT TWO (`cmplwi r11, 4 / ble` @0x82735F0C). Four stopped cars in a
//     row are a red light, not a jam, and the console leaves them alone.
//   * THE CAMERA TEST IS GATED ON mbAllowDivergentBehaviour (+0x717E7, @0x82735F5C), NOT on
//     mbIsOnlineGameMode. Those are different conditions: the flag is
//     `!mbIsOnlineGameMode || mbPlayingShowtimeMode` (_wT1_01.cpp:207), so ONLINE SHOWTIME
//     still runs the camera test. "offline tests, online skips" is only true off-showtime.
//
// ---- what is NOT here, deliberately ---------------------------------------------------------
// The console composes six of its asserts through CgsDev::StrStream ("Index " << i << " is out
// of range (max bits: " << 600 << "\n"). Every one of those belongs to a FastBitArray or Array
// accessor, not to this function -- per this tree's convention (see RemoveVehicle's banner in
// _wT5_01.cpp) an accessor's own assert is NOT restated at the call site. The ONE assert baked
// against BrnTrafficEntityModule.cpp -- "luCurrParam < KU_MAX_PARAMS" at .cpp 8056 -- is this
// function's own and is kept.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"        // Array<u16,64>
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h" // FastBitArray<600> + Iterator
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // [DIAG] only

#include "rw/math/vpu/vector3_operation.h"                     // operator-, Magnitude

#include <cstdlib>                                             // [DIAG] getenv


namespace BrnTraffic
{
namespace
{
    // ---- the console's own .rdata literals -------------------------------------------------
    // ⛔ INIT-ORDER CHECKED, BOTH EDGES (2026-08-29). Neither of these is a dyn-init splat:
    // a scan of the ASSEMBLY of all 30,084 exported ARTIST functions finds ZERO store
    // instructions referencing either symbol, so no CRT thunk writes them and no thunk can
    // observe a half-built dependency. They are plain static .rdata, and the image byte IS
    // the shipped value -- which also means "recovering a truer value" would be the WRONG
    // move here. Values cross-confirmed against independent consumers that fold the same
    // symbol (flt_8200426C: 101 functions, e.g. BehaviourRig::Parameters::Construct;
    // flt_820BA590: DoesParamNeedToStopForStopline and UpdateSympatheticCrashing).

    // flt_8200426C -- `lfs f13` @0x82735A58 / @0x82735CE0, compared against Param::mfSpeed.
    const f32 KF_JAM_MAX_SPEED = 5.0f;

    // flt_820BA590 -- held in f31 across the whole body (@0x82735580), splatted into the
    // vector compare at 0x82736154. Metres from mCameraLastFrame.
    const f32 KF_JAM_CAMERA_PROTECTION_RADIUS = 40.0f;

    // `cmplwi r11, 4 ; ble` @0x82735F0C -- a run of FOUR OR FEWER is not a jam.
    const s32 KI_JAM_MIN_RUN_LENGTH = 4;

    // `addi r25, r25, 3` @0x827361FC -- the drain visits every third collected param.
    const u32 KU_JAM_KILL_STRIDE = 3;

    // The stack Array<u16,64> the run is collected into; the walk stops when it is full
    // (`cmplwi r11, 0x40 ; beq` @0x82735A8C / @0x82735D14). N is the array's own capacity.
    const u32 KU_JAM_RUN_CAPACITY = 64;

    // Param::miBehaviour value 5, the state every car in a jam is in. ⛔ DO NOT NAME IT.
    // BrnTrafficParam.h attests enumerators 0..3 (from the console's own baked assert strings)
    // and 6; 4 and 5 carry no assert string anywhere in the image, and that header's standing
    // instruction is "do NOT invent them". This function is new evidence about what 5 MEANS --
    // it is the state a queueing/obstructed car sits in, since a run of them under 5 m/s is
    // exactly what the console calls a traffic jam -- but evidence about meaning is not a name.
    const s8 KI_JAM_BEHAVIOUR = 5;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::NukeTrafficJams  @ 0x827353E8
//
// Caller: UpdateNonDecisionFrame @0x8274C1A8, behind
// `mbNeedToRunTrafficJamNuker && !mbNeedToKillAllZombies`.
// ----------------------------------------------------------------------------
void TrafficEntityModule::NukeTrafficJams()
{
    // 0x82735400..0x82735428. Two stack locals, both cleared before the walk:
    //   * a private FastBitArray<600> marking params already swallowed by an earlier run, so a
    //     five-car jam is drained once and not once per member (the ten `stdx r24` fields at
    //     0x8273540C..0x8273541C);
    //   * the run collector (`stw r24, var_120` == Array<u16,64>::Clear at 0x82735428).
    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Counters + the A/B suppressor; read
    // once so the env lookup is not in the per-param path.
    static const bool sbDIAGSuppressFlagging = (getenv("BRN_TRAFFIC_NO_JAM_NUKE") != 0);
    u32 luDIAGJamsFound  = 0;
    u32 luDIAGAliveSeen  = 0;
    u32 luDIAGLongestRun = 0;
    u32 luDIAGWouldFlag  = 0;
    u32 luDIAGFlagged    = 0;

    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> lxParamsAlreadyInAJam;
    lxParamsAlreadyInAJam.Construct();

    ::Array<u16, KU_JAM_RUN_CAPACITY> laJamRun;
    laJamRun.Clear();

    // 0x82735424..0x8273554C. `addis r9,r16,4 ; addi r9,r9,-0x2900` == this+0x3D700 ==
    // &mParamSoaData.mAliveParams -- the iterator's mpxSourceMasks. The whole loop is the
    // console's inlined Begin()/operator++/!=End() over that set; End() is the literal 0x258
    // it compares against at 0x8273650C.
    const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrAliveParams =
        mParamSoaData.mAliveParams;

    for (CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>::Iterator lItParam = lrAliveParams.Begin();
         lItParam != lrAliveParams.End();
         ++lItParam)
    {
        const s32 liParam = lItParam.GetIndex();
        ++luDIAGAliveSeen;   // [DIAG]

        // 0x82735958..0x82735980. The console tests the local set with the ITERATOR'S cached
        // mask rather than re-deriving 1<<(i&63) -- same bit, one instruction cheaper. Spelled
        // through the attested IsBitSet here; GetMask() exists but there is no attested field
        // accessor to pair it with.
        if (lxParamsAlreadyInAJam.IsBitSet(static_cast<u32>(liParam)))
        {
            continue;
        }

        // 0x827359E8..0x82735A08. THIS function's own assert (baked file
        // BrnTrafficEntityModule.cpp, line 8056) -- kept, unlike the accessor-owned ones.
        CGS_ASSERT(static_cast<u32>(liParam) < KU_MAX_PARAMS, "luCurrParam < KU_MAX_PARAMS");

        // ---- collect the maximal jam run through this param --------------------------------
        // The console emits the run-walk body TWICE -- one source helper, inlined at both
        // sites. It is kept as two blocks here rather than re-rolled: the two entries differ
        // (backward starts AT liParam, forward starts at liParam's successor and must first
        // test for the 0xFFFF terminator), and the helper's name is not recoverable, so an
        // extracted method would be an invented member on a DWARF-attested class.

        // ---- walk A: BACKWARD through muPrevParam, starting at liParam itself ---------------
        // 0x82735A2C..0x82735C5C.
        {
            u32    luCurrParam = static_cast<u32>(liParam);
            Param* lpParam     = GetParam(luCurrParam);

            while (lpParam->miBehaviour == KI_JAM_BEHAVIOUR)
            {
                // 0x82735A50..0x82735A60. A car at or above 5 m/s is moving: the run ends here.
                if (lpParam->mfSpeed >= KF_JAM_MAX_SPEED)
                {
                    break;
                }

                // 0x82735A88..0x82735A90. The collector is fixed at 64; a longer run is simply
                // truncated (the console does NOT drain-and-continue).
                if (laJamRun.GetCount() == static_cast<s32>(KU_JAM_RUN_CAPACITY))
                {
                    break;
                }

                // 0x82735A94..0x82735BEC.
                laJamRun.Append(static_cast<u16>(luCurrParam));
                lxParamsAlreadyInAJam.SetBit(luCurrParam);

                // 0x82735C0C..0x82735C24. `lhz r11, 2(node)` == ParamListNode::muPrevParam.
                const u16 lu16Prev = GetParamListNode(luCurrParam)->muPrevParam;
                if (lu16Prev == KU_INVALID_PARAM)
                {
                    break;
                }
                luCurrParam = lu16Prev;
                lpParam     = GetParam(luCurrParam);
            }
        }

        // ---- walk B: FORWARD through muNextParam, starting at liParam's successor -----------
        // 0x82735C80..0x82735EE0. liParam itself is deliberately NOT revisited -- walk A
        // already collected it.
        {
            const u16 lu16First = GetParamListNode(static_cast<u32>(liParam))->muNextParam;

            if (lu16First != KU_INVALID_PARAM)
            {
                u32    luCurrParam = lu16First;
                Param* lpParam     = GetParam(luCurrParam);

                while (lpParam->miBehaviour == KI_JAM_BEHAVIOUR)
                {
                    if (lpParam->mfSpeed >= KF_JAM_MAX_SPEED)
                    {
                        break;
                    }

                    if (laJamRun.GetCount() == static_cast<s32>(KU_JAM_RUN_CAPACITY))
                    {
                        break;
                    }

                    laJamRun.Append(static_cast<u16>(luCurrParam));
                    lxParamsAlreadyInAJam.SetBit(luCurrParam);

                    // 0x82735E94..0x82735EA8. `lhzx r11` at node+0 == muNextParam.
                    const u16 lu16Next = GetParamListNode(luCurrParam)->muNextParam;
                    if (lu16Next == KU_INVALID_PARAM)
                    {
                        break;
                    }
                    luCurrParam = lu16Next;
                    lpParam     = GetParam(luCurrParam);
                }
            }
        }

        // [DIAG] the longest run collected THIS PASS, jam or not -- without it a session that
        // reports no jams cannot distinguish "queues form but stay short" from "behaviour 5
        // never happens at all", and those need completely different follow-up.
        if (static_cast<u32>(laJamRun.GetCount()) > luDIAGLongestRun)
        {
            luDIAGLongestRun = static_cast<u32>(laJamRun.GetCount());
        }

        // ---- drain -------------------------------------------------------------------------
        // 0x82735F08..0x82736200. FIVE cars or more, and then only every third one.
        if (laJamRun.GetCount() > KI_JAM_MIN_RUN_LENGTH)
        {
            ++luDIAGJamsFound;

            for (u32 luSlot = 0;
                 luSlot < static_cast<u32>(laJamRun.GetCount());
                 luSlot += KU_JAM_KILL_STRIDE)
            {
                const u32 luParamToKill = laJamRun.GetItem(luSlot);

                // 0x82735F54..0x82735F68. mbAllowDivergentBehaviour is the gate, and the two
                // player-visibility tests live entirely inside it: an online non-showtime
                // event removes jammed cars with no regard to where anyone is looking, because
                // there every client sees a different view and the console will not let one
                // player's camera decide what the shared simulation does.
                if (mbAllowDivergentBehaviour)
                {
                    // 0x82736090..0x827360D0. Never remove a car that was drawn last frame.
                    // Standard-traffic vehicle index == param index (KU_MAX_PARAMS ==
                    // KU_MAX_STANDARD_TRAFFIC), which is why a param index reads this set.
                    if (mVehicleSoaData.mVehiclesRenderedLastFrame.IsBitSet(luParamToKill))
                    {
                        continue;
                    }

                    // 0x827360F4..0x82736198. And never pop one within 40 m of the camera,
                    // even if it happened not to be drawn. The console computes the magnitude
                    // (vrsqrtefp + two Newton steps + a vsel zero-guard) and compares it, not
                    // the square -- de-optimised to the exact form here, as BrnMathUtils.cpp
                    // does for the same idiom.
                    const Vector3 lToParam = GetParamTransform(luParamToKill)->GetLerpedPos()
                                           - mCameraLastFrame.GetPosition();

                    if (Magnitude(lToParam) < KF_JAM_CAMERA_PROTECTION_RADIUS)
                    {
                        continue;
                    }
                }

                ++luDIAGWouldFlag;

                // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. The A/B switch the
                // before/after capture needs: with BRN_TRAFFIC_NO_JAM_NUKE set, the walk, the
                // run collection and every guard above still run exactly as the console does
                // them -- only the flag store is withheld, so the two runs differ in ONE
                // store and nothing else. That is the point: an A/B built from two different
                // binaries, or one that also skipped the walk, would not isolate the valve.
                if (sbDIAGSuppressFlagging)
                {
                    continue;
                }

                // 0x827361BC..0x827361F8. The whole point of the function. UpdateParams
                // (_wT2_02.cpp) picks the flag up on its next pass and calls KillParam.
                CGS_ASSERT(GetParam(luParamToKill)->IsAlive(), "IsAlive()");
                GetParam(luParamToKill)->SetShouldBeRemoved();
                ++luDIAGFlagged;
            }
        }

        // 0x82736204. Reused for the next seed param, drained or not.
        laJamRun.Clear();
    }

    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. One line per pass that found a jam,
    // so the pixels have numbers beside them. ⚠️ The PIXELS are the evidence, not this: a jam
    // that this line says was flagged has still only been PROVEN cleared when the cars are
    // gone from a screenshot, because SetShouldBeRemoved is a request to UpdateParams, not a
    // removal. Counting the request and calling it a removal is exactly the class of gate
    // that has shipped broken work here three times.
    static u32 suDIAGPass        = 0;
    static u32 suDIAGBestEverRun = 0;
    ++suDIAGPass;
    if (luDIAGLongestRun > suDIAGBestEverRun)
    {
        suDIAGBestEverRun = luDIAGLongestRun;
    }

    // Print on a jam, on a NEW session-best run length, or as a 600-pass heartbeat -- so a
    // session with no jams still says how close it got and how many params were alive to
    // queue in the first place.
    const bool lbReport = (luDIAGJamsFound != 0)
                       || (luDIAGLongestRun >= 2 && luDIAGLongestRun == suDIAGBestEverRun)
                       || ((suDIAGPass % 600u) == 0u);

    if (lbReport && getenv("BRN_TRAFFIC_DIAG") != 0
        && (CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[jam-nuke] pass="  << static_cast<s32>(suDIAGPass)
            << " aliveParams="     << static_cast<s32>(luDIAGAliveSeen)
            << " longestRun="      << static_cast<s32>(luDIAGLongestRun)
            << " bestEver="        << static_cast<s32>(suDIAGBestEverRun)
            << " jams="            << static_cast<s32>(luDIAGJamsFound)
            << " passedGuards="    << static_cast<s32>(luDIAGWouldFlag)
            << " flagged="         << static_cast<s32>(luDIAGFlagged)
            << (sbDIAGSuppressFlagging ? " [SUPPRESSED]" : "")
            << "\n";
    }
}

}
