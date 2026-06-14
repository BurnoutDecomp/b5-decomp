#include "rw/rwcore_structs.h"

namespace rw
{
RGBA::RGBA(uint8_t lu8Red, uint8_t lu8Green, uint8_t lu8Blue, uint8_t lu8Alpha)
    : m_rgba((static_cast<uint32_t>(lu8Alpha) << 24) |
             (static_cast<uint32_t>(lu8Blue) << 16) |
             (static_cast<uint32_t>(lu8Green) << 8) |
             static_cast<uint32_t>(lu8Red))
{
}
}
