// Uses the V1 contract supplied by EML; no game is simulated as a real game.
#include "diagnostic.cpp"
#include <iostream>
namespace Test {
BE_HostApiV1 runtimeApi{};const BE_HostApiV1* current=nullptr;int missing=-1;bool missingSquad=false;bool emptyTail=false;BE_Result hookResult=BE_Result_Ok;bool emptyHook=false;int created=0,disabled=0;
const BE_HostApiV1* BE_CALL Runtime(void*){return current;}
void BE_CALL LogMessage(void*,const char*){}
void __fastcall Tick(void*,float,void*){}
BE_Result BE_CALL Resolve(void*,const BE_MethodDescriptorV1* spec,BE_ResolvedMethodV1* out){for(size_t i=0;i<std::size(methods);++i)if(spec==&methods[i].spec){if(int(i)==missing)return BE_Result_NotFound;*out={reinterpret_cast<void*>(i+1),emptyTail&&i==Tail?nullptr:reinterpret_cast<void*>(&Tick)};return BE_Result_Ok;}*out={reinterpret_cast<void*>(1),reinterpret_cast<void*>(&Tick)};return BE_Result_Ok;}
BE_Result BE_CALL Class(void*,const char*,const char*,const char*,BE_ResolvedClassV1* out){*out={reinterpret_cast<void*>(1),reinterpret_cast<void*>(1),reinterpret_cast<void*>(1)};return BE_Result_Ok;}
BE_Result BE_CALL Field(void*,const BE_FieldDescriptorV1*,BE_ResolvedFieldV1* out){if(missingSquad)return BE_Result_NotFound;*out={reinterpret_cast<void*>(1),0};return BE_Result_Ok;}
void* BE_CALL Invoke(void*,const void*,void*,void**,void**){return nullptr;}
void* BE_CALL Unbox(void*,void* value){return value;}
int BE_CALL Copy(void*,const void*,char*,size_t){return 0;}
uint32_t BE_CALL Pin(void*,void*,int){return 1;}
void BE_CALL Free(void*,uint32_t){}
void* BE_CALL Value(void*,const void*,void*){return nullptr;}
BE_Result BE_CALL Hook(void*,const char*,void*,void*,void** next,uint64_t* handle){++created;if(hookResult!=BE_Result_Ok)return hookResult;*next=emptyHook?nullptr:reinterpret_cast<void*>(&Tick);*handle=emptyHook?0:created;return BE_Result_Ok;}
BE_Result BE_CALL Disable(void*,uint64_t){++disabled;return BE_Result_Ok;}
}
int main(){using namespace Test;int failures=0;auto check=[&](bool ok,const char* name){if(!ok){++failures;std::cerr<<"FAIL: "<<name<<"\n";}};
BE_ThirdPartyHostV1 supplied{};supplied.struct_size=sizeof(supplied);supplied.version=1;supplied.get_runtime=Runtime;supplied.log=LogMessage;host=&supplied;
check(Connect()==BE_Result_NotReady&&panelStatus.find("runtime")!=std::string::npos,"runtime not ready is retryable");
runtimeApi.abi_version=1;runtimeApi.resolve_method=Resolve;runtimeApi.resolve_field=Field;runtimeApi.resolve_class=Class;runtimeApi.runtime_invoke=Invoke;runtimeApi.object_unbox=Unbox;runtimeApi.copy_managed_string=Copy;runtimeApi.gchandle_new=Pin;runtimeApi.gchandle_free=Free;runtimeApi.field_get_value_object=Value;current=&runtimeApi;
missing=Tail;check(Connect()==BE_Result_NotFound&&panelStatus.find("TailLateTick")!=std::string::npos,"missing game method identifies exact stage");missing=-1;emptyTail=true;check(Connect()==BE_Result_ContractMismatch,"tail pointer required");emptyTail=false;
missing=LocalRotation;check(Connect()==BE_Result_NotFound&&panelStatus.find("get_localRotation")!=std::string::npos,"missing bone method is rejected before playback");missing=-1;missingSquad=true;check(Connect()==BE_Result_NotFound&&panelStatus.find("squadManager")!=std::string::npos,"missing squad field is named");missingSquad=false;
check(Connect()==BE_Result_NotReady&&panelStatus.find("hook chain")!=std::string::npos,"EML hook service required");BE_HookChainApiV1 hooks{sizeof(hooks),1,nullptr,Hook,Disable,nullptr};supplied.hooks=&hooks;hookResult=BE_Result_Conflict;check(Connect()==BE_Result_Conflict&&hook==0&&next==nullptr,"hook conflict is recoverable");hookResult=BE_Result_Ok;emptyHook=true;check(Connect()==BE_Result_ContractMismatch&&hook==0,"empty hook success rejected");emptyHook=false;
check(Connect()==BE_Result_Ok&&hook&&next&&leases==EaiPose::GetApi()&&poseContracts&&lookContracts&&standingContracts,"EML runtime connects with internal ownership and no BEM DLL");auto count=created;check(Connect()==BE_Result_Ok&&created==count,"reconnect does not duplicate hooks");hook=0;missing=Moving;hookResult=BE_Result_Conflict;check(Connect()==BE_Result_Conflict&&!movementContracts&&!methods[Moving].resolved.method_info,"retry clears stale optional movement contract");FreeCameraInput::Stop();std::cout<<(failures?"FAIL":"PASS")<<": EML V1 game entry contract and error diagnostics (not live game verification)\n";return failures?1:0;}
