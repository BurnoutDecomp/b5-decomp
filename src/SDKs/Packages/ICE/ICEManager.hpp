#ifndef SDKS_PACKAGES_ICE_ICEMANAGER_HPP
#define SDKS_PACKAGES_ICE_ICEMANAGER_HPP

#include "types.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"          // ICE::ICETake (4 embedded takes), ICETakeData, IResourceManager
#include "SDKs/Packages/ICE/ICEMemory.hpp"        // ICE::ICEPointers, ICETimer, ICEMemory
#include "SDKs/Packages/ICE/ICEController.hpp"    // ICE::ICEController (embedded editor/driver)
#include "SDKs/Packages/ICE/ICEFile.hpp"          // ICE::ICEFileHandler (mpFileHandler)

// ============================================================================
// SDKs/Packages/ICE/ICEManager.hpp
//
// ICE::ICEManager -- the top-level owner of the active ICE camera takes. It holds
// four live ICETake instances (the working take, a copy take, a shake take, and a
// movie-playback take), the scalar playback state, the subsystem pointers (frame
// timer, take text sink, ICE memory manager), and an embedded ICEController (the
// in-game take editor/driver). It is constructed once from an ICEPointers bundle,
// torn down on shutdown, returns the currently-active camera take each frame, and
// advances the movie-playback take per frame.
//
// THIS FILE IS THE ICEManager HOME. The four functions reconstructed in this TU --
// Construct / Destruct / GetCameraTake / Update -- get bodies in ICEManager.cpp;
// the rest of the DWARF method set is DECLARATION-ONLY here (each lands a body with
// its own ledger TU; the per-TU `cl /c` gate does not link, so declarations suffice).
//
// ----------------------------------------------------------------------------
// LAYOUT (X360-attested from the four reconstructed functions' asm; the member
// NAMES/TYPES are taken from the DecFIGS DWARF ICEManager.hpp, but the OFFSETS and
// the exact ORDER of the scalar block come from the X360 spine -- the DWARF is the
// version-drifted PS3 build and lists fewer/reordered members):
//
//   @0x0000  ICETake     mTake            (this+0)        ICETake::Construct(this+0,   resMgr)
//   @0x0738  ICETake     mCopyTake        (this+1848)     ICETake::Construct(this+1848,resMgr)
//   @0x0E70  ICETake     mShakeTake       (this+3696)     ICETake::Construct(this+3696,resMgr)
//   @0x15A8  ICETake     mPlaybackTake    (this+5544)     ICETake::Construct(this+5544,resMgr)
//   @0x1CE0  bool        mbPlaybackDataSet  (stb 0; read by GetCameraTake & Update)
//   @0x1CE4  f32         mfAnimElevation    (DWARF; not touched by these 4 funcs)
//   @0x1CE8  s32         miPlayGenericTakeIndex (stw -1)
//   @0x1CEC  u32         muPlayGenericGroupHash (stw 0)
//   @0x1CF0  bool        mbSmoothExit       (stb 0)
//   @0x1CF4  ICETimer*   mpTimer            (= ICEPointers.mpICETimer; Update reads *mpTimer)
//   @0x1CF8  ICEFileHandler* mpFileHandler  (= ICEPointers.mpICEFileHandler; cleared in Destruct)
//   @0x1CFC  ICEMemory*  mpICEMemory        (= ICEPointers.mpICEMemory; cleared in Destruct)
//   @0x1D00  f32         mfOldTimeElapsed   (DWARF; not touched by these 4 funcs)
//   @0x1D04  (12 bytes unrecovered/alignment gap before the controller)
//   @0x1D10  ICEController mController       (this+7440; embedded editor/driver)
//
// NOTE on the X360 scalar order vs DWARF: the X360 stores -1 at +0x1CE8 (the take
// INDEX sentinel) and 0 at +0x1CEC (the group HASH), i.e. miPlayGenericTakeIndex
// PRECEDES muPlayGenericGroupHash -- the opposite of the DWARF's textual order. The
// asm offsets win (AGENTS.md: "pseudocode/asm is the only source of truth for
// offsets and member placement").
//
// FLAG: mfOldTimeElapsed is DWARF-attested but NOT touched by these four functions;
//       placed at +0x1D00 per its DWARF position after mpICEMemory. The 12-byte gap
//       to the controller (+0x1D04..+0x1D0C) is left as an explicit alignment/pad
//       region -- no member is invented for it (none is attested).
// FLAG: ICEController mController is a MINIMAL SLICE (see ICEController.hpp).
// ============================================================================

