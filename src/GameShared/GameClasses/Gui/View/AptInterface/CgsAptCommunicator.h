#ifndef CGS_APT_COMMUNICATOR_H
#define CGS_APT_COMMUNICATOR_H

#include "types.hpp"

#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"               // CgsGui::GuiEvent<N> (event-record bases)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<18432,16>
#include "SDKs/EATech/include/Apt/AptExtObject.h"                 // AptExtObject (base), AptValue
#include "SDKs/EATech/include/Apt/AptString/EAString.h"           // EAStringC (AptNativeString)

// ============================================================================
// GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h
//
// CgsGui::AptCommunicator - the ActionScript <-> game communication bridge the APT
// (Scaleform-style) GUI exposes as a global apt extension object ("gAptCommunicator").
// It is an AptExtObject whose native methods (sMethod_*) Flash calls to push events,
// sound events and component data across, and whose Update*Component* helpers the
// game calls to mirror component state back into the Flash movie.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. SHAPE (members, names, the two helper
// event records, the AptEventType enum, the static-data set) is taken from the
// DecFIGS DWARF (references/DecFIGS/dwarfdump/.../CgsAptCommunicator.h, where the
// class is declared `struct CgsGui::AptCommunicator : public AptExtObject`). The
// X360 stores every data member at a fixed global address (file-static class data):
//   mAptComponentList            @ dword_830395A0  (the AptComponentList instance)
//   miNumUsedKeyValues           @ dword_8305A6D0
//   muNumActivecomponents        @ dword_8305A6D4
//   miMaxNumUsedKeyValues        @ dword_8305A6D8
//   maKeyValuePool[256]          @ byte_830522A0   (stride sizeof(KeyValue)==132)
//   mauReservedVariablesHashes[9]@ dword_8305A6A0..dword_8305A6C4
//   mpAptInternalCommunicator    @ dword_8305A6CC
//   mbCircleButtonAsSelect       @ byte_8305A6DC
//   mOutAptTriggerEvents         @ unk_8305A960    (VariableEventQueue<18432,16>)
// so the members are modelled as static; the index math the asm performs reproduces
// exactly at each member's natural array index (no raw offsets used).
//
// KeyValue is defined HERE (its DWARF home) and the sibling CgsAptComponentList.h
// includes this header back for it; AptComponentList is therefore only forward-
// declared here (it is a static data member -- a declaration needs no complete type)
// and the .cpp pulls in CgsAptComponentList.h for the full type. That one-directional
// dependency breaks the otherwise-circular include.
//
// EA SDK identifiers (AptExtObject, AptValue, EAStringC, AptNativeFunction, ...) are
// kept verbatim per CXX_NAMING_CONVENTIONS (external/middleware API exception).
// ============================================================================

// The apt native-method descriptor (homed: SDKs/EATech/include/Apt/AptNativeFunction.h);
// the six psMethod_* slots hold it by pointer only, so a forward declaration suffices here.
class AptNativeFunction;

namespace CgsGui
{
    // The per-component parallel-array store (full definition in CgsAptComponentList.h,
    // which includes this header for KeyValue below). Forward-declared: AptCommunicator
    // only holds a static instance of it, so the class definition needs no complete type.
    class AptComponentList;

    // ------------------------------------------------------------------------
    // KeyValue (DWARF CgsAptCommunicator.h:95) - one stored key/value pair. The APT
    // GUI communicator keeps a pool of these and hands AptComponentList a KeyValue*
    // for each (component, key-slot) cell. mHashedKey is the hash of the key string;
    // macValue is the fixed-width value text. Defined before any include that needs it.
    // ------------------------------------------------------------------------
    struct KeyValue
    {
        // CgsAptCommunicator.h:96 (DWARF) - value text capacity.
        static const s32 KI_MAX_VALUE_LENGTH = 128;

        u32  mHashedKey;                    // CgsAptCommunicator.h:97 (DWARF)
        char macValue[KI_MAX_VALUE_LENGTH]; // CgsAptCommunicator.h:98 (DWARF)
    };

