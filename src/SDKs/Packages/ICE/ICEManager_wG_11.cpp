// ============================================================================
// ICE::ICEManager::Destruct, split out of SDKs/Packages/ICE/ICEManager.cpp: that
// TU's other bodies need ICEController::Construct / ::Update, neither of which has
// a body in the tree.
// DELETE-WHEN: ICEManager.cpp can mount -- then move this body back into it.
// ============================================================================

#include "SDKs/Packages/ICE/ICEManager.hpp"

namespace ICE
{

// ---------------------------------------------------------------------------
// Destruct
//
// Tear down: drop the two owned subsystem pointers the manager cached (the file
// sink at +0x1CF8 and the ICE memory manager at +0x1CFC), let the controller
// destruct its menus, then null the three editor pointers the controller owns
// (+0xF74, +0xF70, +0xF78 in that order, each a guarded `if (p) p = 0;`).
// ---------------------------------------------------------------------------
void ICEManager::Destruct()
{
    // Release the manager-cached subsystem pointers (ownership passes back to the
    // bundle owner; the manager no longer references them).
    mpFileHandler = nullptr;
    mpICEMemory   = nullptr;

    // Tear down the editor menus.
    mController.DestructMenus();

    if (mController.mpMenuA)
        mController.mpMenuA = nullptr;   // +0xF74
    if (mController.mpMenuB)
        mController.mpMenuB = nullptr;   // +0xF70
    if (mController.mpMenuC)
        mController.mpMenuC = nullptr;   // +0xF78
}

} // namespace ICE
