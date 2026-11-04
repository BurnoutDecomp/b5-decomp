// DirtySDK 5.5.3 native rank state: DecFIGS names, ARTIST PPC32 offsets.
#include "lobbyrank.h"
#include <stddef.h>
#include <string.h>
struct LobbyLocalizeT;
struct LobbyRankVariationT { const char* pName; u32 uListIndex; s32 uParams[8]; u32 uParamCount; s32 iUserType; };
struct LobbyRankIndexT { const char* pName; LobbyRankVariationT* pVariations; u32 uVariationCount; };
struct LobbyRankCategoryT { const char* pName; LobbyRankIndexT* pIndices; u32 uIndexCount; };
struct LobbyRankColumnT { char strName[32]; char strDesc[64]; char strParam[32]; u32 uWidth; s32 iType; s32 iStyle; };
struct LobbyRankListT {
 LobbyRankT* pRanker; char strName[32]; LobbyRankColumnT* pColumns; DispListRef* pList; s32 iId;
 char strParams[64]; LobbyRankIndexT Index; u16 uColumnCount; u16 uExpectedRowCount;
 u16 uFixedColumnCount; u16 uCategoryIndex; u32 uBuddyHash; u16 uIndexIndex;
 u16 uVariationIndex; u16 uListIndex; s16 iOffset; s16 iRows; s16 iBuddyCount;
};
struct LobbyRankT {
 LobbyApiRefT* pLobby; LobbyLocalizeT* pLocalize; s32 iMemGroup; char strView[11];
 LobbyRankCallbackT* pCallback; void* pUserData; s32 eStatus; s32 eDownloadStatus;
 LobbyRankCategoryT* pCategories; LobbyRankListT* pLists; u32 uListCount; u32 uCategoryCount;
 u32 uCategoryIndex; u32 uIndexIndex; u32 uVariationIndex; u32 uListIndex; s32 iId;
 s32 iLastCallbackId; s32 iLastError; u16 uCurrentScrollColumn; u16 uCurrentScrollRow;
 char* pSymTab; s32 iSymCount;
};
typedef char RankCategorySize[(sizeof(LobbyRankCategoryT)==12)?1:-1];
typedef char RankIndexSize[(sizeof(LobbyRankIndexT)==12)?1:-1];
typedef char RankListSize[(sizeof(LobbyRankListT)==0x94)?1:-1];
typedef char RankColumnSize[(sizeof(LobbyRankColumnT)==0x8c)?1:-1];
typedef char RankStatusOffset[(offsetof(LobbyRankT,eStatus)==0x20)?1:-1];
typedef char RankListOffset[(offsetof(LobbyRankT,pLists)==0x2c)?1:-1];
typedef char RankColumnCountOffset[(offsetof(LobbyRankListT,uColumnCount)==0x7c)?1:-1];

extern "C" s32 LobbyRankCategoryCount(LobbyRankT* pRanker)
{
 return (pRanker->eStatus & 1) ? pRanker->uCategoryCount : -5;
}
extern "C" s32 LobbyRankIndexCount(LobbyRankT* pRanker, s32 iCategory)
{
 if (!(pRanker->eStatus & 1)) return -5;
 if (iCategory < 0 || (u32)iCategory >= pRanker->uCategoryCount) return -1;
 return pRanker->pCategories[iCategory].uIndexCount;
}
extern "C" s32 LobbyRankVariationCount(LobbyRankT* pRanker, s32 iCategory, s32 iIndex)
{
 if (!(pRanker->eStatus & 1)) return -5;
 if (iCategory < 0 || (u32)iCategory >= pRanker->uCategoryCount) return -1;
 const LobbyRankCategoryT& Category = pRanker->pCategories[iCategory];
 if (iIndex < 0 || (u32)iIndex >= Category.uIndexCount) return -1;
 return Category.pIndices[iIndex].uVariationCount;
}
extern "C" s32 LobbyRankColumnCount(LobbyRankT* pRanker)
{
 if (!(pRanker->eStatus & 2) || pRanker->eDownloadStatus == 2) return -6;
 return pRanker->pLists[pRanker->uListIndex % pRanker->uListCount].uColumnCount;
}
extern "C" s32 LobbyRankColumnWidth(LobbyRankT* pRanker, s32 iColumn)
{
 if (!(pRanker->eStatus & 2) || pRanker->eDownloadStatus == 2) return -6;
 const LobbyRankListT& List = pRanker->pLists[pRanker->uListIndex % pRanker->uListCount];
 if ((u32)iColumn >= List.uColumnCount) return -12;
 return List.pColumns[iColumn].uWidth;
}
extern "C" s32 LobbyRankColumnType(LobbyRankT* pRanker, s32 iColumn)
{
 if (!(pRanker->eStatus & 2) || pRanker->eDownloadStatus == 2) return 0;
 const LobbyRankListT& List = pRanker->pLists[pRanker->uListIndex % pRanker->uListCount];
 if ((u32)iColumn >= List.uColumnCount) return 0;
 return List.pColumns[iColumn].iType;
}

