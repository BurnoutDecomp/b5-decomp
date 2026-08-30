#ifndef CGS_SYSTEM_PC_STREAMHEADERSPC_H
#define CGS_SYSTEM_PC_STREAMHEADERSPC_H

#include "types.hpp"

// ============================================================================
//  CgsSystem::StreamHeadersPC -- the runtime stream-header resolution chain, on
//  the native-x64 StreamsRegistry graph plus the original-format
//  STREAMHEADERS.bundle (whose SNR/EAAC bodies remain big-endian by format).
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
//  bring-up needs.  It walks the same platform-4 native-x64 Registry image that
//  RegistryResourceType fixes and RootSoundModule merges; no byte-pattern scan
//  or second console-layout copy is involved.
// ============================================================================

namespace CgsSystem
{
    class StreamHeadersPC
    {
    public:
        // ⭐ 2026-08-16 (boot audit F-P5-11/F7). Load both bundles NOW, from the boot point
        // the console loads them: RootSoundModule::Prepare's REGISTRY_LOAD stage, i.e.
        // loading-screen stage 4, with the loading screen up. Before this the tables were
        // built lazily on the first lookup, which on the PC meant "when the first boot video
        // asks for its audio" -- several seconds and a whole flow transition after the
        // console has them, and inside the frame that wanted to start playing.
        // Idempotent; the two resolvers still call it, so a missed preload degrades to the
        // old lazy behaviour rather than to no audio.
        static void Preload();

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
