#pragma once

#include "GameShared/GameClasses/Language/CgsSku.h"

namespace CgsSystem
{
    // ARTIST calls and DecFIGS CgsHardwareSku.h:48 attest this class-static API.
    // The Windows product policy is explicit in CgsHardwareSkuPC.cpp.
    class HardwareSku
    {
    public:
        static const CgsLanguage::ELanguage FindLanguage();
        static const CgsLanguage::ESku GetSku();
    };
}
