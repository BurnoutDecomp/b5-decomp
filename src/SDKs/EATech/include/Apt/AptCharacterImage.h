#pragma once

#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/Apt.h"

// ARTIST Unresolve 0x82AF77B0 releases the texture at character +0x10.
struct AptCharacterImage : public AptCharacter
{
    AptAssetTexture mTexture;
};
