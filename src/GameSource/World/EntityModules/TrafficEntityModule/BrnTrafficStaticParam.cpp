#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX, corroborated by the Feb-2007 leak
// (SharedClasses/Traffic/BrnTrafficStaticParam.cpp), which is ground truth here:
//   BrnTraffic::StaticTrafficParam::Construct   @ 0x82751EF8
//   BrnTraffic::StaticTrafficParam::Initialise  @ 0x82751F18
//
// Construct resets the param to "no vehicle"; Initialise stamps it live with a
// vehicle type, hull and per-hull index. Modelled with named members (the X360
// pseudocode's raw offsets carry a Construct/Initialise field-width ambiguity that
// the leak source resolves).

namespace BrnTraffic
{
    static const u16 KU_INVALID_HULL = 0xFFFF;

    enum E_STATIC_TRAFFIC_FLAGS
    {
        E_FLAG_ALIVE = 1,
    };

    class StaticTrafficParam
    {
        u16 muHull;
        u8  muStaticTrafficIndexOnHull;
        u8  mxFlags;
        u8  muVehicleType;

    public:
        void Construct();
        void Initialise(u8 luVehicleType, u16 luHull, u8 luIndexOnHull);
    };

    void StaticTrafficParam::Construct()
    {
        muHull = KU_INVALID_HULL;
        muStaticTrafficIndexOnHull = static_cast<u8>(~0);
        mxFlags = 0;
        muVehicleType = static_cast<u8>(~0);
    }

    void StaticTrafficParam::Initialise(u8 luVehicleType, u16 luHull, u8 luIndexOnHull)
    {
        mxFlags = E_FLAG_ALIVE;
        muVehicleType = luVehicleType;
        muHull = luHull;
        muStaticTrafficIndexOnHull = luIndexOnHull;
    }
}
