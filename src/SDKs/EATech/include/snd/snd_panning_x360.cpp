// ARTIST Snd9 speaker panning: 0x82B73000, 0x82B73080,
// 0x82B73DF0, 0x82B73E78, 0x82B7CBD8, and 0x82B7CFF8.
#include "types.hpp"
#include "SDKs/EATech/include/snd/sndsys.h"
#include "SDKs/EATech/include/snd/sndhal.h"
#include "rw/audio/core/PlugIn.h"

using rw::audio::core::System;
extern "C" System* off_83271928;

namespace Snd9
{
    // These are the original writable configuration and panning table cells.
    f32 gafSpeakerAzimuths[6];           // 0x83277624
    s16 gasSpeakerConfig2[12];          // 0x832778D4
    s16 gasSpeakerConfig4[12];          // 0x832778EC
    s16 gasSpeakerConfig6[12];          // 0x83277904
    u8* gapAzimuthToSpeaker[6];         // 0x83277700
    u8* gpAzimuthToSpeakerStorage;      // 0x83277C94
}

// These polynomial helpers are the native Snd functions, rather than CRT trig.
extern "C" f32 SNDI_sin(f32 x)
{
    while (x > 6.2831854820251465f) x -= 6.2831854820251465f;
    const f32 x2 = x*x;
    const f32 x3 = x2*x;
    const f32 x5 = x3*x2;
    const f32 x7 = x5*x2;
    const f32 x9 = x7*x2;
    const f32 x11 = x9*x2;
    const f32 x13 = x11*x2;
    return x - x3*0.1666666716337204f + x5*0.008333330042660236f
           - x7*0.00019841270113829523f + x9*0.0000027557318844628753f
           - x11*0.000000025052107943679403f + x13*0.00000000016058999963153298f;
}
extern "C" f32 SNDI_cos(f32 x)
{
    while (x > 6.2831854820251465f) x -= 6.2831854820251465f;
    const f32 x2=x*x, x4=x2*x2, x6=x4*x2, x8=x4*x4, x10=x8*x2, x12=x6*x6;
    return 1.0f-x2*0.5f+x4*0.0416666679084301f-x6*0.0013888889225199819f
           +x8*0.000024801587642286904f-x10*0.0000002755731998149713f
           +x12*0.00000000208767581000302f;
}

// ARTIST 0x82B73080.
extern "C" void SNDI_spkrconfig()
{
    using namespace Snd9;
    gafSpeakerAzimuths[0]=0.0f; gafSpeakerAzimuths[1]=45.0f;
    gasSpeakerConfig2[0]=-8192; gasSpeakerConfig2[1]=8192;
    gasSpeakerConfig4[0]=-8192; gasSpeakerConfig4[1]=8192;
    gafSpeakerAzimuths[2]=135.0f; gafSpeakerAzimuths[3]=225.0f;
    gasSpeakerConfig4[2]=-24576; gasSpeakerConfig4[3]=24576;
    gafSpeakerAzimuths[4]=315.0f;
    gasSpeakerConfig6[0]=-8192; gasSpeakerConfig6[1]=0; gasSpeakerConfig6[2]=8192;
    gasSpeakerConfig6[3]=-24576; gasSpeakerConfig6[4]=24576; gasSpeakerConfig6[5]=0;
}

// ARTIST 0x82B73000: azimuth high byte selects one of 256 native entries.
extern "C" void SNDI_aztospkrvol(u16 azimuth, Snd9::Hal::SpeakerVol* volumes)
{
    const u8 index = static_cast<u8>(azimuth >> 8);
    for (s32 i=0; i<Snd9::gOptions.mOutput.mucDirectOutputCount; ++i)
        volumes->mafGain[i] = static_cast<f32>(Snd9::gapAzimuthToSpeaker[i][index]) * 0.003921568859368563f;
    volumes->mafGain[5] = 0.0f;
}

namespace
{
    u8 ToByte(f32 value)
    {
        const f32 scaled=value*255.0f;
        return static_cast<u8>(static_cast<s32>(scaled < 0.0f ? scaled-0.5f : scaled+0.5f));
    }
    void SetPair(s32 index, s32 first, s32 second, f32 angle, f32 firstAngle, f32 secondAngle)
    {
        const f32 rad=0.01745329424738884f;
        const f32 angleRadians=angle*rad;
        const f32 firstRadians=firstAngle*rad;
        const f32 secondRadians=secondAngle*rad;
        const f32 phase=((angleRadians-secondRadians)/(firstRadians-secondRadians))*1.5707963705062866f;
        const f32 sine=SNDI_sin(phase);
        const f32 cosine=SNDI_cos(phase);
        Snd9::gapAzimuthToSpeaker[first][index]=ToByte(sine);
        Snd9::gapAzimuthToSpeaker[second][index]=ToByte(cosine);
    }
}

// ARTIST 0x82B7CBD8: one separately allocated 256-byte row per direct output.
extern "C" void SNDI_precalcaztospkrvol()
{
    using namespace Snd9;
    System::Lock(off_83271928);
    gpAzimuthToSpeakerStorage=static_cast<u8*>(System::Alloc(off_83271928,
        static_cast<u32>(gOptions.mOutput.mucDirectOutputCount)*256u, "Azimuth to Spkr Table",16,0));
    System::Unlock(off_83271928);
    const s32 count=gOptions.mOutput.mucDirectOutputCount;
    u8* row=gpAzimuthToSpeakerStorage;
    for (s32 i=0;i<count;++i) { gapAzimuthToSpeaker[i]=row; row+=256; }
    for (s32 i=0;i<256;++i)
    {
        const f32 angle=static_cast<f32>((i<<8)&0xff00)*0.0054931640625f;
        if (count==1) { gapAzimuthToSpeaker[0][i]=127; continue; }
        for (s32 j=0;j<count;++j) gapAzimuthToSpeaker[j][i]=0;
        if (angle<=gafSpeakerAzimuths[0])
        {
            if (count>=5) SetPair(i,0,1,angle,gafSpeakerAzimuths[0],gafSpeakerAzimuths[1]);
            else SetPair(i,0,count-1,angle,gafSpeakerAzimuths[0],-(360.0f-gafSpeakerAzimuths[count-1]));
        }
        else if (angle>=gafSpeakerAzimuths[count-1])
            SetPair(i,0,count-1,-(360.0f-angle),gafSpeakerAzimuths[0],-(360.0f-gafSpeakerAzimuths[count-1]));
        else
            for (s32 j=0;j<count;++j)
                if (angle>=gafSpeakerAzimuths[j] && angle<=gafSpeakerAzimuths[j+1])
                    SetPair(i,j,j+1,angle,gafSpeakerAzimuths[j],gafSpeakerAzimuths[j+1]);
    }
}

// ARTIST 0x82B7CFF8. The original does not clear the global after freeing.
extern "C" void SNDI_freespkrtable()
{
    System::Lock(off_83271928);
    System::Free(off_83271928,Snd9::gpAzimuthToSpeakerStorage,0);
    System::Unlock(off_83271928);
}