namespace ICE
{

// Vector3 is referenced by the FixAnimElevation / GetAnimElevationFixup methods
// (declaration-only here). Forward-declared (those bodies live in other TUs; the
// /c gate does not need the full type for a declaration-only pointer parameter).
// Its real home is SDKs/Packages/ICE/ICEMath.hpp (ICE::Vector3).
struct Vector3;

struct ICEManager
{
private:
    // ---- The four live takes (X360 +0x000 / +0x738 / +0xE70 / +0x15A8) ----
    ICETake mTake;          // @0x0000  working take
    ICETake mCopyTake;      // @0x0738  copy/scratch take
    ICETake mShakeTake;     // @0x0E70  camera-shake take
    ICETake mPlaybackTake;  // @0x15A8  movie-playback take

    // ---- Scalar playback state (X360 +0x1CE0 .. +0x1D00) ----
    bool            mbPlaybackDataSet;       // @0x1CE0  a movie is actively playing back
    f32             mfAnimElevation;         // @0x1CE4
    s32             miPlayGenericTakeIndex;  // @0x1CE8  (init -1)
    u32             muPlayGenericGroupHash;  // @0x1CEC  (init 0)
    bool            mbSmoothExit;            // @0x1CF0
    ICETimer*       mpTimer;                 // @0x1CF4  frame timer (provides the timestep)
    ICEFileHandler* mpFileHandler;           // @0x1CF8  take text sink
    ICEMemory*      mpICEMemory;             // @0x1CFC  ICE memory manager
    f32             mfOldTimeElapsed;        // @0x1D00

    // @0x1D04  Unrecovered 12-byte alignment/pad region before the controller. No
    // attested member lives here in these four functions; explicit pad (not an
    // offset hack -- nothing is accessed through it).
    u8              mPadPreController[0x1D10 - (0x1D00 + 0x04)];

    // @0x1D10  The embedded in-game ICE take editor/driver.
    ICEController   mController;

public:
    // Default constructor: constructs the embedded takes (and the controller).
    // Body in ICEManager.cpp.
    ICEManager();                                 // @0x827E13B8

    // ====================================================================
    // Functions OWNED by this TU (bodies in ICEManager.cpp).
    // ====================================================================
    void Construct(ICEPointers* lpICEPointers);   // @0x8253DCF0
    void Destruct();                              // @0x825333F0
    ICETake* GetCameraTake();                     // @0x8252CAD0
    void Update();                                // @0x82540010

    // ====================================================================
    // The rest of the DWARF method set -- DECLARATION-ONLY (bodies in their
    // own TUs). Shapes follow the DWARF; the /c gate does not link.
    // ====================================================================
    void Prepare();
    bool IsEditorOn();
    bool IsEditorOff();
    bool IsCameraPaused();
    f32  GetPlaybackCameraLength();
    void Resolve();
    void Render();
    void ReleaseCameraTake();
    void FixAnimElevation(Vector3* lpPosition);
    void SetupAnimElevation();
    f32  GetAnimElevationFixup(Vector3* lpPosition);
    void SetGenericCameraToPlay(const char* lpcGroupName, s32 liIndex);
    void SetTakeToPlay(ICETakeData* lpTakeData);
    void SetTakeToPlay(ICETakeData* lpTakeData, f32 lfParameter);
    bool IsPlaybackDataSet();
    bool IsGenericCameraPlaying();
    void ChooseReplayCamera();
    void SetSmoothExit(bool lbSmoothExit);
    bool IsSmoothExit();
    f32  GetCurrentTakeParameter();
    void AddShakeTake(ICETakeData* lpTakeData);
};

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEMANAGER_HPP