// ARTIST calls the platform bounded copy even when returning the internal string.
extern "C" char* ds_strnzcpy(char*, const char*, s32);
extern "C" const char* LobbyRankCategoryName(LobbyRankT* r, s32 c, char* out, s32 len)
{
 if (!(r->eStatus & 1)) return 0;
 if (c == -100) c = r->uCategoryIndex;
 else if (c < 0 || (u32)c >= r->uCategoryCount) return 0;
 const char* name = r->pCategories[c].pName;
 if (out) ds_strnzcpy(out, name, len);
 return name;
}
extern "C" const char* LobbyRankIndexName(LobbyRankT* r, s32 c, s32 i, char* out, s32 len)
{
 if (!(r->eStatus & 1)) return 0;
 if (c == -100) c = r->uCategoryIndex;
 else if (c < 0 || (u32)c >= r->uCategoryCount) return 0;
 const LobbyRankCategoryT& category = r->pCategories[c];
 if (i == -100) i = r->uIndexIndex;
 else if (i < 0 || (u32)i >= category.uIndexCount) return 0;
 const char* name = category.pIndices[i].pName;
 if (out) ds_strnzcpy(out, name, len);
 return name;
}
extern "C" const char* LobbyRankVariationName(LobbyRankT* r, s32 c, s32 i, s32 v, char* out, s32 len)
{
 if (!(r->eStatus & 1)) return 0;
 if (c == -100) c = r->uCategoryIndex;
 else if (c < 0 || (u32)c >= r->uCategoryCount) return 0;
 const LobbyRankCategoryT& category = r->pCategories[c];
 if (i == -100) i = r->uIndexIndex;
 else if (i < 0 || (u32)i >= category.uIndexCount) return 0;
 const LobbyRankIndexT& index = category.pIndices[i];
 if (v == -100) v = r->uVariationIndex;
 else if (v < 0 || (u32)v >= index.uVariationCount) return 0;
 const char* name = index.pVariations[v].pName;
 if (out) ds_strnzcpy(out, name, len);
 return name;
}
extern "C" s32 LobbyRankVariationUserType(LobbyRankT* r, s32 c, s32 i, s32 v)
{
 if (!(r->eStatus & 1)) return 0;
 if (c == -100) c = r->uCategoryIndex;
 else if (c < 0 || (u32)c >= r->uCategoryCount) return 0;
 const LobbyRankCategoryT& category = r->pCategories[c];
 if (i == -100) i = r->uIndexIndex;
 else if (i < 0 || (u32)i >= category.uIndexCount) return 0;
 const LobbyRankIndexT& index = category.pIndices[i];
 if (v == -100) v = r->uVariationIndex;
 else if (v < 0 || (u32)v >= index.uVariationCount) return 0;
 return index.pVariations[v].iUserType;
}
extern "C" s32 LobbyRankColumnStyle(LobbyRankT* r, s32 col)
{
 if (!(r->eStatus & 2) || r->eDownloadStatus == 2) return 0;
 const LobbyRankListT& list = r->pLists[r->uListIndex % r->uListCount];
 return (u32)col < list.uColumnCount ? list.pColumns[col].iStyle : 0;
}
extern "C" const char* LobbyRankColumnName(LobbyRankT* r, s32 col, char* out, s32 len)
{
 if (!(r->eStatus & 2)) return 0;
 const LobbyRankListT& list = r->pLists[r->uListIndex % r->uListCount];
 if ((u32)col >= list.uColumnCount) return 0;
 const char* name = list.pColumns[col].strName;
 if (out) ds_strnzcpy(out, name, len);
 return name;
}
extern "C" s32 LobbyRankParm(LobbyRankT* r, s32 index, s32* data)
{
 if (!(r->eStatus & 2)) return -6;
 if (r->uCategoryIndex == (u32)-90) return -2;
 const LobbyRankVariationT& variation = r->pCategories[r->uCategoryIndex].pIndices[r->uIndexIndex].pVariations[r->uVariationIndex];
 if ((u32)index >= variation.uParamCount) return -2;
 *data = variation.uParams[index];
 return 0;
}
extern "C" s32 LobbyRankRowCount(LobbyRankT* r)
{
 if (!(r->eStatus & 2) || r->eDownloadStatus == 2) return -6;
 return DispListCount(r->pLists[r->uListIndex % r->uListCount].pList);
}

