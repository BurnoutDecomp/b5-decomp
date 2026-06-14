#pragma once

#include "rw/rwcore_structs.h"

namespace rw
{
template <uint32_t Count>
struct BaseResourceDescriptors
{
    BaseResourceDescriptor mDescriptors[Count];

    BaseResourceDescriptors& operator+=(const BaseResourceDescriptors& lOther);
};
}
