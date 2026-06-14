#include "RwBaseResourceDescriptors5.h"

namespace rw
{
template <>
BaseResourceDescriptors<5>& BaseResourceDescriptors<5>::operator+=(const BaseResourceDescriptors<5>& lOther)
{
    for (uint32_t luIndex = 0; luIndex < 5; ++luIndex)
    {
        BaseResourceDescriptor& lDescriptor = mDescriptors[luIndex];
        const BaseResourceDescriptor& lOtherDescriptor = lOther.mDescriptors[luIndex];

        if (lOtherDescriptor.m_alignment > 1)
        {
            lDescriptor.m_size =
                (lOtherDescriptor.m_alignment - 1 + lDescriptor.m_size) & ~(lOtherDescriptor.m_alignment - 1);
        }

        if (lDescriptor.m_alignment < lOtherDescriptor.m_alignment)
        {
            lDescriptor.m_alignment = lOtherDescriptor.m_alignment;
        }

        lDescriptor.m_size += lOtherDescriptor.m_size;
    }

    return *this;
}
}
