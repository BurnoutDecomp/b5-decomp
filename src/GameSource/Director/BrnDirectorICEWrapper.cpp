// ============================================================================
// GameSource/Director/BrnDirectorICEWrapper.cpp
//
// BrnDirector::ICEWrapper -- the director-side owner/driver of the ICE (In-game
// Camera Editor) runtime. The four functions in this TU:
//   ICEWrapper              build the heaps + manager + mover, seed the action queue
//   EditorOn                console off, stop playback, enter editor mode
//   EditorOff               console on, leave the editor if it is active
//   ReconstructCameraMover  rebuild the mover from the active take
//
// The embedded heap / manager / mover / camera / space-handler accesses are expressed
// as logical member construction and named member calls (semantic parity by named
// members; the offsets noted in the header are provenance, never used as casts here).
// ============================================================================

#include "GameSource/Director/BrnDirectorICEWrapper.h"

#include "SDKs/Packages/ICE/ICEData.hpp"                                            // ICE::ICETake (GetCameraTake return)
#include "GameShared/GameClasses/Containers/CgsStack.h"                             // CgsContainers::KI_STACK_UNCONSTRUCTED (action-queue seed)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"    // DebugManager::ThreadSafeAquire/Release
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h" // DebugInterface::Enable/DisableConsole

namespace BrnDirector
{

// ---------------------------------------------------------------------------
// ICEWrapper (ctor)
//
// The base heap, the second scratch heap, the ICE manager, the camera mover, the ICE
// camera and the reference-space cache are all embedded members, so member
// construction performs their builds. The only scalar the ctor body sets is the
// action queue's length word: it is seeded to the Stack's "unconstructed" sentinel
// (the queue is made usable later by Construct()/UpdateAction, which Clear it to 0).
// ---------------------------------------------------------------------------
ICEWrapper::ICEWrapper()
{
    // The action stack starts unconstructed: any use before Construct/Clear asserts.
    mActionQueue.miLength = CgsContainers::KI_STACK_UNCONSTRUCTED;
}

// ---------------------------------------------------------------------------
// EditorOn
//
// Bring the in-game ICE editor up in mode liMode:
//   * take a scoped debug-system handle and turn the on-screen console OFF (the
//     editor draws its own overlay, so the console would clutter it),
//   * cancel any movie playback in progress (the editor owns the camera now),
//   * drive the manager's embedded editor into mode liMode.
//
// The body is bracketed with a DebugInterface auto-acquire/critical-section release
// (DebugInterface()/ThreadSafeRelease over the singleton DebugManager). The console
// toggle is a DebugInterface member; expressed here as the established
// thread-safe-acquire -> use -> release pattern (DebugComponent::Register's idiom).
// ---------------------------------------------------------------------------
void ICEWrapper::EditorOn(s32 liMode)
{
    CgsDev::DebugManager*  lpDebugManager = CgsDev::DebugManager::ThreadSafeAquire();
    CgsDev::DebugInterface lDebugInterface(lpDebugManager);
    lDebugInterface.DisableConsole();

    // The editor takes over the camera, so stop any movie playback first.
    mICEManager.ClearPlaybackData();

    // Enter the requested editor mode on the manager's embedded editor.
    mICEManager.GetEditor().EditorOn(liMode);

    CgsDev::DebugManager::ThreadSafeRelease(lpDebugManager);
}

// ---------------------------------------------------------------------------
// EditorOff
//
// Take the in-game ICE editor down:
//   * take a scoped debug-system handle and turn the on-screen console back ON,
//   * if the editor is currently active (its menus are up), transition it back to
//     state 2 -- the idle/play state the manager hands the camera back from.
//
// Reads the editor's menus-active count (> 0) and only then calls
// ICEAuthor::SetState(editor, 2). Same auto-acquire/release bracket as EditorOn.
// ---------------------------------------------------------------------------
void ICEWrapper::EditorOff()
{
    CgsDev::DebugManager*  lpDebugManager = CgsDev::DebugManager::ThreadSafeAquire();
    CgsDev::DebugInterface lDebugInterface(lpDebugManager);
    lDebugInterface.EnableConsole();

    // Only transition the editor out if it is currently active.
    ICE::ICEController& lEditor = mICEManager.GetEditor();
    if (lEditor.AreMenusActive())
        lEditor.SetState(2);

    CgsDev::DebugManager::ThreadSafeRelease(lpDebugManager);
}

// ---------------------------------------------------------------------------
// ReconstructCameraMover
//
// Rebuild the camera mover for this frame: ask the manager for the take currently
// driving the camera, then construct the mover against it. The mover's two by-address
// inputs are the wrapper's own ICE camera and its per-frame reference-space cache;
// liContext is the per-call director context the mover blends against.
//
// Arg order to ICECameraMover::Construct (RECONCILED from the earlier opaque
// input-block model -- arg3 is &mCameraSpaceHandler (+0x11ED0), arg4 is &mICECamera
// (+0x11D60)):
//   (mover, 1, &mCameraSpaceHandler, &mICECamera, cameraTake, 0, context)
// FLAG: the leading `1` is a bool/flag and the `0` a null/extra block, both literal in
//   the call.
// ---------------------------------------------------------------------------
void ICEWrapper::ReconstructCameraMover(s32 liContext)
{
    ICE::ICETake* lpCameraTake = mICEManager.GetCameraTake();

    mCameraMover.Construct(true, &mCameraSpaceHandler, &mICECamera, lpCameraTake, 0, liContext);
}

} // namespace BrnDirector
