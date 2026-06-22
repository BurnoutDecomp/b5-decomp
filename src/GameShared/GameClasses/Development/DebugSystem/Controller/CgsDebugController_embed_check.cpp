// Embed check for CgsDebugController.h: instantiates DebugController and exercises the inline
// accessors the debug UI reads. The ledger function for this header TU
//   CgsDev::DebugManager::SetGamePad @ 0x82815EF8
// is homed in CgsDebugController.cpp (committed); this check covers the header home itself.
#include "GameShared/GameClasses/Development/DebugSystem/Controller/CgsDebugController.h"

namespace
{
    void ExerciseDebugController(CgsDev::DebugController& lrController)
    {
        volatile CgsDev::DebugUI::InputEvent leEvent = lrController.GetInputEvent();
        (void)leEvent;

        volatile f32 lfX  = lrController.GetX();
        volatile f32 lfY  = lrController.GetY();
        volatile f32 lfX2 = lrController.GetX2();
        volatile f32 lfY2 = lrController.GetY2();
        (void)lfX; (void)lfY; (void)lfX2; (void)lfY2;

        volatile bool lbButton = lrController.IsButtonPressed(0);
        volatile char lcKey    = lrController.GetKeyPress();
        volatile CgsDev::DebugController::SpecialKey leSpecial = lrController.GetSpecialKeyPress();
        volatile bool lbCtrl   = lrController.IsCtrlPressed();
        volatile bool lbShift  = lrController.IsShiftPressed();
        volatile bool lbAlt    = lrController.IsAltPressed();
        volatile bool lbPresent= lrController.IsKeyboardPresent();
        volatile bool lbLocked = lrController.IsKeyboardLocked();
        (void)lbButton; (void)lcKey; (void)leSpecial; (void)lbCtrl;
        (void)lbShift; (void)lbAlt; (void)lbPresent; (void)lbLocked;
    }
}

extern void CgsDebugController_embed_check_anchor(CgsDev::DebugController&);
void CgsDebugController_embed_check_anchor(CgsDev::DebugController& lrController)
{
    ExerciseDebugController(lrController);
}
