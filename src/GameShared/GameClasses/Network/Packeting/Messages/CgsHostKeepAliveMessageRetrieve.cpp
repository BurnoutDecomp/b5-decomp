#include "GameShared/GameClasses/Network/Packeting/Messages/CgsHostKeepAliveMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::HostKeepAliveMessage::Retrieve @ 0x82880368
//   (called by CgsNetwork::HostMigrationManager::Update)
//
// Retrieve consumes the message's validity flag exactly once: it reads the flag byte at
// +0x19 (the inherited Message::mx8Flags), tests the VALID bit (bit 0), and -- when set --
// clears it, returning true; otherwise returns false without touching anything.
//
// asm (store-for-store):
//   lbz   r11, 0x19(r3)        ; flags = mx8Flags
//   clrlwi r10, r11, 31        ; flags & 1   (KX8_FLAGS_VALID)
//   beq   ... -> return 0      ; not valid -> nothing to retrieve
//   rlwinm r11, r11, 0,24,30   ; flags &= 0xFE  (clear VALID bit, keep the rest)
//   stb   r11, 0x19(r3)        ; write the flags back
//   ; CGS_ASSERT(!IsMessageValid()) -- VALID bit is now clear, so this never fires
//   return 1
//
// IsMessageValid() is the (mx8Flags & KX8_FLAGS_VALID) test; the post-clear assert is the
// asm's "we definitely cleared it" guard. The flag byte and VALID bit are accessed by name
// through the committed CgsNetwork::Message base.

namespace CgsNetwork
{

// ARTIST 0x82880380 branches to a trailing `return false` (0x828803CC): the consume is the body of
// the valid test. The assert names the header (CgsHostKeepAliveMessage.h:128): the original
// defined Retrieve inline there.
bool HostKeepAliveMessage::Retrieve()
{
    if (IsMessageValid())
    {
        SetMessageInvalid();
        CGS_ASSERT_AT(!IsMessageValid(), "!IsMessageValid()",
                      "..\\..\\..\\GameShared\\GameClasses\\Network/Packeting/Messages/CgsHostKeepAliveMessage.h",
                      128);
        return true;
    }

    return false;
}

}
