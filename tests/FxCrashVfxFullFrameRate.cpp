// FX-CRASHVFX (crash parity 2026-09-25): BrnGameModule::DoDispatch @0x823DC458 writes
// DispatchThreadInputBuffer::mbIsRenderingAtFullFrameRate from the director camera's current flag set -- full rate
// only for E_FLAG_RACING_GAMEPLAY_CAMERA (bit 3) or E_FLAG_ROAD_FOLLOWING_CAM (bit 27). Every other camera (crash,
// takedown, jump...) runs the particle module in its reduced-frame-rate arm. The PC never wrote the flag.
//
// run_fxcrashvfx_full_frame_rate.py extracts the PRODUCTION statement (`const bool lbFullFrameRate = ...;`) out of
// DoDispatch and compiles it against the real BrnDirector::Camera::CameraState. The expected values are the CONSOLE'S
// OWN OUTPUTS: FxCrashVfxFullFrameRateData.h is written by scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/
// gen_fullrate_data.py, which runs 0x823DC5C8..0x823DC630 on emu64 and records the byte stored at +0x99B0 for 93
// flag patterns (every single bit, none, all, and random words with and without the two bits).
#include "types.hpp"
#include "GameSource/Director/Camera/BrnCameraState.h"

#include <cstdio>
#include <cstring>

#include "FxCrashVfxFullFrameRateData.h"

static bool ProductionFullFrameRate(const BrnDirector::Camera::CameraState& lrCameraState)
{
#include "fxcrashvfx_full_frame_rate.inc"
    return lbFullFrameRate;
}

int main()
{
    const u32 luNumCases = static_cast<u32>(sizeof(kaFullFrameRateCases) / sizeof(kaFullFrameRateCases[0]));
    u32 luSame = 0, luShown = 0;
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const FullFrameRateCase& lrCase = kaFullFrameRateCases[luCase];
        BrnDirector::Camera::CameraState lState;
        std::memset(&lState, 0, sizeof(lState));
        lState.mCurrentFlags.maxBits[0] = lrCase.muFlags;
        const bool lbGot  = ProductionFullFrameRate(lState);
        const bool lbWant = lrCase.muStored != 0;
        luSame += (lbGot == lbWant) ? 1u : 0u;
        if (lbGot != lbWant && luShown < 4u)
        {
            ++luShown;
            std::printf("      flags %016llX: got %d want %d\n", static_cast<unsigned long long>(lrCase.muFlags),
                        lbGot ? 1 : 0, lbWant ? 1 : 0);
        }
    }
    const bool lbPass = luSame == luNumCases;
    std::printf("%s  the flag DoDispatch writes is the console's for every pattern (bit 3 or bit 27 -> full rate): "
                "%u/%u\n", lbPass ? "pass" : "FAIL", luSame, luNumCases);
    std::printf("FxCrashVfxFullFrameRate: 1 checks, %u failures\n", lbPass ? 0u : 1u);
    return lbPass ? 0 : 1;
}
