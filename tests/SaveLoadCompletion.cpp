// Execute production load-confirm/update/cleanup with a controlled storage edge.
// Completion must cross an Update boundary and tolerate callback reentrancy.
#include <cstdint>
#include <cstdio>
#include <unordered_map>
using u32=uint32_t;using u64=uint64_t;using f32=float;using DWORD=u32;using BOOL=int;
static const int FALSE=0;
static DWORD XGetOverlappedResult(void*,DWORD*,BOOL){return 997;}
namespace CgsDev {namespace Log {void WriteToLog(const char*){}}}
namespace CgsGui {
enum ESaveLoadTaskResult{E_SAVELOADTASKRESULT_SUCCESS=0,E_SAVELOADTASKRESULT_FAILURE=1};
struct SaveLoadTaskResultHandler{virtual void HandleSaveLoadTaskResult(ESaveLoadTaskResult)=0;};
struct MessageDisplay{};
namespace SaveLoadPC {
using WriteTicket=u64;
enum EContainerReadResult{E_CONTAINERREAD_OK,E_CONTAINERREAD_MISSING,E_CONTAINERREAD_CORRUPT,E_CONTAINERREAD_MISMATCH};
enum EWriteStatus{E_WRITE_PENDING,E_WRITE_SUCCEEDED,E_WRITE_FAILED};struct WriteReport{};
static EContainerReadResult nextRead=E_CONTAINERREAD_OK;
static EContainerReadResult ReadContainer(const char*,void*,int,void*,u32){return nextRead;}
static EWriteStatus PollWriteContainer(WriteTicket,WriteReport&){return E_WRITE_PENDING;}
static void FinishWriteContainer(WriteTicket){}
}
struct Memcard{int updates=0;void Update(int){++updates;}};
struct SaveLoadSystem{
    struct Stored{void* mpData;int miSize;}mStoredDataView;
    MessageDisplay* mpActiveMessageDisplay=nullptr;void* mpMugshotBufferData=nullptr;
    const char* macTitle="test";int mbAsyncOpState=0;void* maOverlapped=nullptr;
    Memcard* mpMemcardInterface=nullptr;
    int GetMugshotBufferSizeBytes(){return 0;}
    void LoadHandleConfirmLoad(u32);void Update();
};
}
namespace {
struct PendingSavePC{CgsGui::SaveLoadPC::WriteTicket muTicket=0;CgsGui::SaveLoadTaskResultHandler* mpHandler=nullptr;};
std::unordered_map<CgsGui::SaveLoadSystem*,PendingSavePC> sPendingSavesPC;
// --rev controls retain this fixture table, but old production never queues or
// consumes it, so the deferred-completion checks must fail on the old code.
struct PendingLoadResultPC{CgsGui::SaveLoadTaskResultHandler* mpHandler;CgsGui::ESaveLoadTaskResult meResult;};
std::unordered_map<CgsGui::SaveLoadSystem*,PendingLoadResultPC> sPendingLoadResultsPC;
void ReportSavePC(CgsGui::SaveLoadTaskResultHandler*,CgsGui::SaveLoadPC::EWriteStatus,const CgsGui::SaveLoadPC::WriteReport&){}
#include "save_cleanup.inc"
}
namespace CgsGui {
#include "save_load_confirm.inc"
#include "save_load_update.inc"
}
struct Handler:CgsGui::SaveLoadTaskResultHandler{
    int calls=0;CgsGui::ESaveLoadTaskResult result=CgsGui::E_SAVELOADTASKRESULT_FAILURE;
    CgsGui::SaveLoadSystem* reenter=nullptr;
    void HandleSaveLoadTaskResult(CgsGui::ESaveLoadTaskResult value)override{
        ++calls;result=value;if(reenter){auto* target=reenter;reenter=nullptr;target->Update();}
    }
};
int main(){
    unsigned checks=0,failures=0;auto expect=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::printf("FAIL %s\n",msg);}};
    CgsGui::SaveLoadSystem system;int data=0;system.mStoredDataView={&data,sizeof(data)};
    Handler handler;system.mpActiveMessageDisplay=reinterpret_cast<CgsGui::MessageDisplay*>(&handler);
    for(auto outcome:{CgsGui::SaveLoadPC::E_CONTAINERREAD_OK,CgsGui::SaveLoadPC::E_CONTAINERREAD_MISSING,
                      CgsGui::SaveLoadPC::E_CONTAINERREAD_CORRUPT,CgsGui::SaveLoadPC::E_CONTAINERREAD_MISMATCH}){
        CgsGui::SaveLoadPC::nextRead=outcome;handler.calls=0;
        system.LoadHandleConfirmLoad(1);
        expect(handler.calls==0,"confirm returns before notifying manager");
        handler.reenter=&system;system.Update();
        expect(handler.calls==1,"callback runs once, including reentrant Update");
        expect(handler.result==(outcome==CgsGui::SaveLoadPC::E_CONTAINERREAD_OK?CgsGui::E_SAVELOADTASKRESULT_SUCCESS:CgsGui::E_SAVELOADTASKRESULT_FAILURE),"real storage outcome forwarded");
        system.Update();expect(handler.calls==1,"no duplicate completion");
    }
    handler.calls=0;system.LoadHandleConfirmLoad(0);
    expect(handler.calls==1&&handler.result==CgsGui::E_SAVELOADTASKRESULT_FAILURE,"decline retains original immediate failure path");
    handler.calls=0;system.LoadHandleConfirmLoad(1);FinishPendingSavePC(&system);system.Update();
    expect(handler.calls==0,"release discards pending callback");
    std::printf("SaveLoadCompletion: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
