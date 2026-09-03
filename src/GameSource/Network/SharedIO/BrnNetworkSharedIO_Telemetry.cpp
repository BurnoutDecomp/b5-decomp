// ============================================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkSharedIO_Telemetry.cpp
// ============================================================================================
// [takedown P1 wave 2026-09-03] BrnNetwork::BrnNetworkModuleIO::TelemetryData -- the 20-byte
// record every E_ACTION_SEND_TELEMETRY (228) post carries (DWARF BrnNetworkSharedIO.h:540).
//
//   Construct(hook)          DWARF :548 -- NO standalone X360 symbol; inlined at every producer as
//                            `stw hook, +0 ; stb 0, +4` (GameStateModule::ProcessTakedownEvents
//                            @0x8238FEF8 / 0x8238FF5C and @0x8238FFB0 / 0x8238FFB8).
//   AddParameter(const char*) DWARF :553 -- X360 0x82354010.
//   AddParameter(Vector3)    DWARF :575 -- X360 sub_8236A8B8 (unnamed in the export; identified by
//                            its four "lVector.GetX()/GetZ()" asserts at BrnNetworkSharedIO.h:721..725
//                            and its tail call into the string overload @0x8236AAAC). Callers:
//                            ProcessTakedownEvents @0x8238FF78, BrnGameModule::BridgeWorldToNetwork.
//
// A partfile beside BrnNetworkSharedIO.cpp (unmounted; RoadRulesDownloadEvent::Construct) so the
// telemetry record can be mounted on its own.
// ============================================================================================
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"            // CgsCore::SPrintf / StrCat
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"    // CgsNetwork::KI_MAX_TELEMETRY_DATA_SIZE
#include <cstring>                                                  // std::strlen

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // ----------------------------------------------------------------------------------------
    // Construct -- inlined on the console: `stw r11, var_D0` (the hook) + `stb r30(0), var_CC`
    // (ProcessTakedownEvents @0x8238FF5C / @0x8238FEF8). An empty parameter string.
    // ----------------------------------------------------------------------------------------
    void TelemetryData::Construct(ETelemetryHook leHook)
    {
        meHook       = leHook;
        macBuffer[0] = 0;
    }

    // ----------------------------------------------------------------------------------------
    // AddParameter(const char*) -- X360 0x82354010.
    //   0x82354010..  strlen(lpcParam) + strlen(macBuffer) > 16 -> the :680 assert
    //   then, if macBuffer is not empty, StrCat(macBuffer, 16, ".") (its CgsStringUtils.h:75
    //   assert inlined: strlen(".") + strlen(macBuffer) < 15), then StrCat(macBuffer, 16, lpcParam)
    //   (same inlined assert). Parameters are dot-separated inside the 16-byte buffer.
    // ----------------------------------------------------------------------------------------
    void TelemetryData::AddParameter(const char* lpcParam)
    {
        CGS_ASSERT(static_cast<s32>(std::strlen(lpcParam) + std::strlen(macBuffer)) <= CgsNetwork::KI_MAX_TELEMETRY_DATA_SIZE,
                   "static_cast< int32_t >( strlen( lpcParam ) + strlen( macBuffer ) ) <= CgsNetwork::KI_MAX_TELEMETRY_DATA_SIZE");

        if (std::strlen(macBuffer) != 0)
        {
            CgsCore::StrCat(macBuffer, static_cast<u32>(sizeof(macBuffer)), ".");
        }
        CgsCore::StrCat(macBuffer, static_cast<u32>(sizeof(macBuffer)), lpcParam);
    }

    // ----------------------------------------------------------------------------------------
    // AddParameter(Vector3) -- X360 sub_8236A8B8. The vector arrives in v1; the X and Z lanes are
    // range-checked against +-10000.0f (flt_82005D9C == 10000.0f, flt_82006C48 == -10000.0f, both
    // read from the image) with `vcmpgtfp.` splats:
    //   0x8236A908  10000 > x   else assert "lVector.GetX() < 10000.0f"   (:721)
    //   0x8236A970  10000 > z   else assert "lVector.GetZ() < 10000.0f"   (:722)
    //   0x8236A9D8  x > -10000  else assert "lVector.GetX() > -10000.0f"  (:724)
    //   0x8236AA38  z > -10000  else assert "lVector.GetZ() > -10000.0f"  (:725)
    // then 0x8236AA6C..0x8236AAA0: `fctiwz` both lanes (truncate toward zero == the C cast) into
    // r6 (x) / r7 (z) and SPrintf(buf, 16, "%i.%i", (int)x, (int)z); 0x8236AAAC hands the string
    // to the overload above. The Y lane is never read.
    // ----------------------------------------------------------------------------------------
    void TelemetryData::AddParameter(Vector3 lVector)
    {
        // The host Vector3 (vendor/renderware rw/math/vpu/types.h) names the four lanes directly;
        // lane 0 == x (`vspltw v0, v1, 0`), lane 2 == z (`vspltw v0, v1, 2`).
        const f32 lfX = lVector.x;
        const f32 lfZ = lVector.z;

        CGS_ASSERT(lfX < 10000.0f,  "lVector.GetX() < 10000.0f");
        CGS_ASSERT(lfZ < 10000.0f,  "lVector.GetZ() < 10000.0f");
        CGS_ASSERT(lfX > -10000.0f, "lVector.GetX() > -10000.0f");
        CGS_ASSERT(lfZ > -10000.0f, "lVector.GetZ() > -10000.0f");

        char lacParam[16];
        CgsCore::SPrintf(lacParam, static_cast<u32>(sizeof(lacParam)), "%i.%i",
                         static_cast<s32>(lfX), static_cast<s32>(lfZ));
        AddParameter(lacParam);
    }
}
}
