// ============================================================================
// ICE::ICEController::DestructMenus, split out of
// SDKs/Packages/ICE/ICEControllerMenus.cpp: that TU's other bodies (ConstructMenus,
// RefreshMenu, RefreshInfoList) need the widget construct/query API and the author
// state read-outs, none of which has a definition in the tree.
// DELETE-WHEN: ICEControllerMenus.cpp can mount -- then move this body back into it.
// ============================================================================

#include "SDKs/Packages/ICE/ICEController.hpp"                 // ICE::ICEController layout + widget members
#include "SDKs/Packages/ICE/ICEMemory.hpp"                     // ICE::ICEMemory::FreeMemory
#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.hpp"       // ICE::ICEWidgetMenu (the seven menu widgets)

namespace ICE
{

// ---------------------------------------------------------------------------
// DestructMenus
//
// Free the editor menu widgets back to the ICE memory manager's edit heap. The loop
// walks the seven-slot menu-pointer block (KI_ICE_NUM_MENU_WIDGETS) starting at
// mpMenuMain (+0xF80); for each non-null slot it frees the widget's row buffer
// (mpRows, the widget's +0x30) and then a second block.
//
// The recorded code frees through HeapMalloc::Free(&mpMenuC->mEditHeap, block) --
// the committed ICEMemory::FreeMemory frees to that same edit heap, so each free is
// expressed as mpMenuC->FreeMemory(block).
//
// FAITHFUL ODDITY: the second free is handed the ADDRESS OF THE BLOCK's first slot,
// not the widget the iteration is on -- the recorded code computes the block base
// once before the loop and passes that same value on every pass, while only the
// read cursor advances. It is kept as recorded. Nothing constructs the editor menus
// in this build, so all seven slots are null and the loop body never runs.
// ---------------------------------------------------------------------------
void ICEController::DestructMenus()
{
    // The seven menu-widget pointers occupy a contiguous block starting at mpMenuMain.
    ICEWidgetMenu** lppMenus = &mpMenuMain;

    for (s32 li = 0; li < KI_ICE_NUM_MENU_WIDGETS; ++li)
    {
        ICEWidgetMenu* lpMenu = lppMenus[li];
        if (lpMenu)
        {
            mpMenuC->FreeMemory(lpMenu->mpRows);
            mpMenuC->FreeMemory(lppMenus);
        }
    }
}

} // namespace ICE
