#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   EA::Jobs::EntryPoint::SetAffinity @ 0x82BC98B0
//   EA::Jobs::EntryPoint::SetName     @ 0x82BC9858
//
// SetAffinity stores the affinity mask at offset 20. SetName copies up to 15
// characters of the supplied name into the 16-byte inline name buffer and null-
// terminates it (a null source yields an empty string).

namespace EA
{
    namespace Jobs
    {
        class EntryPoint
        {
        public:
            void* SetAffinity(int liAffinity);
            char* SetName(const char* pName);
        };

        void* EntryPoint::SetAffinity(int liAffinity)
        {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(this) + 20) = liAffinity;
            return this;
        }

        char* EntryPoint::SetName(const char* pName)
        {
            char* lpDest = reinterpret_cast<char*>(this);
            if (pName)
            {
                int liLen = 0;
                for (; liLen < 16; ++liLen)
                {
                    if (!pName[liLen])
                        break;
                    lpDest[liLen] = pName[liLen];
                }
                if (liLen >= 16)
                    liLen = 15;
                lpDest[liLen] = 0;
            }
            else
            {
                lpDest[0] = 0;
            }
            return lpDest;
        }
    }
}