#include "dirtymem.h"
extern "C" LobbyLocalizeT* LobbyLocalizeDestroy(LobbyLocalizeT*);
struct RankRowT { s32 pts; s32 rank; u32 uFlags; char name[20]; char stat[260]; };
typedef char RankRowFlagsOffset[(offsetof(RankRowT,uFlags)==8)?1:-1];
extern "C" s32 LobbyRankRowAttrib(LobbyRankT* r, s32 row, s32* attrib)
{
 if (!(r->eStatus & 2) || r->eDownloadStatus == 2) return -6;
 RankRowT* entry = static_cast<RankRowT*>(DispListIndex(r->pLists[r->uListIndex % r->uListCount].pList, row));
 if (!entry) return -13;
 *attrib = ((entry->uFlags & 1) ? 1 : 0) | ((entry->uFlags & 4) ? 4 : 0);
 return 0;
}
static void RankFlushList(LobbyRankT* r, u32 index)
{
 u32 slot = index % r->uListCount;
 LobbyRankListT& list = r->pLists[slot];
 if (list.pColumns) { DirtyMemFree(list.pColumns, 1819438699, r->iMemGroup); list.pColumns = 0; }
 if (list.pList) { LobbyApiListFree(r->pLobby, slot + 5, list.pList); list.pList = 0; }
 list.uCategoryIndex = (u16)-1; list.uIndexIndex = (u16)-1; list.uVariationIndex = (u16)-1;
 list.iOffset = -1; list.iRows = -1; list.iBuddyCount = -1;
}
extern "C" void LobbyRankFlush(LobbyRankT* r, s32 all)
{
 if (!(r->eStatus & 2)) return;
 if (all) { for (u32 i=0; i<r->uListCount; ++i) RankFlushList(r, i); }
 else RankFlushList(r, r->uListIndex);
}
extern "C" s32 LobbyRankCancelCB(LobbyRankT* r)
{
 r->pCallback = 0; r->eDownloadStatus = 0; r->pUserData = 0;
 for (u32 i=0; i<r->uListCount; ++i) RankFlushList(r, i);
 return LobbyApiCancelCB(r->pLobby, r->iLastCallbackId);
}
extern "C" void LobbyRankDestroy(LobbyRankT* r)
{
 if (r->pLocalize) LobbyLocalizeDestroy(r->pLocalize);
 if (r->iLastCallbackId) LobbyApiCancelCB(r->pLobby, r->iLastCallbackId);
 LobbyRankFlush(r, 1);
 if (r->pCategories) DirtyMemFree(r->pCategories, 1819438699, r->iMemGroup);
 if (r->pSymTab) DirtyMemFree(r->pSymTab, 1819438699, r->iMemGroup);
 DirtyMemFree(r, 1819438699, r->iMemGroup);
}
static s32 RankSelectIndex(u32 oldIndex, s32 requested, u32 count)
{
 switch (requested) {
 case -100: return oldIndex;
 case -99: return 0;
 case -98: return count-1;
 case -97: return oldIndex+1 == count ? oldIndex : oldIndex+1;
 case -95: return oldIndex == count ? 0 : oldIndex+1;
 case -94: return oldIndex == 0 ? count-1 : oldIndex-1;
 default: return requested >= 0 && (u32)requested < count ? requested : -1;
 }
}
extern "C" s32 LobbyRankSelect(LobbyRankT* r, s32 category, s32 index, s32 variation)
{
 if (category == -90) {
  if (index < 0) return -2;
  if (variation < 0) return -3;
  r->uListIndex = 0;
 } else {
  category = RankSelectIndex(r->uCategoryIndex, category, r->uCategoryCount);
  if (category < 0) return -1;
  index = RankSelectIndex(r->uIndexIndex, index, r->pCategories[category].uIndexCount);
  if (index < 0) return -2;
  LobbyRankIndexT& item = r->pCategories[category].pIndices[index];
  variation = RankSelectIndex(r->uVariationIndex, variation, item.uVariationCount);
  if (variation < 0) return -3;
  r->uListIndex = item.pVariations[variation].uListIndex;
 }
 r->uCategoryIndex=category; r->uIndexIndex=index; r->uVariationIndex=variation;
 r->uCurrentScrollColumn=0; r->uCurrentScrollRow=0;
 r->eStatus |= 2; r->eDownloadStatus &= ~2;
 return 0;
}

