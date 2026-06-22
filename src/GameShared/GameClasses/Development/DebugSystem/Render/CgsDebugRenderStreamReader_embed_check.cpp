// Embed check for CgsDebugRenderStreamReader: exercises the ledger functions reconstructed here
//   DebugRenderStreamReader::Construct(IResourceAllocator*, s32, s32) @ 0x82820CE8
//   DebugRenderStreamReader::Begin                                    @ 0x82817718
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRenderStreamReader.h"

namespace
{
    void ExerciseRenderStreamReader(CgsDev::DebugRenderStreamReader& lrReader,
                                    rw::IResourceAllocator* lpAllocator)
    {
        lrReader.Construct(lpAllocator, 64 /*liMaxCommands*/, 4096 /*liDataBufferSize*/);
        lrReader.Begin();
    }
}

extern void CgsDebugRenderStreamReader_embed_check_anchor(CgsDev::DebugRenderStreamReader&,
                                                          rw::IResourceAllocator*);
void CgsDebugRenderStreamReader_embed_check_anchor(CgsDev::DebugRenderStreamReader& lrReader,
                                                   rw::IResourceAllocator* lpAllocator)
{
    ExerciseRenderStreamReader(lrReader, lpAllocator);
}
