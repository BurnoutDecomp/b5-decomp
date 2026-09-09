// ============================================================================
// The ICE::ICEManager default constructor, split out of SDKs/Packages/ICE/ICEManager.cpp:
// that TU's other bodies need ICEController::Construct / ::DestructMenus / ::Update,
// none of which has a body in the tree.
// DELETE-WHEN: ICEManager.cpp can mount -- then move this body back into it.
// ============================================================================

#include "SDKs/Packages/ICE/ICEManager.hpp"

namespace ICE
{

// FLAG: the controller's second take (console offset +0x760) is not a declared member of
// the still-partial ICEController, so this ctor cannot construct it; nothing in the linked
// set reads it. Self-corrects when the ICEController layout is completed.
ICEManager::ICEManager()
{
    // The two bubble look angles the console ctor clears (controller +0x7260/+0x7262).
    mController.mi16BubbleYaw   = 0;
    mController.mi16BubblePitch = 0;
}

} // namespace ICE
