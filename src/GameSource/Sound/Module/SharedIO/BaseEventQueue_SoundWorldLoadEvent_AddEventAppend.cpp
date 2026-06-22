#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"            // BaseEventQueue<T>::AddEvent/Append (inline generic)
#include "GameSource/Sound/Module/SharedIO/BrnSoundRootSharedIO.h"      // BrnSound::Module::Io::SoundWorldLoadEvent (8-byte element)

// CgsModule::BaseEventQueue<BrnSound::Module::Io::SoundWorldLoadEvent>::AddEvent @ 0x822C9B88
// CgsModule::BaseEventQueue<BrnSound::Module::Io::SoundWorldLoadEvent>::Append   @ 0x823C4238
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEvent /
// ::Append bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit
// instantiations. The X360 element stride is 8 bytes (sizeof(SoundWorldLoadEvent):
// eLoadEvent meEvent(4) + u16 mu16Zone(2) + 2B pad to 4-byte enum alignment) -- matched exactly:
//   AddEvent (@0x822C9B88): asserts mpEvents!=NULL (CgsBaseEventQueue.h:312) and miLength <
//     miMaxLength (the "Reached Max length" message-buffer assert at :313, non-gating), then
//     appends UNCONDITIONALLY at `*mpEvents + 8*miLength` -- two dword stores (asm
//     `*v12=*a2; v12[1]=a2[1]` == meEvent @+0, mu16Zone(+pad) @+4) -- bumps miLength, returns true.
//   Append (@0x823C4238): asserts mpEvents!=NULL (:413), no-overflow (:414) and source owns a
//     buffer (:486), then XMemCpy's `8 * srcLen` bytes onto the tail (asm `slwi r,_,3` == 8*v for
//     both dst offset and byte count) and advances miLength, returns true.
template bool
CgsModule::BaseEventQueue<BrnSound::Module::Io::SoundWorldLoadEvent>::AddEvent(
    const BrnSound::Module::Io::SoundWorldLoadEvent&);

template bool
CgsModule::BaseEventQueue<BrnSound::Module::Io::SoundWorldLoadEvent>::Append(
    const CgsModule::BaseEventQueue<BrnSound::Module::Io::SoundWorldLoadEvent>&);
