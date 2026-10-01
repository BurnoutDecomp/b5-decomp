#pragma once

#include "GameShared/Jobs/ObjectToMesh/ObjectToMesh.h"

// ARTIST82926A70 stores r4 and forwards it to ExecuteImplementation827FF380.
// The latter also consumes r4: the old no-argument reconstruction was incorrect.

struct ObjectToMeshJob
{
    void Execute(ObjectToMeshJobInfo* lpData);

private:
    void ExecuteImplementation(ObjectToMeshJobInfo* lpData);
    static void SharedMemoryChangeCallback(void* lpContext);
    ObjectToMeshJobInfo* mpData;
};
