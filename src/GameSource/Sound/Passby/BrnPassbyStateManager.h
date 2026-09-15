#ifndef BRN_SOUND_LOGIC_PASSBY_PASSBY_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_PASSBY_PASSBY_STATE_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3, EntityId
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"   // CgsSound::Logic::Content (mSplicerBank, BY NAME)     // BrnSound::Logic::BrnStateManager (committed base)

// Cgs3dEffectControl is only referenced through a pointer (Passby::mp3dControl),
// so a forward declaration suffices. Its committed home is Brn3DEffectControl.h,
// which is NOT included here on purpose: that header pulls BrnEffectControl.h,
// which redefines CgsSound::Logic::ClassTypeInfo and BrnSound::Logic::
// IResourceRequester -- both already defined by BrnStateManager.h. Those two
// committed homes each carry their own minimal copy and document that they are
// "never included in the same TU"; pulling both would be an ODR clash. Pointer-
// only use keeps this TU's surface minimal and clash-free.
namespace CgsSound { namespace Logic { struct Cgs3dEffectControl; } }

// =============================================================================
// BrnSound::Logic::Passby::PassbyStateManager
//   GameSource/Sound/Passby/BrnPassbyStateManager.h (DWARF home) +
//   GameSource/Sound/Passby/BrnPassbyStateManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PassbyStateManager is the sound-logic state manager that collects the passby
// requests posted by the traffic / engine / environment controls each frame and
// turns them into 3D passby voices. It derives the committed BrnStateManager
// (CgsSound::Logic::StateManager primary base + IResourceRequester sub-object).
//
// This TU bodies ONE ledger function:
//   BrnSound::Logic::Passby::PassbyStateManager::PostPassby  @ 0x82683488
//     -- append one Passby record to the fixed maPostedPassbys[] ring, guarded by
//        muPostedPassbyCount, returning true on success / false when full.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 asm reaches muPostedPassbyCount
// at +0x220 and the array at +0xA0, and copies the Passby payload as 6x 8-byte
// words (48 bytes on the 4-byte-pointer ABI). On the host the pointer member of
// Passby is 8 bytes wide, so the byte size differs; PostPassby is therefore bodied
// as a by-NAME indexed struct assignment (maPostedPassbys[count] = rPassby), NOT a
// hard-coded 48-byte copy, and no absolute offsets are asserted across pointers.
// =============================================================================

// FLAG: ePassbyTypes is the AttribSys-generated enum (DWARF
// GameSource/AttribSys/Enums/ePassbyTypes.h). Its generated home is not yet
// reconstructed in-tree; the full value list is mirrored here so Passby::meType is
// a faithful, typed field. Replace with the generated header's include when it lands.
namespace AttribSys
{
namespace Enums
{
namespace ePassbyTypes
{
    enum ePassbyTypes
    {
        PassbyAzimuth       = 0,
        PassbyPitch         = 1,
        PassbyCutoff        = 2,
        TrafficSmall        = 3,
        TrafficMedium       = 4,
        TrafficLarge        = 5,
        LampPost            = 6,
        Tree                = 7,
        Bridge              = 8,
        Tunnel              = 9,
        Camera              = 10,
        Misc                = 11,
        Collision           = 12,
        Overpass            = 13,
        Warehouse           = 14,
        Alley               = 15,
        StaticMetal         = 16,
        LargeOverheadObject = 17,
        PassbyBoostOffset   = 18,
        MaxPassbyTypes      = 19,
    };