    // ------------------------------------------------------------------------
    // GuiEventAptTrigger - the 20-byte event record SendAptEvent pushes onto the
    // out queue (DWARF CgsAptCommunicator.h:49, `: public GuiEvent<21>`).
    //
    // X360-grounded: in the X360 build the record actually queued is the 5-field, 20-byte
    // PAYLOAD only (AddEvent is called with liSize==20 and a pointer to a bare 5-word
    // struct of exactly these fields). The DWARF `: public GuiEvent<21>` base carries
    // no extra stored words in front of the payload in this build, so the queued
    // struct == these five members. Modelled as a GuiEvent<21> (so GetEventType()==21
    // and it lives in the CgsGui event vocabulary); the constructor fills only the
    // five payload fields, matching the X360 by-value store.
    // ------------------------------------------------------------------------
    struct GuiEventAptTrigger : public GuiEvent<21>
    {
        // CgsAptCommunicator.h:54 (DWARF) - the apt event kind SendAptEvent forwards.
        enum AptEventType
        {
            E_APT_EVENT_START               = 0,
            E_APT_EVENT_ONLOAD              = 1,
            E_APT_EVENT_POSTFX              = 2,
            E_APT_EVENT_FRAME_TRIGGER       = 3,
            E_APT_EVENT_TRANSITION_COMPLETE = 4,
            E_APT_EVENT_NUM                 = 5
        };

        AptEventType meEventType;         // CgsAptCommunicator.h:66 (DWARF)
        s32          miUniqueId;          // CgsAptCommunicator.h:67 (DWARF)
        const char*  mpacComponentName;   // CgsAptCommunicator.h:68 (DWARF)
        u32          muComponentNameHash; // CgsAptCommunicator.h:69 (DWARF)
        AptValue*    mpComponentRef;      // CgsAptCommunicator.h:70 (DWARF)
    };

    // Native-width queue payload for event 21. ARTIST queues the five fields above
    // as a bare 20-byte PPC record (two words, two 32-bit pointers and one word).
    // The PC queue must preserve the same fields under the x64 ABI, where the two
    // pointers widen. Keeping the payload separate from GuiEvent<21>'s 12-byte
    // wrapper also preserves ARTIST's "payload only" wire shape.
    struct GuiEventAptTriggerPayload
    {
        GuiEventAptTrigger::AptEventType meEventType;
        s32                              miUniqueId;
        const char*                      mpacComponentName;
        u32                              muComponentNameHash;
        AptValue*                        mpComponentRef;
    };

    // ------------------------------------------------------------------------
    // GuiEventSoundTrigger - the 100-byte record SendAptSoundEvent pushes (DWARF
    // CgsAptCommunicator.h:83, `: public GuiEvent<22>`). The X360 queues exactly the
    // 100-byte payload (AddEvent liSize==100): three 32-byte fixed strings + a layer.
    // Same GuiEvent<22> base note as GuiEventAptTrigger above.
    // ------------------------------------------------------------------------
    struct GuiEventSoundTrigger : public GuiEvent<22>
    {
        // CgsAptCommunicator.h:85 (DWARF) - fixed-width string capacity.
        static const s32 KI_EVENT_NAME_LENGTH = 32;

        char macTypeName[KI_EVENT_NAME_LENGTH];   // CgsAptCommunicator.h:87 (DWARF)
        char macActionName[KI_EVENT_NAME_LENGTH]; // CgsAptCommunicator.h:89 (DWARF)
        char macLabel[KI_EVENT_NAME_LENGTH];      // CgsAptCommunicator.h:90 (DWARF)
        s32  miLayer;                             // CgsAptCommunicator.h:91 (DWARF)
    };

    // ------------------------------------------------------------------------
    // AptCommunicator - the apt extension bridge (DWARF CgsAptCommunicator.h:303).
    // ------------------------------------------------------------------------
    class AptCommunicator : public AptExtObject
    {
    public:
        // CgsAptCommunicator.h:393 (DWARF) - the key/value pool capacity (== KU_MAX_COMPONENTS).
        static const s32 KI_MAX_KEYVALUES = 256;
        // CgsAptCommunicator.h:400 (DWARF) - count of registered native methods.
        static const s32 KI_NUM_FUNCS = 6;
        // The reserved-variable table length (the UpdateComponentReserved switch is 9-wide).
        static const s32 KI_NUM_RESERVED_VARIABLES = 9;

