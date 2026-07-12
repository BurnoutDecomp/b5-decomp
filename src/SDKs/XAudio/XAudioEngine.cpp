#include "SDKs/XAudio/XAudioEngine.h"

// ===========================================================================
// XAUDIO::CEngine -- the XAudio engine singleton, homed in BURNOUT_X360_ARTIST.
// See XAudioEngine.h for the full 372-byte layout, the singleton, and the asm
// notes. No reference source and no DWARF exist for this TU; the PowerPC asm is
// authoritative.
//
// This wave HOMES the layout (the leverage the blocked CVoice / CRoutedVoice /
// CReverbEffect TUs need to read the engine by named member instead of raw
// offset) and DEFINES the singleton. All 18 CEngine methods are BLOCKED bodies:
// every one drives the Xbox kernel spinlock/IRQL/threading/semaphore primitives,
// the per-hardware-thread `r13` KPCR reads, the un-recovered module rodata/state
// globals (the shared spinlock `unk_83222C28` + `dword_83222C2C`/`dword_83222C30`
// / `byte_83222C34`, the module state `byte_832BB900..dword_832BB944`, the class
// vtable rodata `off_821B5514`/`off_821B5528`/`off_82108FF4`, `unk_82108F38`), or
// a still-todo foreign vtable slot (CVoice / mastering-voice / batch-allocator).
// Per the fidelity rules those bodies are deferred rather than guessed; they are
// declared in the header so the class interface and vptr are correct. See the
// per-method BLOCKED notes in XAudioEngine.h.
// ===========================================================================

namespace XAUDIO
{

// @ off_832BB944 -- the engine singleton. The ctor @0x82C2EA28 stores `this`
// here (`stw r31, off_832BB944@l(r11)`) and the dtor @0x82C2E2D0 clears it; the
// processing-thread entry points and the CRoutedVoice attach path read the
// default output / effect manager through it. Null until an engine is created.
CEngine* gpEngine = nullptr;

} // namespace XAUDIO
