#pragma once

#include "types.hpp"

// Prop entity streaming publishes three notification classes carrying a zone ID.
// DecFIGS BrnPropEntityModuleIO.h:119-175 supplies their Construct and GetZone APIs.
// ARTIST AddEvent at 0x822C8FE8, 0x822C9120 and 0x822C9258 names each as a class
// and copies one halfword at stride 2.

namespace BrnWorld
{
namespace PropEntityIO
{
    class PropGraphicsLoadedEvent
    {
    public:
        void Construct(u16 luZone) { muZone = luZone; }
        u16 GetZone() const { return muZone; }

    private:
        u16 muZone;
    };

    class PropGraphicsUnloadedEvent
    {
    public:
        void Construct(u16 luZone) { muZone = luZone; }
        u16 GetZone() const { return muZone; }

    private:
        u16 muZone;
    };

    class PropInstancesNeededForZoneEvent
    {
    public:
        void Construct(u16 luZone) { muZone = luZone; }
        u16 GetZone() const { return muZone; }

    private:
        u16 muZone;
    };
}
}