        // The X360 builds the communicator inline in AptAux::InitializeApt @0x82848E50:
        // AptExtObject::operator new(16); AptExtObject(6) (== the base ctor with the
        // 6-native-member reserve); *p = off_820E0A20 (THIS class's vtable). The derived
        // ctor is therefore exactly the base ctor + vtable install.
        AptCommunicator() : AptExtObject(KI_NUM_FUNCS) {}

        // X360 0x8285F0D8 (AptExtObject::Initialize override; AptRegisterExtension
        // @0x82AF7330 virtual-calls it right after GetName). Wrap each of the 6
        // sMethod_* natives in an AptNativeFunction (cached in the psMethod_* slots)
        // and SetFunction it onto the member hash under its ActionScript name; then
        // Construct the out queue and precompute the reserved-variable hashes.
        virtual void Initialize();

        // ---- the apt native methods (static; the apt VM calls them with the bound
        // context + param count and they read their args via AptExtObject::GetParam).
        // DWARF gives the native-ABI signature (AptValue* pContext, int iNumParams).

        // X360 0x8285C360. Forward an apt event (start/onload/postfx/frame/transition)
        // to the out queue; for E_APT_EVENT_ONLOAD also register the new component.
        static AptValue* sMethod_SendAptEvent(AptValue* pContext, int iNumParams);

        // X360 0x8285C9B8. Forward a sound trigger (type/action/label/layer) to the queue.
        static AptValue* sMethod_SendAptSoundEvent(AptValue* pContext, int iNumParams);

        // X360 0x82849870. Bind the apt-side communicator object (an apt Object value).
        static AptValue* sMethod_SetCommunicationObject(AptValue* pContext, int iNumParams);

        // X360 0x82850618. Look a stored key/value up for a component movieclip.
        static AptValue* sMethod_GetComponentData(AptValue* pContext, int iNumParams);

        // X360 0x82849950. Return the platform string ("xbox").
        static AptValue* sMethod_GetPlatformString(AptValue* pContext, int iNumParams);

        // X360 0x82849960. Return whether the circle button acts as "select".
        static AptValue* sMethod_GetCircleButtonAsSelect(AptValue* pContext, int iNumParams);

        // ---- AptExtObject overrides ----
        // X360 0x82849860. The extension's display name ("CAptCommunicator").
        virtual const char* GetName();

        // ---- game-side mirroring helpers ----
        // X360 0x82850958. Mirror one (key,value) onto the named component: find/update
        // an existing KeyValue or allocate a new one from the pool.
        void UpdateComponent(const char* lpacName, const char* lpacKey, const char* lpacValue);

        // X360 0x82851408. Mirror one reserved (typed) variable onto the named component.
        void UpdateComponentReserved(const char* lpacName, const char* lpacKey, const char* lpacValue);

        // X360 0x828499D0. Flush all dirty components to Flash ("UpdateAll") then reset
        // the per-frame dirty flags and the key/value pool.
        void UpdateAllComponents();

        // X360 0x824EAD38. Debug helper: linear-scan the registered components' hashed
        // names for luHash and return that component's stored name text ("Unknown" on a
        // miss). Reads only the static mAptComponentList (the X360 scans mauHashedName
        // @dword_830419A0 and returns maacName @unk_830422A0 + 256*index). Public because
        // BrnGui::GuiCacheDebugComponent::ShowAptComponentGuiCacheStatus calls it to label
        // a GUI-cache dump entry.
        const char* GetComponentNameForHash(u32 luHash);

        // X360 0x82851818. Remove the component whose bound AptValue is being unloaded:
        // find the matching component reference, move the final active slot into its place,
        // then shrink the active count.
        static void RemoveExpiredAptComponent(AptValue* lpAptValue);

