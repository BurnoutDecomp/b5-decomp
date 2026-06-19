#pragma once

#include "types.hpp"

// CgsNetwork::NetworkTexture - a network-transmissible texture (the encoded/compressed mugshot image
// buffers the image-manager debug component holds by value). Its internal layout - pixel buffer
// handle, dimensions, format, encode state - is not yet recovered; it is modelled here as a single
// documented opaque storage block so owners can embed it by value at the size the X360 build uses
// (two instances live at +0x0C and +0x28 of the image debug component, i.e. a 28-byte object).
// Construct/Destruct bodies link from the NetworkTexture TU. Replace maStorage with named members
// when the layout is recovered.

namespace CgsNetwork
{
    class NetworkTexture
    {
    public:
        void Construct();
        void Destruct();

    private:
        u8 maStorage[28];   // unrecovered NetworkTexture state (X360 object span)
    };
}
