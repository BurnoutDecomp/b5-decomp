// DirtySDK lobby -- display list (core/source/util/lobbydisplist.c).
//
// Items come from pool blocks chained through their first slot; free items form a
// singly linked list threaded through their data pointer. The view array holds the
// live items, the ones passing the filter first ("shown"), and has room for twice
// the item count so DispListOrder can partition it in place.
//
// Only the entry points the game build carries are reconstructed (DispListShown,
// DispListFilt and DispListDirty are not part of it).

#include "lobbydisplist.h"
#include "lobbysort.h"   // LobbyMSort
#include "dirtymem.h"    // DirtyMemAlloc / DirtyMemFree / DirtyMemGroupQuery

#include <cstdint>
#include <cstring>

// memory module id 'ldsp'
#define DISPLIST_MEMID (('l' << 24) | ('d' << 16) | ('s' << 8) | 'p')

struct DispListT
{
    void* data;
};

struct DispListRef
{
    s32 memgroup;           // +0x00  memory group the list allocates from
    s32 addcnt;             // +0x04  fixed growth step
    s32 addpct;             // +0x08  growth step in percent of the count (wins when > 0)
    s32 change;             // +0x0C  pending changes (the view needs rebuilding)
    s32 dirty;              // +0x10  pending refresh without a rebuild
    s32 avail;              // +0x14  free items
    s32 shown;              // +0x18  view items that passed the filter
    s32 count;              // +0x1C  view items
    s32 filtcon;            // +0x20
    void* filtref;
    DispListFiltT* filtptr;
    s32 sortcon;
    void* sortref;
    DispListSortT* sortptr;
    void* dataref;
    DispListT* free;        // free item chain (through DispListT::data)
    DispListT* list;        // pool block chain (through slot 0 of each block)
    DispListT** view;       // 2 * (avail + count) entries
};

// Allocate another pool block and grow the view to match. Returns the number of items
// added (0 when the allocation failed).
static s32 DispListExpand(DispListRef* ref)
{
    s32 size;
    DispListT* list;
    DispListT** view;
    s32 idx;

    if (ref->addpct >= 1)
    {
        size = (ref->count * 100) / ref->addpct;
        if (ref->addcnt != 0)
        {
            size = (size + ref->addcnt) / 2;
        }
    }
    else
    {
        size = ref->addcnt;
    }
    if (size < 2)
    {
        size = 2;
    }

    // slot 0 links the previous block, the last slot is a -1 sentinel
    list = static_cast<DispListT*>(DirtyMemAlloc((size + 2) * (s32)sizeof(DispListT), DISPLIST_MEMID, ref->memgroup));
    if (list == NULL)
    {
        return 0;
    }
    memset(list, 0, (size + 2) * sizeof(DispListT));
    list[0].data = ref->list;
    list[size + 1].data = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
    ref->list = list;
    for (idx = 1; list[idx].data == NULL; ++idx)
    {
        list[idx].data = ref->free;
        ref->free = &list[idx];
    }
    ref->avail += idx - 1;

    view = static_cast<DispListT**>(DirtyMemAlloc((ref->avail + ref->count) * 2 * (s32)sizeof(DispListT*), DISPLIST_MEMID, ref->memgroup));
    if (ref->view != NULL)
    {
        memcpy(view, ref->view, ref->count * sizeof(DispListT*));
        DirtyMemFree(ref->view, DISPLIST_MEMID, ref->memgroup);
    }
    ref->view = view;
    return idx - 1;
}

// LobbyMSort compare over view entries: forwards the two records to the sort callback.
static s32 DispListCompare(void* zref, const void* p1, const void* p2)
{
    DispListRef* ref = static_cast<DispListRef*>(zref);
    DispListT* q1 = *static_cast<DispListT* const*>(p1);
    DispListT* q2 = *static_cast<DispListT* const*>(p2);

    return ref->sortptr(ref->sortref, ref->sortcon, q1->data, q2->data);
}

extern "C" DispListRef* DispListCreate(s32 basis, s32 addcnt, s32 addpct)
{
    DispListRef* ref;
    s32 memgroup;

    memgroup = DirtyMemGroupQuery();
    ref = static_cast<DispListRef*>(DirtyMemAlloc((s32)sizeof(*ref), DISPLIST_MEMID, memgroup));
    memset(ref, 0, sizeof(*ref));
    ref->memgroup = memgroup;

    // the first block holds the basis count
    ref->addcnt = basis;
    DispListExpand(ref);

    ref->addcnt = addcnt;
    ref->addpct = addpct;
    return ref;
}

extern "C" void DispListDestroy(DispListRef* ref)
{
    while (ref->list != NULL)
    {
        DispListT* kill = ref->list;
        ref->list = static_cast<DispListT*>(kill->data);
        DirtyMemFree(kill, DISPLIST_MEMID, ref->memgroup);
    }
    DirtyMemFree(ref->view, DISPLIST_MEMID, ref->memgroup);
    DirtyMemFree(ref, DISPLIST_MEMID, ref->memgroup);
}

