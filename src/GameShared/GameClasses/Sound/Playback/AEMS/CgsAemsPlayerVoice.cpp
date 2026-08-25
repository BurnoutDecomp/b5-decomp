#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826A2B58
//   CgsSound::Playback::AemsPlayerVoice::GetClientAllocationSize(Factory&, const VoiceSpec&)
//   (DWARF CgsAemsPlayerVoice.h:242, static, returns size_t).
//   called by AemsPlayerVoice::operator new and the AemsPlayerVoice ctor.
//
// X360 behaviour (asm, big-endian):
//   - recover the concrete AemsFactory registry (Factory-4 -> +0x60 == mpRegistry).
//   - key = *VoiceSchema (the schema's first word == its interned Name).
//   - lpCsis = Registry::GetEntity<AemsVoiceCsisClass>(mpRegistry, &key), asserted != 0.
//   - csisStart = lhz *(lpCsis + 0xC)  == AemsVoiceCsisClass::mu16UserParameterStart
//     (2026-08-25 wave 6: an earlier comment misnamed this the "parameter count" --
//     the DWARF member at +0xC is mu16UserParameterStart; mu32ParameterCount is the
//     u32 at +0x8).
//   - fs = VoiceSchema::GetFeatureSchema(GetVoiceSchema(spec), 0) (GetVoiceSchema re-called).
//   - return 4 * ( *(fs + 8) + csisStart + 0x2A )   (paramSchemaCount + csisStart + 42).
//
// All fields reached BY NAME via the real homes (2026-08-25 wave 6 fold).

namespace CgsSound
{
namespace Playback
{
    size_t AemsPlayerVoice::GetClientAllocationSize(Factory& arFactory, const VoiceSpec& arVoiceSpec)
    {
        Registry* lpRegistry = GetAemsFactoryRegistry(&arFactory);
        CGS_ASSERT(lpRegistry, "mpRegistry");

        const VoiceSchema& lrSchema = arVoiceSpec.GetVoiceSchema();
        Name lSchemaName = lrSchema.GetName();
        const AemsVoiceCsisClass* lpCsis = lpRegistry->GetEntity<AemsVoiceCsisClass>(lSchemaName);
        CGS_ASSERT(lpCsis, "lpCsis");

        const u32 lu32CsisUserParameterStart = lpCsis->GetUserParameterStart();

        // GetVoiceSchema is re-called (matching the X360 second bl) before GetFeatureSchema(0).
        const FeatureSchema& lrFeatureSchema = arVoiceSpec.GetVoiceSchema().GetFeatureSchema(0);

        return 4u * (lrFeatureSchema.GetParameterSchemaCount() + lu32CsisUserParameterStart + 42u);
    }
}
}
