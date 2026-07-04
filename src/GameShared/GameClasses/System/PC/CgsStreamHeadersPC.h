#ifndef CGS_SYSTEM_PC_STREAMHEADERSPC_H
#define CGS_SYSTEM_PC_STREAMHEADERSPC_H

#include "types.hpp"

// ============================================================================
//  CgsSystem::StreamHeadersPC -- the runtime stream-header resolution chain, on
//  the ORIGINAL X360 sound data (SOUND\STREAMS\STREAMSREGISTRY.BUNDLE +
//  STREAMHEADERS.bundle, big-endian zlib'd bnd2 containers).
//
//  THE CONSOLE CHAIN (X360 ARTIST, asm-verified):
//    - BrnSound::Logic::Streaming::StreamingStateManager::Prepare @0x826EE680
//      loads "sound\streams\StreamHeaders.bundle" (+ language variants);
//    - RootSoundModule::RegistryLoad merges StreamsRegistry ContentSpecs into
//      the playback Registry (Module::AddRegistry @0x826C7BE8);
//    - a stream request resolves Registry::GetEntity<ContentSpec> by interned
//      NAME (CgsSound::Playback::Name::MakeHash), then ContentSpec::GetPathZone
//      (spec, 0) @0x826928C8 yields the gamedb URL (the spec path up to '|');
//    - ResourceRegistrar::GetResource @0x826B0B08 keys the StreamHeaders bundle
//      by CgsResource::ID::HashString(url) (= zlib crc32 of the LOWERCASED
//      string, table @dword_820F71F0) -> the GenericRwacWaveContent (SNR):
//      channels / sample rate / total samples / the PREFETCHED attack;
//    - path zone 1 (after '|') is the .SNS stream file under "SOUND\STREAMS\".
//
//  This PC realisation loads both bundles once and answers the two lookups the
//  bring-up needs. FLAG (bring-up parse): the registry payload is scanned for
//  its wave ContentSpec records ({nameHash, typeHash 0x511A448B, classRef,
//  misc, path}) rather than through the full serialised CgsSound::Playback::
//  Registry reconstruction (Registry::Import/FixUp -- its own recon slice).
// ============================================================================

namespace CgsSystem
{
    class StreamHeadersPC
    {
    public:
        // Resolve by ContentSpec NAME (e.g. "Guns_And_Roses"): fills the raw SNR
        // (GenericRwacWaveContent) bytes + the zone-1 .SNS file name. The SNR
        // memory is owned by the resident table (valid for the process lifetime).
        static bool ResolveBySpecName(const char* lpacSpecName,
                                      const u8** lppSnr, u32* lpuSnrLen,
                                      char* lpacSnsFile, u32 luSnsCap);

        // Resolve by the .SNS FILE name (zone 1, case-insensitive, no directory)
        // -- the movie-audio path derives the stream from the video name.
        static bool ResolveBySnsName(const char* lpacSnsFile,
                                     const u8** lppSnr, u32* lpuSnrLen);
    };
}

#endif // CGS_SYSTEM_PC_STREAMHEADERSPC_H
