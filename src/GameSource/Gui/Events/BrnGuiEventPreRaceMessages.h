#pragma once
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstddef>
#include <cstring>

namespace BrnGui
{
// ARTIST AddMessage823A6D40/AddStringToMessage823A7090 and AddGuiEvent
// specialization pin a header-free1744-byte payload. DecFIGS supplies names;
// ARTIST reordered MessageInfo and shortened each parameter from20 to16bytes.
struct GuiEventPreRaceMessages
{
    enum ERelationshipTypes
    {
        E_RELATIONSHIP_GOOD=0, E_RELATIONSHIP_BAD=1,
        E_RELATIONSHIP_NEUTRAL=2, E_RELATIONSHIP_COUNT=3
    };
    static const s32 KI_MAX_NUM_MESSAGES=3;
    struct MessageInfo
    {
        static const s32 KI_NUM_MESSAGE_STRINGS=3;
        static const s32 KI_MAX_LENGTH_STRING_ID=128;
        static const s32 KI_NUM_PARAMETERS=1;
        static const s32 KI_MAX_LENGTH_PARAMETERS=16;
        s32 miNumMsgIDs;                        // ARTIST+000
        s32 maiNumParams[3];                    // +004
        ERelationshipTypes meRelationshipType; // +010
        char maacMessageIDs[3][128];            // +014
        char maacMessageParameters[3][16];      // +194
        char macGamerName[128];                 // +1C4
    };
    MessageInfo mMessages[3];
    s32 miNumMessages;                          // +6CC
    s32 GetEventType() const {return 159;}
    void Construct() {miNumMessages=0;}
    void AddMessage(ERelationshipTypes,const char*);
    void AddStringToMessage(s32,s32,const char*,s32,const char*);
};
using PreEventInfo=GuiEventPreRaceMessages::MessageInfo;
static_assert(sizeof(PreEventInfo)==580,"ARTIST pre-event row stride244");
static_assert(offsetof(PreEventInfo,meRelationshipType)==16,"ARTIST relationship+10");
static_assert(offsetof(PreEventInfo,maacMessageIDs)==20,"ARTIST message IDs+14");
static_assert(offsetof(PreEventInfo,maacMessageParameters)==404,"ARTIST parameters+194");
static_assert(offsetof(PreEventInfo,macGamerName)==452,"ARTIST gamer name+1C4");
static_assert(sizeof(GuiEventPreRaceMessages)==1744,"GUI159 queues1744payload bytes");
static_assert(offsetof(GuiEventPreRaceMessages,miNumMessages)==1740,"ARTIST count+6CC");

// ARTIST823A6D40: reset exactly the selected row, copy gamer name, then count.
inline void GuiEventPreRaceMessages::AddMessage(ERelationshipTypes leType,
                                               const char* lpcGamerName)
{
    CGS_ASSERT(lpcGamerName!=nullptr,"Invalid string passed to GuiEventPreRaceMessages::AddMessage");
    CGS_ASSERT(leType<E_RELATIONSHIP_COUNT,"Invalid message type passed to GuiPreraceMessage::AddMessage");
    CGS_ASSERT(miNumMessages<KI_MAX_NUM_MESSAGES,"Too many messages added to event in GuiPreraceMessage::AddMessage");
    CGS_ASSERT(std::strlen(lpcGamerName)<128,"Overlong gamer name passed to GuiEventPreRaceMessages::AddMessage");
    MessageInfo& lrMessage=mMessages[miNumMessages];
    lrMessage.miNumMsgIDs=0;
    std::memset(lrMessage.maacMessageIDs,0,sizeof(lrMessage.maacMessageIDs));
    for(s32 li=0;li<3;++li) lrMessage.maiNumParams[li]=0;
    std::memset(lrMessage.maacMessageParameters,0,sizeof(lrMessage.maacMessageParameters));
    lrMessage.meRelationshipType=leType;
    CGS_ASSERT(std::strlen(lpcGamerName)<128,"String too long: ");
    std::strncpy(lrMessage.macGamerName,lpcGamerName,128);
    ++miNumMessages;
}

// ARTIST823A7090: the original message-index bound permits index==count.
// Counters increment after each copy; a no-parameter string leaves its count0.
inline void GuiEventPreRaceMessages::AddStringToMessage(s32 liMessageIndex,
    s32 liStringIndex,const char* lpcStringId,s32 liNumParameters,const char* lpcParameter)
{
    CGS_ASSERT(lpcStringId!=nullptr,"Invalid string passed to GuiEventPreRaceMessages::AddStringToMessage");
    CGS_ASSERT(liMessageIndex>=0&&liMessageIndex<=miNumMessages,"Invalid message index passed to GuiPreraceMessage::AddMessage");
    CGS_ASSERT(liStringIndex>=0&&liStringIndex<3,"Invalid string index in GuiPreraceMessage::AddMessage");
    CGS_ASSERT(std::strlen(lpcStringId)<128,"Message stringID too long in GuiPreraceMessage::AddMessage");
    CGS_ASSERT(std::strlen(lpcStringId)<128,"String too long: ");
    MessageInfo& lrMessage=mMessages[liMessageIndex];
    std::strncpy(lrMessage.maacMessageIDs[liStringIndex],lpcStringId,128);
    ++lrMessage.miNumMsgIDs;
    CGS_ASSERT(lrMessage.miNumMsgIDs<=3,"Too many msg stringIDs in GuiPreraceMessage::AddMessage");
    CGS_ASSERT(liNumParameters>=0&&liNumParameters<=1,"Invalid number of parameters passed to GuiPreraceMessage::AddMessage");
    if(liNumParameters==1)
    {
        CGS_ASSERT(lpcParameter!=nullptr,"Invalid string passed to GuiEventPreRaceMessages::AddStringToMessage");
        CGS_ASSERT(std::strlen(lpcParameter)<16,"Parameter string too long in GuiPreraceMessage::AddMessage");
        CGS_ASSERT(std::strlen(lpcParameter)<16,"String too long: ");
        std::strncpy(lrMessage.maacMessageParameters[liStringIndex],lpcParameter,16);
        ++lrMessage.maiNumParams[liStringIndex];
        CGS_ASSERT(lrMessage.maiNumParams[liStringIndex]<=1,"Too many parameters for message in GuiPreraceMessage::AddMessage");
    }
}
}
