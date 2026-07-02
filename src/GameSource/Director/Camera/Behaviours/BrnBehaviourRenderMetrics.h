#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                        // Vector3
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"    // BrnDirector::Camera::Behaviour base slice
#include "SDKs/EA/GameTalk/GameTalk.h"                             // EA::GameTalk::GameTalkMessage

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.h
//
// BrnDirector::Camera::BehaviourRenderMetrics -- the render-metrics probe
// behaviour: an authoring tool drives it over the EA GameTalk channel to park
// the camera at a requested world position (and optionally an "idle" camera
// pose) while the tool samples render metrics. NO DecFIGS DWARF exists for
// this TU; the class is asm-shape-derived from the one exported method
// (GameTalkMessageReceiver @0x8220FB60, whose assert names the original source
// GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.cpp:132).
//
// FLAG: MINIMAL SLICE -- only the members the receiver touches are named; the
// rest of the behaviour interior (X360 [+0x14, +0x90) and the other gaps) is
// opaque NOMINAL storage. The behaviour's virtual surface (Construct @0x822303B0
// per the BehaviourManager's NewBehaviour table, Update, etc.) is its own
// ledger work -- grow this slice in place when it lands; member NAMES are
// stable. X360 offsets in the comments; the PC layout keeps only the ORDER.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

class BehaviourRenderMetrics : public Behaviour
{
public:
    // @0x8220FB60 (this TU) -- the GameTalk message callback the tool channel
    // registers with a context slot: lppBehaviour points at the live behaviour
    // pointer (the asm loads `*a2` once, before the key loop). For each key of
    // the message (index-iterated through GetNumKeys/GetKey):
    //   "GetMetrics"           content = 3 float32s (a world position): debug-log
    //                          the request; if no metrics run is pending, latch
    //                          the position, clear the idle-camera flag, and
    //                          raise the pending word -- else log the
    //                          "controller was not ready" failure.
    //   "SetIdleMetricsCamera" content = 6 float32s (asserted, cpp:132): floats
    //                          0-2 -> the idle camera position, floats 3-5 ->
    //                          its look target, and raise the idle-camera flag.
    //   "StopRenderMetrics"    clear the pending word.
    // The X360 leaves the last GetNumKeys value in r3; the return is incidental
    // (modelled void).
    static void GameTalkMessageReceiver(EA::GameTalk::GameTalkMessage* lpMessage,
                                        BehaviourRenderMetrics** lppBehaviour);

private:
    // FLAG: opaque NOMINAL storage -- behaviour interior the receiver never
    // touches (X360 [+0x14, +0x90) after the Behaviour base slice).
    u8      maUnmodelledInterior[0x7C];

    Vector3 mIdleMetricsCameraPosition;    // X360 +0x90 ("SetIdleMetricsCamera" floats 0-2)
    Vector3 mIdleMetricsCameraTarget;      // X360 +0xA0 ("SetIdleMetricsCamera" floats 3-5)
    bool    mbUseIdleMetricsCamera;        // X360 +0xB0 (raised by "SetIdleMetricsCamera", cleared by "GetMetrics")
    u8      maUnmodelledB1toBC[0xB];       // FLAG: opaque NOMINAL storage (X360 [+0xB1, +0xBC))
    s32     miRenderMetricsRequested;      // X360 +0xBC (1 while a metrics run is pending; "StopRenderMetrics" clears)
    u8      maUnmodelledC0toD0[0x10];      // FLAG: opaque NOMINAL storage (X360 [+0xC0, +0xD0))
    Vector3 mMetricsPosition;              // X360 +0xD0 (the requested probe position)
};

}
}
