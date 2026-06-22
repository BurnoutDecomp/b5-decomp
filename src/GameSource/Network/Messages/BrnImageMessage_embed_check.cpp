// Compile-only embed check for BrnNetwork::ImageMessage
// (TU: GameSource/Network/Messages/BrnImageMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DFD20. Not linked into the game.
#include "GameSource/Network/Messages/BrnImageMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::ImageMessage;

    // The message derives from the committed CgsNetwork::ReliableMessage base (by name).
    static_assert(std::is_base_of<CgsNetwork::ReliableMessage, ImageMessage>::value,
                  "ImageMessage : CgsNetwork::ReliableMessage");

    // Segmentation bounds (DWARF BrnImageMessage.h:92-93).
    static_assert(ImageMessage::KI_MAX_PHOTO_PACKETS == 50, "KI_MAX_PHOTO_PACKETS == 50");
    static_assert(ImageMessage::KI_PHOTO_SEGMENT_SIZE == 500, "KI_PHOTO_SEGMENT_SIZE == 500");

    bool CheckGetName()
    {
        ImageMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Image Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }

    void TouchType()
    {
        BrnGameState::GameStateModuleIO::EImageType leType =
            BrnGameState::GameStateModuleIO::E_IMAGE_TYPE_MUGSHOT;
        (void)leType;
        (void)CheckGetName();
    }
}
