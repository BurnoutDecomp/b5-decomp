// =====================================================================================
// rw::audio::core::SubMixConnector::Disconnect -- unlink an inbound connector from its
// SubMix and (optionally) fold its per-channel gains back into the SubMix.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82B9C3C0; the
// PowerPC asm is authoritative. No Feb-2007 leak source and no DecFIGS DWARF exist.
//
// Store-for-store from the asm (r3 = this, r4 = foldBackGains):
//   lwz r11,0xC(r3)            v2 = mpSubMix
//   cmplwi r11,0 / beqlr       if (!v2) return            // nothing to do
//   lwz  r10,0x28(r11)         v3 = v2->mpConnectorHead
//   cmplw r3,r10 / bne ...     if (this == head)
//   lwz  r10,0(r10)            head = this->mpNext
//   stw  r10,0x28(r11)
//   lwz  r11,4(r3)             v4 = mppPrev
//   cmpwi/ beq                 if (v4)
//   lwz  r10,0(r3)              *v4 = mpNext
//   stw  r10,0(r11)
//   lwz  r11,0(r3)             if (mpNext)
//   cmpwi/beq
//   lwz  r10,4(r3)              mpNext->mppPrev = this->mppPrev
//   stw  r10,4(r11)
//   li   r7,0                  (the zero used for the trailing clears)
//   cmplwi r4,0 / beq          if (foldBackGains)
//     stb 1,0x8D(mpSubMix)      mpSubMix->mbDirty = 1
//     lbz 0x21(mpSubMix)        n = mpSubMix->mbNumChannels
//     loop i in [0,n):          mpSubMix->mafChannelGain[i] += foldBackGains[i]
//   stw r7,0xC(r3)             mpSubMix  = 0
//   stw r7,8(r3)              mField08 = 0
//   stb r7,0x10(r3)            mbField10 = 0   (single byte)
//   return this
//
// NOTE: the asm re-reads mpSubMix (lwz 0xC) each loop iteration and re-reads
// mbNumChannels (lbz 0x21) at the loop bottom; the rolling form below preserves that
// the count is re-evaluated against the (unchanged) SubMix each pass.
// =====================================================================================

#include "rw/audio/core/SubMixConnector.h"

namespace rw
{
namespace audio
{
namespace core
{

SubMixConnector *SubMixConnector::Disconnect(SubMixConnector *self, const f32 *foldBackGains)
{
    SubMix *submix = self->mpSubMix; // v2 = *(this+0xC)
    if (submix)
    {
        // ---- unlink from the SubMix's doubly-linked connector list ----
        SubMixConnector *head = submix->mpConnectorHead; // v3 = *(v2+0x28)
        if (self == head)
            submix->mpConnectorHead = self->mpNext;      // head = this->mpNext

        SubMixConnector **prev = self->mppPrev;          // v4 = *(this+4)
        if (prev)
            *prev = self->mpNext;                        // *mppPrev = mpNext

        if (self->mpNext)                                // if (*this)
            self->mpNext->mppPrev = self->mppPrev;       // mpNext->mppPrev = this->mppPrev

        // ---- optional gain fold-back ----
        if (foldBackGains)
        {
            self->mpSubMix->mbDirty = 1;                 // *(v2+0x8D) = 1
            for (int i = 0; i < self->mpSubMix->mbNumChannels; ++i)
                self->mpSubMix->mafChannelGain[i] =
                    foldBackGains[i] + self->mpSubMix->mafChannelGain[i];
        }

        // ---- clear the owning-SubMix / cursor / flag fields ----
        self->mpSubMix = 0;           // +0x0C
        self->mpSubMixBuffer = 0;     // +0x08
        self->mNumSubMixChannels = 0; // +0x10 (stb)
    }
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
