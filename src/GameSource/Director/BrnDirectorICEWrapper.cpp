// ============================================================================
// GameSource/Director/BrnDirectorICEWrapper.cpp
//
// BrnDirector::ICEWrapper -- the director-side owner/driver of the ICE (In-game
// Camera Editor) runtime. The three functions in this TU:
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
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                                          // ICE::ICEAuthor (GetAuthor's base-identity view)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h" // DebugInterface::Enable/DisableConsole

namespace BrnDirector
{

// ---------------------------------------------------------------------------
// EditorOn
//
// Bring the in-game ICE editor up for the given source take:
//   * take a scoped debug-system handle and turn the on-screen console OFF (the
//     editor draws its own overlay, so the console would clutter it),
//   * cancel any movie playback in progress (the editor owns the camera now),
//   * drive the manager's embedded editor into take-edit mode on lpTakeData
//     (ICEAuthor::EditorOn -- see the header's RETYPED note).
//
// The body is bracketed by an automatic DebugInterface (its constructor acquires the
// singleton DebugManager, its destructor releases it).
// ---------------------------------------------------------------------------
void ICEWrapper::EditorOn(ICE::ICETakeData* lpTakeData)
{
    CgsDev::DebugInterface lDebugInterface;
    lDebugInterface.DisableConsole();

    // The editor takes over the camera, so stop any movie playback first.
    mICEManager.ClearPlaybackData();

    // Enter take-edit mode for the source take on the manager's embedded editor.
    GetAuthor().EditorOn(lpTakeData);
}

// ---------------------------------------------------------------------------
// GetAuthor moved to BrnDirectorICEWrapper.h as a header inline.
//
// It has NO standalone X360 symbol, and its caller GetKeyAnimFromGuid @0x821F69A8
// emits `lwz r11,0x230(r31); addi r3,r11,0x2750` -- zero instructions of its own,
// so the console inlines the accessor away entirely. Keeping an out-of-line body
// here cost the camera family an unresolved external for no fidelity gain, and
// leaving both spellings would be C2084.
// ---------------------------------------------------------------------------
// EditorOff
//
// Take the in-game ICE editor down:
//   * take a scoped debug-system handle and turn the on-screen console back ON,
//   * if the editor is currently active (its menus are up), transition it back to
//     state 2 -- the idle/play state the manager hands the camera back from.
//
// Reads the editor state (> OFF) and only then calls ICEAuthor::SetState(editor, 2). Same
// automatic DebugInterface bracket as EditorOn.
// ---------------------------------------------------------------------------
void ICEWrapper::EditorOff()
{
    CgsDev::DebugInterface lDebugInterface;
    lDebugInterface.EnableConsole();

    // Only transition the editor out if it is currently active.
    ICE::ICEAuthor& lrAuthor = GetAuthor();
    if (lrAuthor.miState > ICE::E_ICE_AUTHOR_STATE_OFF)
        lrAuthor.SetState(ICE::E_ICE_AUTHOR_STATE_EXIT_CONFIRM);
}

// ---------------------------------------------------------------------------
// ReconstructCameraMover
//
// Rebuild the camera mover for this frame: ask the manager for the take currently
// driving the camera, then construct the mover against it.
//
// Arg order to ICECameraMover::Construct -- CONFIRMED against the PS3 DecFIGS prototype
// (0x5BC428): Construct(int liViewIndex, ICECameraAnchor* lpCar, ICECamera* lpCamera,
// ICETake* lpTake, ICEGroup* lpShakeGroup, const IResourceManager* lpResourceMgr). The
// 5th arg is the shake group (ICEGroup*), passed null here. (The 11 reconstructed mover
// functions show the second arg stored at mover+0x00 as its ICECameraAnchor* mpCar, the
// third at +0x04 as mpICECamera, the fourth at +0x110 as mpTake):
//   (mover, viewIndex=1, &mICECameraAnchor, &mICECamera, cameraTake, shakeGroup=0, context)
// (The anchor was a reinterpret_cast of a bare CameraSpaceHandler member until 2026-09-27; the
// member is the DWARF's ICE::ICECameraAnchor mICECameraAnchor now, so no cast is needed.)
// ---------------------------------------------------------------------------
void ICEWrapper::ReconstructCameraMover(const ICE::IResourceManager* lpResourceManager)
{
    ICE::ICETake* lpCameraTake = mICEManager.GetCameraTake();

    mCameraMover.Construct(1, &mICECameraAnchor, &mICECamera, lpCameraTake, 0, lpResourceManager);
}

} // namespace BrnDirector
