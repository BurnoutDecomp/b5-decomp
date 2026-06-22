// Embed-check for class:CgsResource::ResourceIO::InputBuffer (non-const GetResourceQueue).
// Instantiates an InputBuffer by value and exercises the writable accessor so the gate links
// the TU. Not shipped; lives next to the home.
#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"

void* CgsResourceModuleIO_InputBuffer_GetResourceQueue_EmbedCheck()
{
    CgsResource::ResourceIO::InputBuffer kInputBuffer;
    return kInputBuffer.GetResourceQueue();
}
