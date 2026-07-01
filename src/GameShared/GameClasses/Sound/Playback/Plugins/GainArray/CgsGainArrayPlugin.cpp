#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsSound::Playback::Plugins::GainArray)
//
//   CreateInstance @ 0x826C3A10 -- placement init of a gain-array plugin instance:
//       if (instance) instance[0] = vtable;          // off_820AE188
//       instance[3] = &instance[10];                 // gain block self-pointer
//       for 6 iterations: set two gain arrays to 1.0
//           interleaved: instance[10],[12],[14],[16],[18],[20]
//           contiguous : instance[22],[23],[24],[25],[26],[27]
//       return 1;
//   GetSize @ 0x82689E08 -- return 112; (byte size of the instance)
//   Process @ 0x8268CDB0 -- per-channel gain ramp across a double-buffered pair of
//       mix buffers held by the splicer content, then swap the two buffer slots.
//   `scalar deleting destructor' @ 0x826AA9E0 -- reset vptr to the destructing
//       vtable (off_820AA810) then conditionally operator delete.
//
// Offsets are in 32-bit words from the instance base, matching the Hex-Rays
// `_DWORD*` view.

namespace rw { namespace audio { namespace core {

// Forward-modelled BY NAME. GainArray::Process reads only mpData (+0x04, lwz 4)
// and muStride (+0x0E, lhz 0xE); the full RenderWare MixBuffer lives in another TU.
struct MixBuffer
{
    u8  mPad0[4];   // +0x00
    f32* mpData;    // +0x04
    u8  mPad8[6];   // +0x08
    u16 muStride;   // +0x0E
};

// External RenderWare mixer entry point (bl rw__audio__core__CopyWithGainRamp).
// r3=dst, r4=src, f1=startGain, f2=gainStep, r7=sampleCount.
extern void CopyWithGainRamp( f32* lpDst, f32* lpSrc, float lfGain, float lfStep, int liCount );

}}} // namespace rw::audio::core

namespace CgsSound { namespace Playback { namespace Plugins {

    // Plugin vtable (off_820AE188), installed live by CreateInstance.
    extern void* const gpGainArrayVTable;
    // Destructing vtable (off_820AA810), installed by the scalar deleting dtor.
    extern void* const gpGainArrayDtorVTable;

    // Fixed per-block ramp-step scale (flt_820ADC00, f31 in Process).
    static const f32 KF_GAIN_RAMP_STEP = 0.0f; // value = flt_820ADC00 rodata

    struct GainArray
    {
        void* mpVtable;        // [0]  word 0 (byte 0x00)
        u32   mau1[2];         // [1..2]
        u8    mau2Pad[8];      // [3..4] bytes 0x0C..0x13
        u32   mau3;            // [5] byte 0x14
        u32   mau4[2];         // [6..7] bytes 0x18..0x1F
        u8    mau5Pad0x20;     // byte 0x20
        u8    mu1Count;        // byte 0x21 (lbz r26,0x21) -- channel count
        u16   mu2Pad0x22;      // bytes 0x22..0x23
        u32   mau6;            // byte 0x24
        f32*  mpGainBlock;     // gain block self-pointer (word 9 region)
        f32   maBlock[18];     // [10..27]  byte 0x28 .. byte 0x6F

        static int CreateInstance(GainArray* lpInstance);
        static int GetSize();
        int  Process(void* lprContent, u8 buFirstPass);
        virtual ~GainArray();
    };

    static const int KI_GAIN_ARRAY_INSTANCE_SIZE = 112;
    static const int KI_GAIN_ARRAY_ENTRIES       = 6;

    int GainArray::CreateInstance(GainArray* lpInstance)
    {
        if (lpInstance)
            lpInstance->mpVtable = gpGainArrayVTable;

        lpInstance->mpGainBlock = lpInstance->maBlock;

        for (int liEntry = 0; liEntry < KI_GAIN_ARRAY_ENTRIES; ++liEntry)
        {
            lpInstance->maBlock[liEntry * 2]   = 1.0f;   // interleaved (byte 0x28,0x30,..)
            lpInstance->maBlock[12 + liEntry]  = 1.0f;   // contiguous  (byte 0x58..0x6C)
        }
        return 1;
    }

    int GainArray::GetSize()
    {
        return KI_GAIN_ARRAY_INSTANCE_SIZE;
    }

    // @ 0x8268CDB0. Ramp each channel's source mix buffer into the destination
    // through CopyWithGainRamp, then flip the double-buffered slot pointers.
    //   lprContent (a2) exposes two MixBuffer* slots:
    //       slotA @ content + 0x3000C   slotB @ content + 0x30010
    //   For channel i in [0,count): dst = *slotB, src = *slotA.
    int GainArray::Process(void* lprContent, u8 buFirstPass)
    {
        typedef rw::audio::core::MixBuffer MixBuffer;

        u8* lpContent = static_cast<u8*>(lprContent);
        MixBuffer** lppSlotA = reinterpret_cast<MixBuffer**>(lpContent + 0x3000C);
        MixBuffer** lppSlotB = reinterpret_cast<MixBuffer**>(lpContent + 0x30010);

        MixBuffer* lpSlotA = *lppSlotA;   // r28
        MixBuffer* lpSlotB = *lppSlotB;   // r27

        const u32 luCount = mu1Count;
        if (luCount)
        {
            f32* lpfTarget  = &maBlock[0];    // this + 0x28, stride 8 bytes
            f32* lpfCurrent = &maBlock[12];   // this + 0x58, stride 4 bytes

            for (u32 lu = 0; lu < luCount; ++lu)
            {
                if (buFirstPass)
                    *lpfCurrent = *lpfTarget;

                const f32 lfStep = (*lpfTarget - *lpfCurrent) * KF_GAIN_RAMP_STEP;

                // r3 (dst) = slot B ; r4 (src) = slot A.
                rw::audio::core::CopyWithGainRamp(
                    lpSlotB->mpData + 4 * lpSlotB->muStride * lu,
                    lpSlotA->mpData + 4 * lpSlotA->muStride * lu,
                    *lpfCurrent,
                    lfStep,
                    256);

                *lpfCurrent = *lpfTarget;

                lpfTarget  += 2;
                lpfCurrent += 1;
            }
        }

        // Double-buffer flip: swap the two slot pointers.
        MixBuffer* lpTmp = *lppSlotA;
        *lppSlotA = *lppSlotB;
        *lppSlotB = lpTmp;

        return 1;
    }

    // @ 0x826AA9E0. Compiler-synthesised `scalar deleting destructor': install the
    // destructing vtable (off_820AA810), then let the emitting delete-expression's
    // operator delete stand in for the X360 (a2&1) free tail. No owned heap state.
    GainArray::~GainArray()
    {
        mpVtable = gpGainArrayDtorVTable; // off_820AA810
    }

}}}
