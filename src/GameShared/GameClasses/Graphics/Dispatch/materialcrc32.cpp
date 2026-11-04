#include "GameShared/GameClasses/Graphics/Dispatch/materialcrc32.h"

// ARTIST 0x827F7878
rw::BaseResourceDescriptors<5> MaterialCRC32::GetResourceDescriptor()
{
    CGS_STATIC_ASSERT((sizeof(MaterialCRC32) == 12), "ARTIST material size");
    CGS_STATIC_ASSERT((__alignof(MaterialCRC32) == 4), "ARTIST material alignment");

    rw::BaseResourceDescriptors<5> lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(MaterialCRC32);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = __alignof(MaterialCRC32);
    return lDescriptor;
}
