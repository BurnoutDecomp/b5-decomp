#pragma once

#include <cstddef>
#include <cstdint>
#include "SDKs/EATech/include/Apt/AptPseudoData.h"

// Names corroborated by the B4Extern PDB. Payload placement follows Paradise's
// XB1 resolve/unresolve twins, sub_1408567D0 and sub_14085EE40.
enum AptControlType
{
    AptControlType_DoAction = 1,
    AptControlType_FrameLabel = 2,
    AptControlType_PlaceObject2 = 3,
    AptControlType_RemoveObject2 = 4,
    AptControlType_BackgroundColour = 5,
    AptControlType_StartSound = 6,
    AptControlType_StartSoundStream = 7,
    AptControlType_DoInitAction = 8
};

struct AptActionBlock
{
    unsigned char* aActionStream;
};

struct AptControlDoAction
{
    AptActionBlock actions;
};

struct AptControlFrameLabel
{
    char* szLabel;
};

struct AptControlDoInitAction
{
    int32_t nSpriteID;
    AptActionBlock actions;
};

// The tag selects a serialized payload. PlaceObject's body is also accessed
// through AptPlaceObjectInfo_t::GetBody(), shared with the display-list readers.
struct AptControl
{
    AptControlType eType;
    union
    {
        AptControlDoAction action;
        AptControlFrameLabel frameLabel;
        AptPlaceObjectBody_t placeObject2;
        AptControlDoInitAction initAction;
    };
};

static_assert(offsetof(AptControl, action.actions.aActionStream) == 0x08, "XB1 action stream");
static_assert(offsetof(AptControl, frameLabel.szLabel) == 0x08, "XB1 frame label");
static_assert(offsetof(AptControl, initAction.nSpriteID) == 0x08, "XB1 init-action id");
static_assert(offsetof(AptControl, initAction.actions.aActionStream) == 0x10, "XB1 init-action stream");
