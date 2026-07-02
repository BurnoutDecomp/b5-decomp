#include "GameSource/Network/BrnNetworkPlayerMenuData.h"

// BrnNetwork::PlayerMenuData -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Network/BrnNetworkPlayerMenuData.cpp):
//   PlayerMenuData::Clear @0x82586598

namespace BrnNetwork
{

// @ 0x82586598 -- base clear, then reset the Burnout-specific view: the four car/
// wheel ids drop to 0, the marked man to "none", the camera to NONE, the X360-only
// float pair to its 0.0/0.85 defaults, the freeburn colour pair to 0 and the
// car-select colour pair to the invalid sentinels.
void PlayerMenuData::Clear()
{
    CgsNetwork::PlayerMenuData::Clear();

    mFreeBurnCarId   = 0;
    mFreeBurnWheelId = 0;
    mCarId           = 0;
    mWheelId         = 0;

    mMarkedManID   = -1;
    meCameraStatus = E_CAMERA_STATUS_NONE;

    mfUnknown48 = 0.0f;
    mfUnknown4C = 0.85f;   // flt_82087030

    mu16FreeburnCarColourIndex   = 0;
    mu16FreeburnPaintFinishIndex = 0;
    mu16CarColourIndex           = KU_INVALID_COLOUR_INDEX;
    mu16PaintFinishIndex         = KU_INVALID_PAINT_FINISH_INDEX;
    mbFinalCarSelection          = false;
}

}
