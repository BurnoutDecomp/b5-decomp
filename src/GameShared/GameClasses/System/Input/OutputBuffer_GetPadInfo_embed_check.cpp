// Embed-check for class:CgsInput::InputIO::OutputBuffer::GetPadInfo.
// Instantiates an OutputBuffer and exercises the GetPadInfo read-lock accessor.
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

namespace
{
    const CgsInput::InputIO::PadOutputInformation* ExerciseGetPadInfo(
        const CgsInput::InputIO::OutputBuffer& rBuffer, s32 iPort)
    {
        return rBuffer.GetPadInfo(iPort);
    }

    void Check()
    {
        CgsInput::InputIO::OutputBuffer kBuffer;
        const CgsInput::InputIO::PadOutputInformation* lpInfo = ExerciseGetPadInfo(kBuffer, 0);
        (void)lpInfo;
    }
}

extern "C" void OutputBuffer_GetPadInfo_embed_check()
{
    Check();
}