        // The per-frame trigger publish the console CgsGui::GuiModule::Update @0x828602C8
        // inlines at its tail: bulk-append mOutAptTriggerEvents (the SendAptEvent 21 /
        // SendAptSoundEvent 22 records) into the view OUTPUT buffer's GUI event queue,
        // then Clear the communicator queue. Exposed as a member so the queue stays
        // private (the X360 reaches it as file-scope data, unk_8305A960).
        static void FlushTriggerEventsTo(CgsModule::VariableEventQueue<18432, 16>* lpDest);

    private:
        // X360 0x82849F48. Hash lpacName and return its registered component index, or -1.
        // Static: it only touches the file-static class data (no `this`), and the static
        // sMethod_* / instance Update* helpers both call it.
        static s32 FindAptComponent(const char* lpacName);

        // X360 sub_8284A0B8 (DWARF cpp:808): the movieclip-ref overload -- linear-scan
        // the registered components' bound references for pointer equality; -1 on miss.
        // Static: it only touches the file-static class data (the native methods call it).
        static s32  FindAptComponent(AptValue* lpMovieClip);
        // X360 0x82849B88 (DWARF cpp:721): register a new component for an ONLOAD event
        // (sMethod_SendAptEvent calls it): seed the next slot's hashes/name/reference,
        // duplicate-check against the existing hashed names (debug log), bump the count.
        static void AddNewAptComponent(AptValue* lpMovieClip, const char* lpacName);

        // X360 0x8284A1F8 (Initialize tail-calls it): precompute the reserved-variable
        // name hashes into mauReservedVariablesHashes.
        static void CalculateReservedVariableHashes();

        // ===== static data (file-static class members; fixed X360 globals) =====

        // @ dword_830395A0 - the per-component parallel-array store.
        static AptComponentList mAptComponentList;

        // @ dword_8305A6D0 - count of KeyValue records currently taken from the pool.
        static s32 miNumUsedKeyValues;
        // @ dword_8305A6D4 - count of registered/active components (the loop bound).
        static u32 muNumActivecomponents;
        // @ dword_8305A6D8 - high-water mark of miNumUsedKeyValues.
        static s32 miMaxNumUsedKeyValues;

        // @ byte_830522A0 - the KeyValue backing pool UpdateComponent hands out.
        static KeyValue maKeyValuePool[KI_MAX_KEYVALUES];

        // @ dword_8305A6CC - the apt-side communicator object SetCommunicationObject binds.
        static AptValue* mpAptInternalCommunicator;

        // @ byte_8305A6DC - "circle == select" controller flag.
        static bool mbCircleButtonAsSelect;

        // @ unk_8305A960 - the GUI-trigger out queue SendApt(Sound)Event push to.
        static CgsModule::VariableEventQueue<18432, 16> mOutAptTriggerEvents;

        // @ mpacReservedVariableNames / @ dword_8305A6A0 - the reserved-variable name
        // table and its precomputed hashes (filled by CalculateReservedVariableHashes).
        // FLAG: the literal name strings live in un-exported X360 rodata (the table
        // @0x8284A1F8 reads); until they are recovered the table entries are null and
        // the hashes zero -- UpdateComponentReserved then correctly treats every key as
        // "not reserved" (off the title-menu path).
        static const char* mpacReservedVariableNames[KI_NUM_RESERVED_VARIABLES];
        static u32         mauReservedVariablesHashes[KI_NUM_RESERVED_VARIABLES];

        // @ dword_8305A6E0..8305A6F4 - the six cached AptNativeFunction wrappers
        // Initialize creates for the sMethod_* natives (the DWARF psMethod_* slots).
        static AptValue* psMethod_SendAptEvent;
        static AptValue* psMethod_SendAptSoundEvent;
        static AptValue* psMethod_SetCommunicationObject;
        static AptValue* psMethod_GetComponentData;
        static AptValue* psMethod_GetPlatformString;
        static AptValue* psMethod_GetCircleButtonAsSelect;
    };
}

#endif // CGS_APT_COMMUNICATOR_H
