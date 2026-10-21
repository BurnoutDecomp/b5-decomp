#pragma once

#include "GameShared/GameClasses/Language/CgsSku.h"

namespace CgsSystem
{
    // ARTIST 0x828D6938 and DecFIGS CgsHardwareLanguage.h:45.
    class HardwareLanguage
    {
    public:
        static const CgsLanguage::ELanguage GetHardwareLanguage();
    };
}
