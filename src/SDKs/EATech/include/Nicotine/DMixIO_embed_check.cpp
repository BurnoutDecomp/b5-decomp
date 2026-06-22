// Tiny standalone embed check: confirms the DMixIO home includes cleanly and
// the accessor surface is callable. Not part of the shipped build.
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

static int EmbedCheck_DMixIO()
{
    Nicotine::DMixIO io;
    int iTotal = 0;
    io.SetDMixInput(0, 1);
    io.SetDMixInputUnsafe(1, 2);
    iTotal += io.GetDMixInput(0);
    iTotal += io.GetDMixOutput(3, Nicotine::DMixIO::DMX_PITCH);
    iTotal += (io.GetDMixInputPtr() ? 1 : 0);
    iTotal += (io.GetDMixOutputPtr() ? 1 : 0);
    io.TurnOnMixOutput();
    io.TurnOffMixOutput();
    iTotal += io.GetStateID();
    iTotal += io.GetSFX_ID();
    iTotal += io.GetInstanceNum();
    iTotal += io.IsSFXObj();
    return iTotal;
}

int g_iDMixIOEmbedCheck = EmbedCheck_DMixIO();
