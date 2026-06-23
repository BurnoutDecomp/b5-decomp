#include "GameSource/Effects/Particles/BrnParticleDescription.h"

#include <cctype>   // std::tolower (X360 calls libc tolower @ each byte)

// Out-of-line body for BrnParticle::ParticleDescription::HashString.
//
// Reconstructed store-for-store from the X360 ARTIST asm at 0x82276FF8. Case-insensitive
// FNV-1a over the string's characters: walk to the NUL to measure the length, then fold
// every real character (excluding the terminator) -- each lowercased via tolower -- into
// the running hash with the FNV offset basis 0x811C9DC5 and prime 0x01000193. An empty
// string returns the bare offset basis (the `if ( v2 - a1 != 1 )` guard skips the fold).
// Identical to BrnParticle::TextureNameMap::Entry::HashString (sibling Lion name key).

namespace BrnParticle
{
    u32 ParticleDescription::HashString( const char* lpcName )
    {
        // Measure length (walk to and past the NUL, as the X360 does), then fold the
        // real characters [0 .. liLength-1].
        const char* lpcCursor = lpcName;
        while ( *lpcCursor++ )
            ;
        u32 luLength = (u32)( lpcCursor - lpcName - 1 );

        u32 luHash = (u32)0x811C9DC5;   // -2128831035 (FNV-1a 32-bit offset basis)
        if ( luLength != 0 )
        {
            u32 luIndex = 0;
            do
            {
                u8 luChar = (u8)std::tolower( (unsigned char)lpcName[luIndex] );
                ++luIndex;
                luHash = 0x01000193 * ( luChar ^ luHash );
            }
            while ( luIndex < luLength );
        }

        return luHash;
    }
}
