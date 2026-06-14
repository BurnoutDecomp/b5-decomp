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

        private:
            char macName[16];
            u8   mPad16[4];
            int  miAffinity;
        };

        void* EntryPoint::SetAffinity(int liAffinity)
        {
            miAffinity = liAffinity;
            return this;
        }

        char* EntryPoint::SetName(const char* pName)
        {
            if (pName)
            {
                int liLen = 0;
                for (; liLen < 16; ++liLen)
                {
                    if (!pName[liLen])
                        break;
                    macName[liLen] = pName[liLen];
                }
                if (liLen >= 16)
                    liLen = 15;
                macName[liLen] = 0;
            }
            else
            {
                macName[0] = 0;
            }
            return macName;
        }
    }
}
