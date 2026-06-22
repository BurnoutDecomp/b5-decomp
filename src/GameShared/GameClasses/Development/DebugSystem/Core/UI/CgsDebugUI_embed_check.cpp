// Translation-unit embed check for CgsDev::DebugUI::DebugUI.
// Forces the owning header to compile standalone and exercises the asserting 2D
// renderer accessor so its signature stays wired to its home.
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

namespace
{
void EmbedCheck(const CgsDev::DebugUI::DebugUI* lpUI)
{
    CgsDev::Debug2DImmediateRender* const lpRender = lpUI->Get2DRenderer();
    (void)lpRender;
}
}