extern "C" void DispListClear(DispListRef* ref)
{
    s32 idx;
    DispListT* item;

    for (idx = 0; idx < ref->count; ++idx)
    {
        item = ref->view[idx];
        item->data = ref->free;
        ref->free = item;
    }
    ref->shown = 0;
    ref->avail += ref->count;
    ref->count = 0;
    ref->change = 1;
}

extern "C" DispListT* DispListAdd(DispListRef* ref, void* recptr)
{
    DispListT* item;

    if ((ref->free == NULL) && (DispListExpand(ref) < 1))
    {
        return NULL;
    }
    item = ref->free;
    ref->free = static_cast<DispListT*>(item->data);
    item->data = recptr;
    ref->view[ref->count] = item;
    ref->count += 1;
    ref->avail -= 1;

    // a record the filter would hide does not change the shown view
    if ((ref->filtptr == NULL) || (ref->filtptr(ref->filtref, ref->filtcon, item->data) > 0))
    {
        ref->change += 1;
    }
    return item;
}

extern "C" void DispListSet(DispListRef* ref, DispListT* item, void* recptr)
{
    if ((ref->filtptr == NULL) || (item->data == recptr) ||
        (ref->filtptr(ref->filtref, ref->filtcon, item->data) > 0) ||
        (ref->filtptr(ref->filtref, ref->filtcon, recptr) > 0))
    {
        ref->change += 1;
    }
    item->data = recptr;
}

extern "C" void* DispListGet(DispListRef* ref, DispListT* item)
{
    (void)ref;
    return (item != NULL) ? item->data : NULL;
}

extern "C" void* DispListDelByIndex(DispListRef* ref, s32 idx)
{
    void* data;
    DispListT* item;

    if ((idx >= ref->count) || (ref->view[idx] == NULL))
    {
        return NULL;
    }
    item = ref->view[idx];
    if (idx < ref->shown)
    {
        ref->shown -= 1;
        ref->change += 1;
    }
    for (ref->count -= 1; idx < ref->count; ++idx)
    {
        ref->view[idx] = ref->view[idx + 1];
    }

    data = item->data;
    item->data = ref->free;
    ref->free = item;
    ref->avail += 1;
    return data;
}

extern "C" void* DispListDel(DispListRef* ref, DispListT* item)
{
    s32 idx;

    for (idx = 0; idx < ref->count; ++idx)
    {
        if (ref->view[idx] == item)
        {
            break;
        }
    }
    return DispListDelByIndex(ref, idx);
}

extern "C" s32 DispListCount(DispListRef* ref)
{
    return ref->count;
}

extern "C" void* DispListIndex(DispListRef* ref, s32 index)
{
    if ((ref == NULL) || (index < 0) || (index >= ref->count))
    {
        return NULL;
    }
    return ref->view[index]->data;
}

extern "C" void DispListSort(DispListRef* ref, void* sortref, s32 sortcon, DispListSortT* sortptr)
{
    if (ref->count > 0)
    {
        ref->change = 1;
    }
    ref->sortref = sortref;
    ref->sortcon = sortcon;
    ref->sortptr = sortptr;
}

extern "C" s32 DispListChange(DispListRef* ref, s32 set)
{
    if (set != 0)
    {
        ref->change += 1;
    }
    return (ref->change > 0) ? 1 : 0;
}

extern "C" s32 DispListOrder(DispListRef* ref)
{
    DispListT** src;
    DispListT** dst1;
    DispListT** dst2;

    if (ref->change + ref->dirty == 0)
    {
        return 0;
    }
    if (ref->change == 0)
    {
        ref->dirty = 0;
        return 1;
    }

    // partition: shown items to the front, filtered ones parked past the count
    dst1 = ref->view;
    dst2 = ref->view + ref->count;
    for (src = ref->view; src != ref->view + ref->count; ++src)
    {
        if ((ref->filtptr != NULL) && (ref->filtptr(ref->filtref, ref->filtcon, (*src)->data) <= 0))
        {
            *dst2++ = *src;
        }
        else
        {
            *dst1++ = *src;
        }
    }
    ref->shown = (s32)(dst1 - ref->view);
    for (src = ref->view + ref->count; src != dst2; )
    {
        *dst1++ = *src++;
    }
    ref->count = (s32)(dst1 - ref->view);

    if (ref->sortptr != NULL)
    {
        LobbyMSort(ref, ref->view, ref->shown, (s32)sizeof(*ref->view), DispListCompare);
    }
    ref->change = 0;
    ref->dirty = 0;
    return 2;
}

extern "C" void DispListDataSet(DispListRef* ref, void* dataref)
{
    ref->dataref = dataref;
}

extern "C" void* DispListDataGet(DispListRef* ref)
{
    return ref->dataref;
}