#include "lobbytagfield.h"
#include "platform.h"
extern "C" s32 LobbyLocalizeData(LobbyLocalizeT*, s32, char*, s32, const char*);
extern "C" const char* LobbyRankData(LobbyRankT* r, s32 column, s32 row, char* out, s32 length)
{
 if (!(r->eStatus & 2)) return 0;
 const LobbyRankListT& list = r->pLists[r->uListIndex % r->uListCount];
 RankRowT* entry = static_cast<RankRowT*>(DispListIndex(list.pList, row));
 if (!entry) return 0;
 if (column == -3) { ds_strnzcpy(out, entry->name, length); return out; }
 if (column == -2) { ds_snzprintf(out, length, "%d", entry->pts); return out; }
 if (column == -1) { ds_snzprintf(out, length, "%d", entry->rank); return out; }
 if (length > 1024) length = 1024;
 if ((u32)column >= list.uColumnCount) return 0;
 s32 type = 0;
 if (r->eDownloadStatus != 2) type = list.pColumns[column].iType;
 if (type == 0 || type == 0x7e726177) {
  TagFieldGetDelim(entry->stat, out, length, "", column, ',');
  return out;
 }
 char buffer[1088]; const char* source; s32 localType;
 if (type == 0x7e726e6b || type == 0x7e707473) {
  ds_snzprintf(buffer, length, "%d", type == 0x7e726e6b ? entry->rank : entry->pts);
  source = buffer; localType = 0x7e6e756d;
 } else if (type == 0x7e6e616d) {
  source = entry->name; localType = 0x7e737472;
 } else {
  TagFieldGetDelim(entry->stat, buffer, length, "", column, ',');
  source = buffer; localType = type;
 }
 LobbyLocalizeData(r->pLocalize, localType, out, length, source);
 return out;
}

