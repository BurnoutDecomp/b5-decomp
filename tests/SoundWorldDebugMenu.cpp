// The sphere color passed by SoundWorldScene::QuerySoundMap is the original
// dword_82F2F818 value, expressed through RenderWare's production RGBA packing.
#include "rw/rwcore_structs.h"

#include <cstdio>

int main()
{
    const rw::RGBA lColour(0xA0, 0xFF, 0x00, 0xFF);
    const bool lbPackedAsOriginal = lColour.m_rgba == 0xFF00FFA0u;
    std::printf("SoundWorldDebugMenu: 1 checks, %d failures\n",
                lbPackedAsOriginal ? 0 : 1);
    return lbPackedAsOriginal ? 0 : 1;
}