    const int KI_NUM_ENUMS = 20; // ePassbyTypes.h:35
    const int KI_MAX_VALUE = 19; // ePassbyTypes.h:36
}
}
}

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// BrnPassbyStateManager.h:45 (DWARF). Derives the committed BrnStateManager.
class PassbyStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // 2026-09-15: the NAME-only ContentPlaceholder is RETIRED. The DWARF type is
    // CgsSound::Logic::Content (not CgsSound::Playback::Content), which HAS had a
    // committed home since the TrafficStateManager TU attested its shape
    // (GameShared/GameClasses/Sound/Logic/CgsContent.h) and which the sibling
    // AIVehicleStateManager already embeds by value. mSplicerBank is now that real
    // type, so Prepare/ResourcesAreReady can construct and probe it.

    // BrnPassbyStateManager.h:79 (DWARF). One posted passby request.
    struct Passby
    {
        // ePassbyTypes.h:39 (DWARF) typedef.
        typedef AttribSys::Enums::ePassbyTypes::ePassbyTypes EePassbyTypes;

        // BrnPassbyStateManager.h:85 -- from a live 3D effect control.
        Passby( const CgsSound::Logic::Cgs3dEffectControl* lp3dControl,
                f32 lfRelativeVelocityMagnitude,
                EePassbyTypes leType,
                bool lbSuppressBoostBys,
                f32 lfVolumeModifier );

        // BrnPassbyStateManager.h:102 -- from a static world position.
        Passby( Vector3 lStaticPos,
                f32 lfRelativeVelocityMagnitude,
                EePassbyTypes leType,
                bool lbSuppressBoostBys,
                f32 lfVolumeModifier );

        Passby() {}

        Vector3                                  mStaticPos;                 // h:111
        const CgsSound::Logic::Cgs3dEffectControl* mp3dControl;             // h:112
        f32                                      mfRelativeVelocityMagnitude; // h:113
        EePassbyTypes                            meType;                     // h:114
        f32                                      mfVolumeModifier;           // h:115
        bool                                     mbSuppressBoostBys;         // h:116
    };

    // BrnPassbyStateManager.h:134 (DWARF).
    static const u32 KU_DYNAMIC_PROP_CACHE_SIZE = 32;

    // BrnPassbyStateManager.h:133 (DWARF). Recent dynamic-prop passbys, so the
    // same prop is not re-triggered every frame.
    struct DynamicPropByCache
    {
        // BrnPassbyStateManager.h:137 (DWARF).
        struct Item
        {
            bool     mbActive;     // h:138
            f32      mfTimeStamp;   // h:139
            EntityId mId;           // h:140

            Item();                 // h:141
        };

        void  Update( f32 lfCurrentTime );                    // h:146
        Item* Insert( f32 lfTimeStamp, const EntityId& lId ); // h:160
        Item* Find( const EntityId& lId );                    // h:180

        Item maItems[KU_DYNAMIC_PROP_CACHE_SIZE]; // h:142
    };

    // ---- RTTI hooks (declared for home completeness; bodied in this TU's .cpp). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    // STATIC: GetStaticTypeInfo returns the function-local static RTTI descriptor and
    // CreateObject is the factory the descriptor stores as a free-function pointer
    // (StateManager*(*)(u32)). The X360 CreateObject @ 0x82702030 never touches an
    // instance -- its int arg is the operator-new flavour selector, not `this` -- so it
    // is a static class function; it MUST be static for &CreateObject to be storable in
    // ClassTypeInfo<StateManager>::mpfnCreateObject (a pointer-to-member is not a
    // free-function pointer). (Grown from the committed non-static decls to match the
    // X360 ABI + the in-tree RTTI registration shape.)
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();          // h:47
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );

    PassbyStateManager();
    virtual ~PassbyStateManager();

    virtual bool Prepare();
    virtual bool Release();
    virtual void UpdateParams( f32 lfTimeStep );

    // h:67. The passby splicer bank. Prepare @0x826F9748 probes IsLoaded() on it
    // before advancing, and ResourcesAreReady @0x826D4BA8 constructs it from
    // "PassbyAsset" through the splicer factory.
    const CgsSound::Logic::Content& GetSplicerBank() const { return mSplicerBank; }
    virtual void ResourcesAreReady();

    // 2026-09-15: the leaf override of GetResourceRegistrar() is RETIRED. The premise
    // it was written on ("BrnStateManager declares but does NOT body it") is false --
    // BrnStateManager::GetResourceRegistrar @0x82696510 IS bodied
    // (BrnStateManager.cpp:130, forwarding to mpLogicModule's embedded registrar), and
    // the console has NO PassbyStateManager override at all. The leaf stub that was
    // here asserted false and returned a TU-LOCAL EMPTY registrar, so every LoadAsset
    // this manager issued would have been enqueued into a registrar nobody pumps.

    // BrnPassbyStateManager.h:206 (DWARF: `bool PostPassby(const Passby&)`).
    // @ 0x82683488 -- append rPassby to maPostedPassbys[muPostedPassbyCount] when
    // there is still room, then advance the count. The X360 guard is
    // `(count + 1) >= KU_MAX_PASSBY_POSTS` (so the final slot is intentionally left
    // unused), returning false when full and true after a successful post.
    bool PostPassby( const Passby& lrPassby )
    {
        if( ( muPostedPassbyCount + 1 ) >= KU_MAX_PASSBY_POSTS )
        {
            return false;
        }

        maPostedPassbys[muPostedPassbyCount] = lrPassby;
        ++muPostedPassbyCount;
        return true;
    }

protected:
    void UpdateDynamicPropBys( f32 lfTimeStep );

    // BrnPassbyStateManager.h:130 (DWARF).
    static const u32 KU_MAX_PASSBY_POSTS = 8;

    Passby             maPostedPassbys[KU_MAX_PASSBY_POSTS]; // h:194
    u32                muPostedPassbyCount;                  // h:195
    CgsSound::Logic::Content mSplicerBank;                   // h:196
    DynamicPropByCache mDynamicPropCache;                    // h:197
};

// BrnPassbyStateManager.h:30/31 (DWARF) -- forward-declared siblings used by the
// .cpp (declared here for home completeness; reconstructed in their own homes).
struct Passby3DControl;
struct PassbyState;

} // namespace Passby
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_PASSBY_PASSBY_STATE_MANAGER_H
