#include "SharedClasses/Graphics/TextureNameMapResourceType.h"

#include <cctype>   // std::tolower (X360 calls libc tolower @ each byte)

// BrnParticle::TextureNameMap::Entry::HashString  @ 0x82277CD0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Case-insensitive FNV-1a over the string's
// characters: it first walks to the NUL to measure the length, then folds every real
// character (excluding the terminator) — each lowercased via tolower — into the running
// hash with the FNV offset basis 0x811C9DC5 and prime 0x01000193. Empty string returns
// the bare offset basis (the fold loop is skipped). Callers (AcquireTexture /
// TextureRegister / LoadFXBundle) precompute Lion texture-name hashes with this.
namespace BrnParticle
{
    u32 TextureNameMap::Entry::HashString( const char* lpcName )
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
