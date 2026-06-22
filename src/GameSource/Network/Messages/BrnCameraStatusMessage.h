#pragma once

// ===================================================================================
// BrnNetwork::CameraStatusMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCameraStatusMessage.h
//
// A CgsNetwork::Message subclass advertising the local camera-feed status. Class
// shape is the DecFIGS DWARF ground truth for BrnCameraStatusMessage.h, cross-checked
// against the DecFIGS DWARF (BrnCameraStatusMessage.h:44) and gated on the X360 binary.
// The base CgsNetwork::Message is the committed home in CgsMessage.h and is reused
// BY NAME here -- not forked. The single leaf member meCameraStatus is declared after
// the Message base (positionally, by name; GetName never touches it, so its absolute
// offset is not pinned -- the X360 32-bit vs host 64-bit base widths differ).
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::CameraStatusMessage::GetName  @ 0x827DE0B8
//     -> returns the literal "Camera Status Message" (lis/addi a rodata string, blr).
//        No member or base access.
//
// The remaining declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/PackOrUnpack) live in the sibling .cpp TU and are declared here
// for the class shape but NOT bodied in this TU.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"  // CgsNetwork::Message (committed base)

namespace BrnNetwork
{
    // DWARF BrnCameraStatusMessage.h:48 -- the advertised camera-feed status.
    enum ECameraStatus
    {
        E_CAMERA_STATUS_NONE      = 0,
        E_CAMERA_STATUS_AVAILABLE = 1,
        E_CAMERA_STATUS_IN_USE    = 2,
        E_CAMERA_STATUS_COUNT     = 3,
    };

    struct CameraStatusMessage : public CgsNetwork::Message
    {
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          Destruct();
        void          PrepareForSend(u16 lu16CurrentFrame, ECameraStatus leCameraStatus);
        bool          Retrieve(ECameraStatus* lpeCameraStatus);
        virtual s32   GetPackedMessageSize();   // DWARF :149 (sibling .cpp)

        // LEDGER func @ 0x827DE0B8 -- bodied in this TU (DWARF BrnCameraStatusMessage.h:90).
        virtual const char* GetName() const;

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        ECameraStatus meCameraStatus;           // DWARF BrnCameraStatusMessage.h:80 (after the Message base, by name)
    };

    // BrnNetwork::CameraStatusMessage::GetName  @ 0x827DE0B8
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* CameraStatusMessage::GetName() const
    {
        return "Camera Status Message";
    }
} // namespace BrnNetwork
