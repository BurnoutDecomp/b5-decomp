// DirtySDK 5.5.3 LobbyHasher: X360 ARTIST PPC32; DecFIGS declarations.
// XDK size optimization matches ARTIST at 0x82B30D10.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "dirtymem.h"
#include "lobbyapi_x360_internal.h"
#include "lobbyname.h"
#include <stddef.h>
#include <string.h>

static const s32 HASHER_MEMID = 1818784616;

// ARTIST 0x82B30D88 bumps `expand` by one for the block's link record (addi r29,r4,1),
// allocates and walks `table+expand`, and returns `expand-1` (addi r3,r29,-1).
// DWARF (lobbyhasher.c:251-253): locals `table`, `item`.
extern "C" s32 HasherExpand(HasherRef* ref, s32 expand)
{
 HasherT* table;
 HasherT* item;
 if (expand < 1) expand=1;
 ++expand;
 table=static_cast<HasherT*>(DirtyMemAlloc(expand*sizeof(HasherT), HASHER_MEMID, ref->memgroup));
 if (!table) return -1;
 table->next=ref->list; ref->list=table;
 for (item=table+1; item != table+expand; ++item) {
  item->next=ref->avail; ref->avail=item;
 }
 return expand-1;
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
 // ARTIST 0x82B3147C re-tests `initial > 0` before the expand call, although the
 // clamp above already forces it.
 if (initial>0) HasherExpand(ref,initial);
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
// DWARF (lobbyhasher.c:362) gives HashStrHash one local, `int32_t hash`: the loop
// walks `key` itself (XDK char is signed: ARTIST's extsb. at 0x82B30EFC).
static s32 HashStrHash(HasherRef* ref, const char* key)
{
 s32 hash;
 for (hash=0; *key!=0; ++key)
  if (*key>' ') hash=(*key & 0x5f)+131*hash;
 return (hash & 0x7fffffff) % ref->hashlen;
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
// DWARF (lobbyhasher.c:509-511): locals `item`, `link`, `hash`. ARTIST 0x82B31068
// tests `(item = *link)` at the loop head, compares through a reloaded `*link`
// (0x82B310E8) and steps with `link = &(*link)->next` (lwz r29,0(r29)); the unlink
// follows the loop and owns the epilogue, the not-found exit branches to it
// (0x82B3111C). HashNumDel (0x82B31298) has the same shape.
extern "C" void* HashStrDel(HasherRef* ref, const char* key)
{
 HasherT* item, **link;
 s32 hash=HashStrHash(ref,key);
 for (link=&ref->hashbuf[hash];; link=&(*link)->next) {
  if ((item=*link)==0) return 0;
  if (ref->compare((*link)->key.ptr,key)==0) break;
 }
 *link=item->next;
 if (ref->iter.curr==item) ref->iter.curr=item->next;
 item->next=ref->avail; ref->avail=item; --ref->items;
 return item->data;
}

// The numeric bucket is computed in place in each HashNum* function (DWARF lists no
// numeric hash helper, only the `hash` local): ARTIST 0x82B31160/0x82B31228/0x82B31298
// all open with the absolute key modulo hashlen (signed divw, cmpwi before the copy).
extern "C" s32 HashNumAdd(HasherRef* ref, s32 key, void* data)
{
 HasherT* item;
 s32 hash;
 hash=(key>=0 ? key : -key)%ref->hashlen;
 if (!ref->avail && HasherExpand(ref,(3*ref->items+10)/2)<=0) return -1;
 item=ref->avail; ref->avail=item->next;
 item->key.val=key; item->data=data; item->next=ref->hashbuf[hash]; ref->hashbuf[hash]=item;
 ++ref->items; return 1;
}
extern "C" void* HashNumFind(HasherRef* ref, s32 key)
{
 HasherT* item;
 s32 hash;
 hash=(key>=0 ? key : -key)%ref->hashlen;
 for (item=ref->hashbuf[hash]; item!=0; item=item->next)
  if (item->key.val==key) return item->data;
 return 0;
}
extern "C" void* HashNumDel(HasherRef* ref, s32 key)
{
 HasherT* item, **link;
 s32 hash;
 hash=(key>=0 ? key : -key)%ref->hashlen;
 for (link=&ref->hashbuf[hash];; link=&(*link)->next) {
  if ((item=*link)==0) return 0;
  if ((*link)->key.val==key) break;
 }
 *link=item->next;
 if (ref->iter.curr==item) ref->iter.curr=item->next;
 item->next=ref->avail; ref->avail=item; --ref->items;
 return item->data;
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