// ARTIST 82B263F8: one allocation holds the 0x60-byte ranker and four lists.
struct RankAllocation { LobbyRankT ranker; LobbyRankListT lists[4]; };
typedef char RankAllocationSize[(sizeof(RankAllocation)==0x2b0)?1:-1];
extern "C" LobbyRankT* LobbyRankCreate(LobbyApiRefT* lobby, s32 cache)
{
 (void)cache;
 s32 group = DirtyMemGroupQuery();
 RankAllocation* allocation = static_cast<RankAllocation*>(DirtyMemAlloc(sizeof(RankAllocation), 1819438699, group));
 if (!allocation || !lobby) return 0;
 LobbyRankT* r = &allocation->ranker;
 r->iMemGroup=group; r->pLobby=lobby; r->pLocalize=0; r->pCallback=0;
 r->uListCount=4; r->eStatus=0; r->eDownloadStatus=0; r->pCategories=0;
 r->uCategoryCount=0; r->uCategoryIndex=0; r->uIndexIndex=0; r->uVariationIndex=0;
 r->iId=0; r->iLastCallbackId=0; r->iLastError=0; r->pLists=allocation->lists;
 r->strView[0]=0; r->strView[1]=0; r->strView[2]=0; r->strView[3]=0;
 r->strView[4]=0; r->strView[5]=0; r->strView[6]=0; r->strView[7]=0;
 r->strView[8]=0; r->strView[9]=0; r->strView[10]=0;
 memset(r->pLists, 0, r->uListCount * sizeof(LobbyRankListT));
 for (u32 i=0; i<r->uListCount; ++i) {
  LobbyRankListT& list=r->pLists[i]; list.pRanker=r;
  list.uCategoryIndex=(u16)-1; list.uIndexIndex=(u16)-1; list.uVariationIndex=(u16)-1;
  list.uListIndex=(u16)-1; list.iOffset=-1; list.iRows=-1; list.iBuddyCount=-1;
 }
 r->pSymTab=0; r->iSymCount=0;
 return r;
}

#include "dirtylib.h"
static void RankDispListCb(LobbyApiRefT*, LobbyApiMsgT*, void* user)
{
 LobbyRankListT* list = static_cast<LobbyRankListT*>(user);
 LobbyRankT* r = list->pRanker;
 if (DispListCount(list->pList) == list->uExpectedRowCount) {
  list->iId = -1;
  if (r->pCallback) {
   LobbyApiMsgT msg; msg.kind=0x6c697374; msg.code=0; msg.pData=0;
   r->pCallback(r, &msg, r->pUserData);
  }
 }
}
static void RankSnapCb(LobbyApiRefT* lobby, LobbyApiMsgT* msg, void* user)
{
 LobbyRankT* r=static_cast<LobbyRankT*>(user);
 r->iLastCallbackId=0;
 if (r->eDownloadStatus != 2) {
  NetPrintfCode("_LobbyRankSnapCb: incorrectly triggered in state %d\n", r->eDownloadStatus);
  return;
 }
 r->eDownloadStatus=0;
 if (msg->code) {
  NetPrintfCode("_LobbyRankSnapCb: error code = %d\n", msg->code);
  r->iLastError=msg->code==0x74696d65 ? -8 : -7;
 } else {
  s32 channel=TagFieldGetNumber(TagFieldFind(msg->pData,"CHAN"),-1);
  if (channel < 0) NetPrintfCode("_LobbyRankSnapCb: bad channel = %d\n",channel);
  else if ((u32)(channel-5) >= r->uListCount) NetPrintfCode("_LobbyRankSnapCb: bad channel %d >= %d\n",channel-5,r->uListCount);
  else {
   LobbyRankListT& list=r->pLists[channel-5];
   s32 seq=TagFieldGetNumber(TagFieldFind(msg->pData,"SEQN"),-1);
   if (seq != list.iId) { NetPrintfCode("_LobbyRankSnapCb: mismatched uids, %d != %d\n",seq,list.iId); return; }
   list.uExpectedRowCount=TagFieldGetNumber(TagFieldFind(msg->pData,"RANGE"),0);
   TagFieldGetString(TagFieldFind(msg->pData,"DESC"),list.strName,sizeof(list.strName),"");
   list.uColumnCount=TagFieldGetNumber(TagFieldFind(msg->pData,"CC"),0);
   list.uFixedColumnCount=TagFieldGetNumber(TagFieldFind(msg->pData,"FC"),0);
   if (list.pList) { LobbyApiListFree(lobby,channel,list.pList); list.pList=0; }
   DirtyMemGroupEnter(r->iMemGroup);
   list.pList=LobbyApiListAlloc(lobby,channel,RankDispListCb,&list);
   DirtyMemGroupLeave();
   if (!list.pList) r->iLastError=-7;
   else {
    if (list.pColumns) { DirtyMemFree(list.pColumns,1819438699,r->iMemGroup); list.pColumns=0; }
    list.pColumns=static_cast<LobbyRankColumnT*>(DirtyMemAlloc(sizeof(LobbyRankColumnT)*list.uColumnCount,1819438699,r->iMemGroup));
    if (!list.pColumns) r->iLastError=-7;
    else {
     for (u32 i=0; i<list.uColumnCount; ++i) {
      char key[16]; LobbyRankColumnT& col=list.pColumns[i];
      ds_snzprintf(key,sizeof(key),"CN%d",i); TagFieldGetString(TagFieldFind(msg->pData,key),col.strName,sizeof(col.strName),"");
      ds_snzprintf(key,sizeof(key),"CD%d",i); TagFieldGetString(TagFieldFind(msg->pData,key),col.strDesc,sizeof(col.strDesc),"");
      ds_snzprintf(key,sizeof(key),"CP%d",i); TagFieldGetString(TagFieldFind(msg->pData,key),col.strParam,sizeof(col.strParam),"");
      ds_snzprintf(key,sizeof(key),"CW%d",i); col.uWidth=TagFieldGetNumber(TagFieldFind(msg->pData,key),56);
      ds_snzprintf(key,sizeof(key),"CT%d",i); col.iType=TagFieldGetNumber(TagFieldFind(msg->pData,key),0);
      ds_snzprintf(key,sizeof(key),"CS%d",i); col.iStyle=TagFieldGetNumber(TagFieldFind(msg->pData,key),0);
     }
     r->iLastError=0;
     if (list.uExpectedRowCount) return;
     list.iId=-1;
    }
   }
   if (r->pCallback) { msg->kind=0x6c697374; msg->code=r->iLastError; r->pCallback(r,msg,r->pUserData); }
   return;
  }
  r->iLastError=-7;
 }
 if (r->pCallback) { msg->kind=0x6c697374; msg->code=r->iLastError; r->pCallback(r,msg,r->pUserData); }
}

