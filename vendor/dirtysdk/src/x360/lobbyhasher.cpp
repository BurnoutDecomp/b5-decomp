// DirtySDK 5.5.3 LobbyHasher: X360 ARTIST PPC32; DecFIGS declarations.
#include "dirtymem.h"
#include "lobbyname.h"
#include <stddef.h>
#include <string.h>

struct HasherT
{
 HasherT* next;
 void* data;
 union { s32 val; const char* ptr; } key;
};
struct HashEnumT { s32 idx; HasherT* curr; };
typedef s32 (HasherCompareF)(const char*, const char*);
struct HasherRef
{
 s32 memgroup;
 s32 items;
 s32 flush;
 HashEnumT iter;
 s32 hashlen;
 HasherT** hashbuf;
 HasherT* list;
 HasherT* avail;
 HasherCompareF* compare;
};
typedef char HasherItemSize[(sizeof(HasherT)==12)?1:-1];
typedef char HasherRefSize[(sizeof(HasherRef)==40)?1:-1];
typedef char HasherHashOffset[(offsetof(HasherRef,hashbuf)==24)?1:-1];
typedef char HasherCompareOffset[(offsetof(HasherRef,compare)==36)?1:-1];
static const s32 HASHER_MEMID = 1818784616;

extern "C" s32 HasherExpand(HasherRef* ref, s32 expand)
{
 if (expand < 1) expand=1;
 HasherT* table=static_cast<HasherT*>(DirtyMemAlloc((expand+1)*sizeof(HasherT), HASHER_MEMID, ref->memgroup));
 if (!table) return -1;
 table->next=ref->list; ref->list=table;
 for (HasherT* item=table+1; item != table+expand+1; ++item) {
  item->next=ref->avail; ref->avail=item;
 }
 return expand;
}
extern "C" HasherRef* HasherCreate(s32 initial, s32 hashlen)
{
 s32 group=DirtyMemGroupQuery();
 if (initial<1) initial=1;
 if (hashlen<16) hashlen=16;
 HasherRef* ref=static_cast<HasherRef*>(DirtyMemAlloc(sizeof(HasherRef),HASHER_MEMID,group));
 memset(ref,0,sizeof(*ref));
 ref->memgroup=group; ref->hashlen=hashlen;
 ref->hashbuf=static_cast<HasherT**>(DirtyMemAlloc(sizeof(HasherT*)*hashlen,HASHER_MEMID,group));
 memset(ref->hashbuf,0,sizeof(HasherT*)*ref->hashlen);
 ref->compare=LobbyNameCmp;
 HasherExpand(ref,initial);
 return ref;
}
extern "C" void HasherDestroy(HasherRef* ref)
{
 while (ref->list) { HasherT* block=ref->list; ref->list=block->next; DirtyMemFree(block,HASHER_MEMID,ref->memgroup); }
 DirtyMemFree(ref->hashbuf,HASHER_MEMID,ref->memgroup);
 DirtyMemFree(ref,HASHER_MEMID,ref->memgroup);
}
extern "C" void* HasherFlush(HasherRef* ref)
{
 if (!ref->items) return 0;
 while (!ref->hashbuf[ref->flush]) ref->flush=(ref->flush+1)%ref->hashlen;
 HasherT* item=ref->hashbuf[ref->flush];
 ref->hashbuf[ref->flush]=item->next;
 item->next=ref->avail; ref->avail=item; --ref->items;
 return item->data;
}
static s32 HashStrHash(HasherRef* ref, const char* key)
{
 u32 hash=0;
 for (const signed char* p=reinterpret_cast<const signed char*>(key); *p; ++p)
  if (*p>32) hash=(*p & 0x5f)+131u*hash;
 return (s32)(hash & 0x7fffffffu) % ref->hashlen;
}
extern "C" s32 HashStrAdd(HasherRef* ref, const char* key, void* data)
{
 s32 hash=HashStrHash(ref,key);
 if (!ref->avail && HasherExpand(ref,(3*ref->items+10)/2)<=0) return -1;
 HasherT* item=ref->avail; ref->avail=item->next;
 item->key.ptr=key; item->data=data; item->next=ref->hashbuf[hash]; ref->hashbuf[hash]=item;
 ++ref->items; return 1;
}
extern "C" void* HashStrFind(HasherRef* ref, const char* key)
{
 for (HasherT* item=ref->hashbuf[HashStrHash(ref,key)]; item; item=item->next)
  if (!ref->compare(item->key.ptr,key)) return item->data;
 return 0;
}
extern "C" void* HashStrDel(HasherRef* ref, const char* key)
{
 HasherT** link=&ref->hashbuf[HashStrHash(ref,key)];
 while (*link) {
  HasherT* item=*link;
  if (!ref->compare(item->key.ptr,key)) {
   *link=item->next;
   if (ref->iter.curr==item) ref->iter.curr=item->next;
   item->next=ref->avail; ref->avail=item; --ref->items;
   return item->data;
  }
  link=&item->next;
 }
 return 0;
}
extern "C" void HasherEnumInit(HasherRef* ref) { ref->iter.idx=0; ref->iter.curr=0; }
extern "C" void* HasherEnum(HasherRef* ref, void** key)
{
 if (!ref->iter.curr) {
  const s32 length=ref->hashlen;
  while (ref->iter.idx<ref->hashlen && !ref->hashbuf[ref->iter.idx]) ++ref->iter.idx;
  if (ref->iter.idx>=length) return 0;
  ref->iter.curr=ref->hashbuf[ref->iter.idx++];
 }
 HasherT* item=ref->iter.curr; ref->iter.curr=item->next;
 if (key) *key=const_cast<char*>(item->key.ptr);
 return item->data;
}
