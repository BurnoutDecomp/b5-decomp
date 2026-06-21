#ifndef SDKS_PACKAGES_ICE_ICECONTROLLER_HPP
#define SDKS_PACKAGES_ICE_ICECONTROLLER_HPP

#include "types.hpp"
#include "SDKs/Packages/ICE/ICEData.hpp"      // ICE::ICETake (the embedded edit take)
#include "SDKs/Packages/ICE/ICEMemory.hpp"    // ICE::ICEPointers (Construct arg)

// ============================================================================
// SDKs/Packages/ICE/ICEController.hpp
//
// ICE::ICEController -- the in-game ICE camera-take EDITOR/driver embedded by
// value at the tail of ICE::ICEManager (X360 ICEManager this+0x1D10). It owns the
// live "edit" take the manager returns when not playing back a movie, plus the
// on-screen editor menus and the editor-active state the manager polls each frame.
//
// **MINIMAL SLICE -- NOT THE FULL ICEController.** This header models ONLY what
// ICE::ICEManager's four reconstructed functions (Construct / Destruct /
// GetCameraTake / Update) touch, accessed BY NAME. The real ICEController is a
// large editor class (Construct/Update/DestructMenus/ConstructMenus/RenderMenus/
// JoyHandler/... per the X360 ledger); its full layout is not yet reconstructed.
// When the ICEController TU lands, GROW THIS HEADER in place to the attested
// layout -- do NOT fork a parallel definition.
//
// X360 EVIDENCE (offsets relative to the controller base = ICEManager this+0x1D10):
//   * Construct(this, ICEPointers*)          @0x8253DA30
//   * Update(this)                           @0x8253F4E8  (called when editor active)
//   * DestructMenus(this)                    @0x8252E8A8
//   * GetCameraTake returns controller+0x28  -> the embedded edit take mEditTake
//   * Update polls controller+0xE98 (s32) > 0 -> editor/menus active flag
//   * Destruct clears three controller pointers at +0xF70/+0xF74/+0xF78
//
// The exact placement of mEditTake (+0x28) and the menu-active flag (+0xE98) and
// the three pointers (+0xF70..) is reproduced with explicit padding buffers so the
// members land at their X360 offsets; every member is accessed BY NAME (this is
// LAYOUT RECOVERY WITH PADDING, not an offset-cast hack). The padding sizes are
// derived purely from the four ICEManager functions' accesses.
//
// FLAG: ICEController is a MINIMAL SLICE. The padding regions (mPad*) stand in for
// not-yet-recovered editor members; only mEditTake, mbMenusActive, and the three
// owned pointers (mpMenuA/B/C) are attested by the ICEManager asm. Replace the
// padding with the real members when the ICEController TU is reconstructed.
// ============================================================================

namespace ICE
{

struct ICEController
{
    // @0x00  Not-yet-recovered leading editor state (cursor / mode / counts). The
    //        ICEManager functions do not read this region; sized so mEditTake lands
    //        at the X360-attested controller+0x28.
    u8 mPad0[0x28];

    // @0x28  The live "edit" camera take. ICEManager::GetCameraTake returns
    //        &mController.mEditTake when not playing back a movie (controller+0x28).
    ICETake mEditTake;

    // Editor state between the edit take and the menu-active flag. Not touched by
    // ICEManager; padding preserves the offset of mbMenusActive. mEditTake ends at
    // 0x28 + sizeof(ICETake); the runtime X360 ICETake is 0x738 bytes, so the gap to
    // the +0xE98 flag is computed from the X360 layout, not this reconstruction's
    // (possibly smaller) sizeof(ICETake). FLAG: pad size assumes the X360 ICETake
    // (0x738); if the reconstructed ICETake differs, the absolute offset drifts --
    // harmless under the parity-by-name gate, but noted.
    u8 mPad1[0xE98 - (0x28 + 0x738)];

    // @0xE98  Editor/menus-active count. ICEManager::Update calls
    //         ICEController::Update(this) and skips the playback advance while this
    //         is > 0 (the editor owns the camera). s32 per the X360 `cmpwi ...,0`.
    s32 miMenusActive;

    // Editor state between the active flag and the three owned menu pointers.
    u8 mPad2[0xF70 - (0xE98 + 0x04)];

    // @0xF70/+0xF74/+0xF78  Three owned editor-menu/widget pointers that
    // ICEManager::Destruct nulls out after DestructMenus (each `if (p) p = 0;`).
    // Modelled as void* (their pointee types are editor types not yet reconstructed).
    void* mpMenuA;   // @0xF70
    void* mpMenuB;   // @0xF74
    void* mpMenuC;   // @0xF78

    // === DECLARE-ONLY (bodies in the ICEController TU; the /c gate does not link) ===
    void Construct(ICEPointers* lpICEPointers);
    void Update();
    void DestructMenus();

    // Editor draw pass. BrnDirector::ICEWrapper::Update calls it each frame on the
    // embedded controller (the editor renders its overlay after the manager advances).
    // FLAG: declaration-only here; its body lands with the ICEController TU.
    void Render();

    // Editor entry points driven by BrnDirector::ICEWrapper (EditorOn/EditorOff).
    // FLAG: these are ICE::ICEAuthor base methods (ICEAuthor::EditorOn,
    // ICEAuthor::SetState), invoked on the editor `this` (== this controller's
    // base). ICEController is-an ICEAuthor, but this minimal slice does not yet
    // model the ICEAuthor base, so the two methods are DECLARED here on the slice
    // (their `this` is identical). When the ICEController TU lands its real
    // ICEAuthor base, MOVE these (and the miMenusActive/edit-mode state) onto that
    // base -- do NOT keep a fork.
    void EditorOn(s32 liMode);
    void SetState(s32 liState);

    // Accessor used by ICEManager::GetCameraTake to return the embedded edit take.
    ICETake* GetEditTake() { return &mEditTake; }

    // Editor-active poll used by ICEManager::Update.
    bool AreMenusActive() const { return miMenusActive > 0; }
};

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICECONTROLLER_HPP