extern "C" LobbyApiUserT* LobbyApiGetUserInfo(LobbyApiRefT*, const char*);
extern "C" LobbyLocalizeT* LobbyLocalizeCreate(u32, s32);
static u32 RankBuddyHash(const char** names, s32 count)
{
 u32 hash=0;
 while (count--) {
  const signed char* c=reinterpret_cast<const signed char*>(*names++);
  while (*c) { if (*c > 32) hash=((*c & 0x5f) + 131*hash); ++c; }
 }
 return hash;
}
static bool RankIndexMatch(LobbyRankT* r, LobbyRankListT& list, s32 offset, s32 rows, const char** names, s32 count)
{
 if (r->uCategoryIndex != list.uCategoryIndex || r->uIndexIndex != list.uIndexIndex ||
     r->uVariationIndex != list.uVariationIndex || r->uListIndex != list.uListIndex ||
     list.iOffset != offset || list.iRows != rows || list.iBuddyCount != count) return false;
 return RankBuddyHash(names,count) == list.uBuddyHash;
}
extern "C" s32 LobbyRankFetch(LobbyRankT* r, s32 timeout, s32 offset, s32 rows,
 const char** names, s32 count, LobbyRankCallbackT* callback, void* user)
{
 (void)timeout;
 if (!(r->eStatus & 2)) return -6;
 if (!r->pLocalize) {
  u32 locality=0x656e5553; LobbyApiUserT* info=LobbyApiGetUserInfo(r->pLobby,0);
  if (info) locality=info->uLocality;
  DirtyMemGroupEnter(r->iMemGroup); r->pLocalize=LobbyLocalizeCreate(locality,0); DirtyMemGroupLeave();
 }
 u32 slot=r->uListIndex % r->uListCount;
 LobbyRankListT& list=r->pLists[slot];
 if (RankIndexMatch(r,list,offset,rows,names,count)) {
  if (callback) { LobbyApiMsgT msg; msg.kind=0x6c697374; msg.code=0; msg.pData=0; callback(r,&msg,user); }
  return 0;
 }
 char request[1824]; request[0]=0;
 TagFieldSetNumber(request,sizeof(request),"CHAN",slot+5);
 TagFieldSetString(request,sizeof(request),"VIEW",r->strView);
 TagFieldSetNumber(request,sizeof(request),"CI",r->uCategoryIndex);
 TagFieldSetNumber(request,sizeof(request),"II",r->uIndexIndex);
 TagFieldSetNumber(request,sizeof(request),"VI",r->uVariationIndex);
 ++r->iId; TagFieldSetNumber(request,sizeof(request),"SEQN",r->iId);
 TagFieldSetNumber(request,sizeof(request),"COLS",1); list.iId=r->iId;
 if (offset == -89) TagFieldSetString(request,sizeof(request),"FIND","#");
 else if (count == 1) {
  if (offset == -93) {
   TagFieldSetString(request,sizeof(request),"FIND",**names ? *names : "$");
   if (rows >= 0) TagFieldSetNumber(request,sizeof(request),"RANGE",rows);
  }
 } else if (count <= 0) {
  if (rows >= 0) TagFieldSetNumber(request,sizeof(request),"RANGE",rows);
  TagFieldSetNumber(request,sizeof(request),"START",offset);
 }
 TagFieldPrintf(request,sizeof(request),"~BUDDIES=");
 for (s32 i=0; i<count; ++i) TagFieldPrintf(request,sizeof(request),",%s",*names[i] ? names[i] : "$");
 list.uCategoryIndex=r->uCategoryIndex; list.uIndexIndex=r->uIndexIndex;
 list.uVariationIndex=r->uVariationIndex; list.uListIndex=r->uListIndex;
 list.iOffset=offset; list.iRows=rows; list.iBuddyCount=count; list.uBuddyHash=RankBuddyHash(names,count);
 r->pCallback=callback; r->pUserData=user; r->eDownloadStatus=2;
 if (r->iLastCallbackId) LobbyApiCancelCB(r->pLobby,r->iLastCallbackId);
 if (list.pList) { LobbyApiListFree(r->pLobby,slot+5,list.pList); list.pList=0; }
 r->iLastCallbackId=LobbyApiRequestCB(r->pLobby,0x736e6170,request,RankSnapCb,r);
 if (r->iLastCallbackId < 0) { r->eDownloadStatus=0; return -10; }
 return 0;
}

