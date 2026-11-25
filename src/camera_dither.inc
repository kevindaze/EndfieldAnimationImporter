// Resolve independently of the animation connection. The original camera tick
// still runs before clearing the fade it just applied.
using CameraDitherTick=void(__fastcall*)(void*,void*);
struct CameraDitherState {
 const BE_HostApiV1* api=nullptr;
 const void* clearMethod=nullptr;
 CameraDitherTick next=nullptr;
 uint64_t handle=0;
 ULONGLONG retryAt=0;
 bool requested=false,reportedMissing=false,reportedFault=false;
 std::atomic_bool enabled{false},fault{false};
} cameraDither;

bool ClearCameraDither(const BE_HostApiV1* api,const void* method,void* camera){
 void* exception=nullptr;
 __try{api->runtime_invoke(api->context,method,camera,nullptr,&exception);}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
 return !exception;
}

void __fastcall CameraDitherDetour(void* camera,void* method){
 if(cameraDither.next)cameraDither.next(camera,method);
 if(!camera||!cameraDither.enabled.load(std::memory_order_acquire)||cameraDither.fault.load())return;
 if(!ClearCameraDither(cameraDither.api,cameraDither.clearMethod,camera))
  cameraDither.fault=true;
}

void PumpCameraDither(){
 if(!cameraDither.requested||!host)return;
 if(cameraDither.fault&&!cameraDither.reportedFault){
  cameraDither.reportedFault=true;
  Log("Near-camera fade: ForceClearDither failed; original camera behavior retained");
 }
 if(cameraDither.handle)return;
 const auto now=GetTickCount64();
 if(now<cameraDither.retryAt)return;
 cameraDither.retryAt=now+3000;
 const auto* api=host->get_runtime?host->get_runtime(host->context):host->runtime;
 const auto* hooks=host->hooks;
 if(!api||api->abi_version!=1||!api->resolve_method||!api->runtime_invoke||
    !hooks||hooks->version!=1||hooks->struct_size<sizeof(*hooks)||!hooks->create||!hooks->disable)return;

 const BE_MethodDescriptorV1 tick{game,"Beyond.Gameplay.View","CameraMono","_ProcessDitherByPitch",nullptr,"System.Void",0};
 const BE_MethodDescriptorV1 clear{game,"Beyond.Gameplay.View","CameraMono","ForceClearDither",nullptr,"System.Void",0};
 BE_ResolvedMethodV1 dither{},remove{};
 if(api->resolve_method(api->context,&tick,&dither)!=BE_Result_Ok||!dither.method_pointer||
    api->resolve_method(api->context,&clear,&remove)!=BE_Result_Ok||!remove.method_info){
  if(!cameraDither.reportedMissing){
   cameraDither.reportedMissing=true;
   Log("Near-camera fade: CameraMono methods not ready; will retry");
  }
  return;
 }
 cameraDither.api=api;
 cameraDither.clearMethod=remove.method_info;
 const auto result=hooks->create(hooks->context,id,dither.method_pointer,
     reinterpret_cast<void*>(&CameraDitherDetour),reinterpret_cast<void**>(&cameraDither.next),&cameraDither.handle);
 if(result!=BE_Result_Ok||!cameraDither.handle||!cameraDither.next){
  if(cameraDither.handle)hooks->disable(hooks->context,cameraDither.handle);
  cameraDither.handle=0;
  if(!cameraDither.reportedMissing){
   cameraDither.reportedMissing=true;
   Log("Near-camera fade: CameraMono hook unavailable ("+std::to_string(int(result))+"); will retry");
  }
  return;
 }
 cameraDither.fault=false;
 cameraDither.enabled.store(true,std::memory_order_release);
 Log("Near-camera fade: CameraMono connected; suppression enabled");
}

void StopCameraDither(){
 cameraDither.requested=false;
 cameraDither.enabled.store(false,std::memory_order_release);
 if(cameraDither.handle&&host&&host->hooks)
  host->hooks->disable(host->hooks->context,cameraDither.handle);
 cameraDither.handle=0;
}
