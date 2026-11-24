// CgsGuiUnity: one of ARTIST's unity translation units (DWARF compile unit CgsGuiUnity.cpp),
// ARTIST 0x82847580-0x828607EC.
// Each member also compiles on its own; this file holds only their includes.
// Include form from ARTIST's __FILE__ strings (0x820E0ED0: ...gameshared\gameclasses\gui\CgsGuiShared.cpp).
// Order: ARTIST's emission passes over the range (MSVC emits bottom-up over the call graph, each
// level in definition order: 94/101 transitions forward).

#include "CgsGuiShared.cpp"
#include "CgsGuiKeyboard.cpp"
#include "View/CustomRenderer/CgsCustomRenderer.cpp"
#include "View/ParticleSystem2d/CgsBillboardRenderer.cpp"
#include "Model/CgsModelModule.cpp"
#include "Model/CgsModelModuleIO.cpp"
#include "Model/CgsModelModuleIO_OutputBuffer.cpp"  // holds functions of CgsModelModuleIO.cpp (x360 tree split)
#include "Model/CgsModelModuleIO_InputBuffer.cpp"  // holds functions of CgsModelModuleIO.cpp (x360 tree split)
#include "Model/CgsEventInterpreterModule.cpp"
#include "Model/CgsEventInterpreterModuleIO.cpp"
#include "Model/CgsEventObserver.cpp"
#include "Model/Resources/CgsGuiResourceModule.cpp"
#include "Model/Resources/CgsGuiResourceModuleIO.cpp"
#include "Model/Resources/CgsGuiResourceModuleIO_OutputBuffer.cpp"  // holds functions of CgsGuiResourceModuleIO.cpp (x360 tree split)
#include "Model/Resources/CgsGuiGeometryObjects.cpp"
#include "View/CgsGuiFontCollection.cpp"
#include "Model/State/CgsGuiStateInterface.cpp"
#include "Model/State/CgsGuiState.cpp"
#include "Model/State/CgsGuiStateMachine.cpp"
#include "Model/State/CgsGuiComponent.cpp"
#include "View/CgsGuiViewModule.cpp"
#include "View/CgsGuiViewModuleIO.cpp"
#include "View/AptInterface/CgsAptAnimChannel.cpp"
#include "View/AptInterface/CgsAptAnimData.cpp"
#include "View/AptInterface/CgsAptAnimator.cpp"
#include "View/AptInterface/CgsAptAux.cpp"
#include "View/AptInterface/CgsAptCommunicator.cpp"
#include "View/AptInterface/CgsAptDataHandler.cpp"
#include "View/AptInterface/CgsAptInterpolator.cpp"
#include "View/AptInterface/CgsAptObjectController.cpp"
#include "View/AptInterface/CgsAptCallbackRender.cpp"  // holds functions of cgsaptrender.cpp (x360 tree split)
#include "View/AptInterface/CgsAptRenderHandler.cpp"  // holds functions of cgsaptrender.cpp (x360 tree split)
#include "View/AptInterface/CgsAptCallbackRenderAllocateString.cpp"  // holds functions of cgsaptrender.cpp (x360 tree split)
#include "View/AptInterface/CgsAptString.cpp"
#include "Model/Resources/CgsGuiPopupResource.cpp"
#include "Model/Resources/CgsAptDataHeaderType.cpp"
#include "Model/Resources/CgsGuiPopupResourceType.cpp"
#include "CgsSaveLoadPS3.cpp"
#include "../SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp"
#include "CgsGuiModule.cpp"
#include "CgsGuiModuleIO.cpp"
#include "CgsGuiModuleIO_OutputBuffer.cpp"  // holds functions of CgsGuiModuleIO.cpp (x360 tree split)
#include "CgsGuiModuleIO_InputBuffer.cpp"  // holds functions of CgsGuiModuleIO.cpp (x360 tree split)
#include "CgsGuiModuleBridges.cpp"