#include <stdlib.h>
static const char* RankFindSymbol(LobbyRankT* r, s32 index)
{
 if (!r->pSymTab || index < 0 || index >= r->iSymCount) return "$unknown$";
 const char* value=r->pSymTab;
 while (index-- > 0) value += strlen(value)+1;
 return value;
}
static void RankCategoryCb(LobbyApiRefT*, LobbyApiMsgT* msg, void* user)
{
 LobbyRankT* r=static_cast<LobbyRankT*>(user);
 r->iLastCallbackId=0;
 if (r->eDownloadStatus != 1) return;
 r->eDownloadStatus=0;
 if (msg->code) {
  r->iLastError=msg->code==0x74696d65 ? -8 : msg->code==0x75766977 ? -4 : -7;
  if (r->pCallback) { msg->code=r->iLastError; r->pCallback(r,msg,r->pUserData); }
  return;
 }
 s32 space=TagFieldGetNumber(TagFieldFind(msg->pData,"SS"),0);
 if (space) {
  r->pSymTab=static_cast<char*>(DirtyMemAlloc(space,1819438699,r->iMemGroup));
  if (!r->pSymTab) {
   r->iLastError=-9; if (r->pCallback) { msg->code=-9; r->pCallback(r,msg,r->pUserData); } return;
  }
  char* target=r->pSymTab; s32 count=0; const char* symbols=TagFieldFind(msg->pData,"SYMS");
  if (symbols) {
   for (;;) { s32 n=TagFieldGetDelim(symbols,target,space,0,count,','); if (n<0) break; ++count; space-=n+1; target+=n+1; }
  } else {
   for (;;) { const char* item=TagFieldFindIdx(msg->pData,"",count); if (!item) break; s32 n=TagFieldGetString(item,target,space,""); ++count; space-=n+1; target+=n+1; }
  }
  r->iSymCount=count;
 } else { r->pSymTab=0; r->iSymCount=0; }
 u32 categories=TagFieldGetNumber(TagFieldFind(msg->pData,"CC"),1);
 u32 indices=TagFieldGetNumber(TagFieldFind(msg->pData,"IC"),1);
 u32 variations=TagFieldGetNumber(TagFieldFind(msg->pData,"VC"),1);
 LobbyRankCategoryT* category=static_cast<LobbyRankCategoryT*>(DirtyMemAlloc(12*(4*variations+indices+categories),1819438699,r->iMemGroup));
 if (!category) { r->iLastError=-9; if (r->pCallback) { msg->code=-9; r->pCallback(r,msg,r->pUserData); } return; }
 r->pCategories=category; r->uCategoryCount=categories;
 LobbyRankIndexT* index=reinterpret_cast<LobbyRankIndexT*>(category+categories);
 LobbyRankVariationT* variation=reinterpret_cast<LobbyRankVariationT*>(index+indices);
 const char* records=TagFieldFind(msg->pData,"R"); const char* types=TagFieldFind(msg->pData,"U");
 char value[64]; s32 field=0, typeField=1; u32 params=0; u32 state=0;
 while (TagFieldGetDelim(records,value,sizeof(value),"",field,',') > 0 && value[0]) {
  bool variationDone=false;
  switch (state) {
   case 0: category->pName=RankFindSymbol(r,atoi(value)); state=1; break;
   case 1: indices=atoi(value); category->pIndices=index; category->uIndexCount=indices; state=2; break;
   case 2: index->pName=RankFindSymbol(r,atoi(value)); state=3; break;
   case 3: variations=atoi(value); index->pVariations=variation; index->uVariationCount=variations; state=4; break;
   case 4: variation->pName=RankFindSymbol(r,atoi(value)); state=5; break;
   case 5: variation->uListIndex=atoi(value); state=6; break;
   case 6: variation->uParamCount=atoi(value); params=0; variationDone=(variation->uParamCount==0); state=7; break;
   case 7: variation->uParams[params++]=atoi(value); variationDone=(params==variation->uParamCount); break;
  }
  if (variationDone) {
   if (types) { char type[16]; TagFieldGetDelim(types,type,sizeof(type),"",typeField++,','); variation->iUserType=*type ? TagFieldGetToken(type,0) : 0; }
   ++variation;
   if (--variations) state=4;
   else { ++index; if (--indices) state=2; else { ++category; if (--categories) state=0; else { r->eStatus |= 1; if (r->pCallback) r->pCallback(r,msg,r->pUserData); return; } } }
  }
  ++field;
 }
}
extern "C" s32 LobbyRankCategoryFetch(LobbyRankT* r, const char* view, LobbyRankCallbackT* callback, void* user)
{
 if (strlen(view) > 11) return -4;
 if (r->eDownloadStatus == 1) return -11;
 if (r->eStatus & 1) {
  if (strcmp(view,r->strView)==0) return 0;
  r->eStatus &= ~1; DirtyMemFree(r->pCategories,1819438699,r->iMemGroup); r->pCategories=0;
 }
 char request[80]; request[0]=0; TagFieldSetString(request,32,"VIEW",view);
 r->pCallback=callback; r->pUserData=user; r->eDownloadStatus=1;
 if (r->iLastCallbackId) LobbyApiCancelCB(r->pLobby,r->iLastCallbackId);
 r->iLastCallbackId=LobbyApiRequestCB(r->pLobby,0x63617465,request,RankCategoryCb,r);
 if (r->iLastCallbackId < 0) { r->eDownloadStatus=0; return -10; }
 ds_strnzcpy(r->strView,view,11); r->iLastError=-5;
 return 0;
}
