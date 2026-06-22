#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"

#include <cstddef>   // offsetof

// Embed/layout check for BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene. Proves the
// scene-input-interface member offset (+16) the X360 Construct addresses and exercises the
// bodied Construct.

namespace
{
    using BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene;

    void Exercise(OutputBuffer_PreScene* lpBuffer)
    {
        OutputBuffer_PreScene::_AssertLayout();   // proves mSceneInputInterface @16
        lpBuffer->Construct();
    }
}

extern "C" int BrnTriggerEntityModuleIO_embed_check_anchor(void) { return 0; }
