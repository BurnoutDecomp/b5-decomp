#pragma once

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"

// ARTIST getters 828E1D48..828E2528 and DecFIGS CgsBundleLoaderModuleIO.h.
// Native event records and their queue storage use sizeof(T), not the 32-bit
// console's byte offsets or opaque placeholder spans.
namespace CgsResource { namespace BundleLoaderIO {

struct InputBuffer_Update : public CgsModule::IOBuffer
{
    typedef CgsModule::EventQueue<Events::LoadBundleRequest, 256> LoadBundleRequestQueue;
    typedef CgsModule::EventQueue<Events::UnloadBundleRequest, 256> UnloadBundleRequestQueue;
    typedef LoadBundleRequestQueue EventQueueStorage;

    void Construct();
    void Destruct();
    const LoadBundleRequestQueue* GetLoadBundleRequestQueue() const;
    LoadBundleRequestQueue* GetLoadBundleRequestQueue();
    const UnloadBundleRequestQueue* GetUnloadBundleRequestQueue() const;
    UnloadBundleRequestQueue* GetUnloadBundleRequestQueue();
    const EventQueueStorage* GetEventQueue() const; // historical accessor alias

private:
    LoadBundleRequestQueue mLoadBundleRequestQueue;
    UnloadBundleRequestQueue mUnloadBundleRequestQueue;
};
typedef InputBuffer_Update InputBuffer;

struct InputBuffer_Record : public CgsModule::IOBuffer
{
    typedef CgsModule::VariableEventQueue<4096, 16> PoolReceiveQueue;
    typedef PoolReceiveQueue RecordStorage;

    void Construct();
    void Destruct();
    const PoolReceiveQueue* GetPoolReceiveQueue() const;
    PoolReceiveQueue* GetPoolReceiveQueue();
    const RecordStorage* GetRecord() const; // historical accessor aliases
    RecordStorage* GetRecord();

private:
    PoolReceiveQueue mPoolReceiveQueue;
};

struct OutputBuffer : public CgsModule::IOBuffer
{
    typedef CgsModule::VariableEventQueue<4096, 16> PoolSendQueue;
    typedef CgsModule::EventQueue<Events::LoadBundleResponse, 256> LoadBundleResponseQueue;
    typedef CgsModule::EventQueue<Events::UnloadBundleResponse, 256> UnloadBundleResponseQueue;
    typedef CgsModule::VariableEventQueue<256, 16> StreamRequestQueue;
    typedef PoolSendQueue PoolStorage;
    typedef StreamRequestQueue StreamStorage;

    void Construct();
    void Destruct();
    const PoolSendQueue* GetPoolSendQueue() const;
    PoolSendQueue* GetPoolSendQueue();
    const LoadBundleResponseQueue* GetLoadBundleResponseQueue() const;
    LoadBundleResponseQueue* GetLoadBundleResponseQueue();
    const UnloadBundleResponseQueue* GetUnloadBundleResponseQueue() const;
    UnloadBundleResponseQueue* GetUnloadBundleResponseQueue();
    const StreamRequestQueue* GetStreamRequestQueue() const;
    StreamRequestQueue* GetStreamRequestQueue();
    PoolStorage* GetPool(); // historical accessor aliases
    const StreamStorage* GetStream() const;

private:
    PoolSendQueue mPoolSendQueue;
    LoadBundleResponseQueue mLoadBundleResponseQueue;
    UnloadBundleResponseQueue mUnloadBundleResponseQueue;
    StreamRequestQueue mStreamRequestQueue;
};

} }
