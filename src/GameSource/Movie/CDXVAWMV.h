#ifndef CDXVAWMV_H
#define CDXVAWMV_H

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x????????.
// DXVA WMV decoder wrapper; the constructor marks the object as initialised.
class CDXVAWMV
{
public:
    CDXVAWMV();

private:
    u32 mState; // guest +0
};

#endif
