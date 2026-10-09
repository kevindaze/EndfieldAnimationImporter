// Unity calls below run exclusively in the existing camera-tail callback.
struct FreeCameraState {
 std::unique_ptr<Pins> pins;void* camera=nullptr;void* transform=nullptr;void* entity=nullptr;
 FreeCamera::View view;PoseMath::Vector3 savedPosition{};PoseMath::Quaternion savedRotation{};
 PoseMath::Vector3 writtenPosition{};PoseMath::Quaternion writtenRotation{};
 void* inputManager=nullptr;int inputGroup=0;bool inputWasEnabled=false;
 struct HiddenCanvas{void* canvas;bool enabled;};std::vector<HiddenCanvas> hiddenCanvases;std::unique_ptr<Pins> uiPins;bool hideUi=false;ULONGLONG uiScanAt=0;
 bool enabled=false,wrote=false;int request=-1;bool reset=false;std::string status="自由相機已關閉";
} freeCamera;
struct CameraInputHook {void* next=nullptr;void* target=nullptr;int kind=0;uint64_t handle=0;};
std::array<CameraInputHook,64> cameraInputHooks{};size_t cameraInputHookCount=0;bool cameraInputReady=false;
using CameraPush=void(__fastcall*)(void*,void*,void*);
CameraPush cameraPushNext=nullptr;uint64_t cameraPushHook=0;bool cameraPushReady=false;
bool ObserveGameCamera();void ApplyFreeCameraView();
void __fastcall CameraPushDetour(void* self,void* state,void* method){
 if(cameraPushNext)cameraPushNext(self,state,method);
 if(!active)return;std::lock_guard lock(gate);
 if(!freeCamera.enabled||GetCurrentThreadId()!=unityThread)return;
 if(Call(CameraMain)==freeCamera.camera&&Living(freeCamera.transform)&&ObserveGameCamera())ApplyFreeCameraView();
}
bool InstallCameraOutput(){
 if(cameraPushReady)return true;if(!host||!host->hooks||!runtime)return false;
 BE_MethodDescriptorV1 spec{"Cinemachine.dll","Cinemachine","CinemachineBrain","PushStateToUnityCamera","Cinemachine.CameraState&","System.Void",1};BE_ResolvedMethodV1 found{};
 if(runtime->resolve_method(runtime->context,&spec,&found)!=BE_Result_Ok||!found.method_pointer){Log("Free camera final output contract missing");return false;}
 if(host->hooks->create(host->hooks->context,id,found.method_pointer,reinterpret_cast<void*>(&CameraPushDetour),reinterpret_cast<void**>(&cameraPushNext),&cameraPushHook)!=BE_Result_Ok){Log("Free camera final output hook failed");return false;}
 cameraPushReady=true;Log("Free camera final output hook connected");return true;
}
std::atomic<void*> cameraInputManager{nullptr};
bool CameraGameInputBlocked(){return FreeCameraInput::blocked||FreeCameraInput::toggleHeld||FreeCameraInput::toggleRequests.load()>0;}
template<size_t N> struct CameraGate {
 static void __fastcall Check(void* self,void* method){if(self)cameraInputManager=self;if(!CameraGameInputBlocked()&&cameraInputHooks[N].next)reinterpret_cast<void(__fastcall*)(void*,void*)>(cameraInputHooks[N].next)(self,method);}
 static bool __fastcall BoolInstance(void* self,uintptr_t key,void* method){return !CameraGameInputBlocked()&&cameraInputHooks[N].next&&reinterpret_cast<bool(__fastcall*)(void*,uintptr_t,void*)>(cameraInputHooks[N].next)(self,key,method);}
 static bool __fastcall BoolStatic(uintptr_t key,void* method){return !CameraGameInputBlocked()&&cameraInputHooks[N].next&&reinterpret_cast<bool(__fastcall*)(uintptr_t,void*)>(cameraInputHooks[N].next)(key,method);}
 static float __fastcall AxisInstance(void* self,uintptr_t key,void* method){return CameraGameInputBlocked()||!cameraInputHooks[N].next?0.f:reinterpret_cast<float(__fastcall*)(void*,uintptr_t,void*)>(cameraInputHooks[N].next)(self,key,method);}
 static float __fastcall AxisStatic(uintptr_t key,void* method){return CameraGameInputBlocked()||!cameraInputHooks[N].next?0.f:reinterpret_cast<float(__fastcall*)(uintptr_t,void*)>(cameraInputHooks[N].next)(key,method);}
};
enum class CameraGateKind {Check,BoolInstance,BoolStatic,AxisInstance,AxisStatic};
template<size_t... N> auto CameraGateFunctions(CameraGateKind kind,std::index_sequence<N...>){return std::array<void*,sizeof...(N)>{(kind==CameraGateKind::Check?reinterpret_cast<void*>(&CameraGate<N>::Check):kind==CameraGateKind::BoolInstance?reinterpret_cast<void*>(&CameraGate<N>::BoolInstance):kind==CameraGateKind::BoolStatic?reinterpret_cast<void*>(&CameraGate<N>::BoolStatic):kind==CameraGateKind::AxisInstance?reinterpret_cast<void*>(&CameraGate<N>::AxisInstance):reinterpret_cast<void*>(&CameraGate<N>::AxisStatic))...};}
bool InstallCameraInput(){
 if(cameraInputReady)return true;if(!host||!host->hooks||!runtime)return false;
 auto add=[](const char* assembly,const char* space,const char* klass,const char* name,const char* argument,CameraGateKind kind,bool required){
  BE_MethodDescriptorV1 spec{assembly,space,klass,name,argument,kind==CameraGateKind::Check?"System.Void":kind==CameraGateKind::AxisInstance||kind==CameraGateKind::AxisStatic?"System.Single":"System.Boolean",kind==CameraGateKind::Check?0u:1u};BE_ResolvedMethodV1 found{};
  if(runtime->resolve_method(runtime->context,&spec,&found)!=BE_Result_Ok||!found.method_pointer){if(required)Log(std::string("Free camera input contract missing: ")+klass+"."+name);return !required;}
  // Wrapper methods can share a native entry. Reuse only identical ABI targets.
  for(size_t i=0;i<cameraInputHookCount;++i)if(cameraInputHooks[i].target==found.method_pointer)return cameraInputHooks[i].kind==int(kind);
  if(cameraInputHookCount>=cameraInputHooks.size())return false;auto functions=CameraGateFunctions(kind,std::make_index_sequence<64>{});auto& slot=cameraInputHooks[cameraInputHookCount];
  auto result=host->hooks->create(host->hooks->context,id,found.method_pointer,functions[cameraInputHookCount],&slot.next,&slot.handle);
  if(result!=BE_Result_Ok){Log(std::string("Free camera input hook failed: ")+klass+"."+name+" result="+std::to_string(int(result)));return false;}slot.target=found.method_pointer;slot.kind=int(kind);++cameraInputHookCount;return true;
 };
 bool ok=add("Input.Beyond.dll","Beyond.Input","InputManager","_CheckInput",nullptr,CameraGateKind::Check,true);
 for(auto name:{"GetKey","GetKeyOrDown","GetKeyDown","GetKeyUp"})ok=add("Input.Beyond.dll","Beyond.Input","InputManager",name,"Beyond.Input.KeyboardKeyCode",CameraGateKind::BoolInstance,true)&&ok;
 for(auto name:{"GetKey","GetKeyDown","GetKeyUp"})ok=add("Input.Beyond.dll","Beyond.Input","InputManager",name,"Beyond.Input.GamepadKeyCode",CameraGateKind::BoolInstance,true)&&ok;
 ok=add("Input.Beyond.dll","Beyond.Input","InputManager","GetAxis","System.String",CameraGateKind::AxisInstance,true)&&ok;
 for(auto name:{"GetKey","GetKeyDown","GetKeyUp"})ok=add("Input.Beyond.dll","Beyond.Input","KeyboardHandler",name,"Beyond.Input.KeyboardKeyCode",CameraGateKind::BoolStatic,true)&&ok;
 for(auto argument:{"System.String","System.Int32"}){
  for(auto name:{"GetButton","GetButtonDown","GetButtonUp"})ok=add("Rewired_Core.dll","Rewired","Player",name,argument,CameraGateKind::BoolInstance,true)&&ok;
  for(auto name:{"GetAxis","GetAxisRaw"})ok=add("Rewired_Core.dll","Rewired","Player",name,argument,CameraGateKind::AxisInstance,true)&&ok;
 }
 for(auto name:{"GetKeyInt","GetKeyDownInt","GetKeyUpInt"})ok=add("UnityEngine.InputLegacyModule.dll","UnityEngine","Input",name,"UnityEngine.KeyCode",CameraGateKind::BoolStatic,false)&&ok;
 for(auto name:{"GetMouseButton","GetMouseButtonDown","GetMouseButtonUp"})ok=add("UnityEngine.InputLegacyModule.dll","UnityEngine","Input",name,"System.Int32",CameraGateKind::BoolStatic,false)&&ok;
 for(auto name:{"GetAxis","GetAxisRaw"})ok=add("UnityEngine.InputLegacyModule.dll","UnityEngine","Input",name,"System.String",CameraGateKind::AxisStatic,false)&&ok;
 if(!ok){for(size_t i=0;i<cameraInputHookCount;++i)host->hooks->disable(host->hooks->context,cameraInputHooks[i].handle);cameraInputHookCount=0;return false;}
 cameraInputReady=true;return true;
}
void RestoreCameraUi(){
 for(auto& item:freeCamera.hiddenCanvases)if(Living(item.canvas))WriteValue(SetUiEnabled,item.canvas,item.enabled);
 freeCamera.hiddenCanvases.clear();freeCamera.uiPins.reset();freeCamera.uiScanAt=0;
}
void PumpCameraUi(){
 if(!freeCamera.hideUi){if(freeCamera.uiPins)RestoreCameraUi();return;}
 auto now=GetTickCount64();if(freeCamera.uiScanAt&&now-freeCamera.uiScanAt<250)return;freeCamera.uiScanAt=now;
 if(!uiCanvas.type_object){Log("Free camera: Canvas contract unavailable; UI remains visible");return;}
 void* args[]{uiCanvas.type_object};auto array=Call(FindCanvases,nullptr,args);int count=0;int dimension=0;void* lengthArgs[]{&dimension};
 if(!array||!Value(Length,array,count,lengthArgs)||count<0||count>4096)return;
 if(!freeCamera.uiPins)freeCamera.uiPins=std::make_unique<Pins>();
 for(int i=0;i<count;++i){void* indexArgs[]{&i};auto canvas=Call(Item,array,indexArgs);if(!Living(canvas))continue;
 auto found=std::find_if(freeCamera.hiddenCanvases.begin(),freeCamera.hiddenCanvases.end(),[&](auto& item){return item.canvas==canvas;});
 if(found==freeCamera.hiddenCanvases.end()){bool enabled=false;if(!Value(UiEnabled,canvas,enabled))continue;auto pinned=freeCamera.uiPins->Keep(canvas);if(!pinned)continue;freeCamera.hiddenCanvases.push_back({pinned,enabled});}
 WriteValue(SetUiEnabled,canvas,false);
 }
}
void StopFreeCamera(const char* status="自由相機已關閉"){
 RestoreCameraUi();
 if(freeCamera.inputManager){int group=0;if(Value(InputRootGroup,freeCamera.inputManager,group)&&group==freeCamera.inputGroup)WriteValue(InputToggleAll,freeCamera.inputManager,freeCamera.inputWasEnabled);freeCamera.inputManager=nullptr;}
 if(freeCamera.enabled&&Living(freeCamera.transform)){WriteValue(SetPosition,freeCamera.transform,freeCamera.savedPosition);WriteValue(SetWorldRotation,freeCamera.transform,freeCamera.savedRotation);}
 freeCamera.enabled=false;FreeCameraInput::enabled=false;freeCamera.pins.reset();freeCamera.camera=freeCamera.transform=freeCamera.entity=nullptr;freeCamera.status=status;freeCamera.reset=false;restored.notify_all();
 // Keep game input suppressed until keys held during camera mode are released.
}
bool BeginFreeCamera(){
 if(!hook){freeCamera.status="請先手動連接遊戲入口";return false;}
 if(!FreeCameraInput::ready||!InstallCameraInput()||!InstallCameraOutput()){freeCamera.status="自由相機輸入攔截未接通，未啟用";Log("Free camera activation rejected: required input/output hook unavailable");return false;}
 auto pins=std::make_unique<Pins>();auto camera=pins->Keep(Call(CameraMain));auto transform=camera?pins->Keep(Call(ComponentTransform,camera)):nullptr;PoseMath::Vector3 position{};PoseMath::Quaternion rotation{};
 if(!Living(camera)||!Living(transform)||!Value(Position,transform,position)||!Value(WorldRotation,transform,rotation)||!freeCamera.view.Capture(position,rotation)){freeCamera.status="找不到可控制的遊戲相機，未啟用";return false;}
 auto input=pins->Keep(cameraInputManager.load());int group=0;bool wasEnabled=false;void* arguments[]{&group};
 if(!input||!Value(InputRootGroup,input,group)||!Value(InputGroupEnabled,input,wasEnabled,arguments)||!WriteValue(InputToggleAll,input,false)){freeCamera.status="自由相機輸入攔截未接通，未啟用";Log("Free camera activation rejected: game input root group unavailable");return false;}
 bool stillEnabled=true;if(!Value(InputGroupEnabled,input,stillEnabled,arguments)||stillEnabled){WriteValue(InputToggleAll,input,wasEnabled);freeCamera.status="自由相機輸入攔截未接通，未啟用";Log("Free camera activation rejected: input group disable not confirmed");return false;}
 freeCamera.inputManager=input;freeCamera.inputGroup=group;freeCamera.inputWasEnabled=wasEnabled;
 freeCamera.camera=camera;freeCamera.transform=transform;freeCamera.entity=pins->Keep(Call(Main));freeCamera.savedPosition=position;freeCamera.savedRotation=rotation;freeCamera.pins=std::move(pins);
 for(int i=1;i<256;++i)if(i!=VK_SHIFT&&i!=VK_CONTROL&&i!=VK_MENU&&(GetAsyncKeyState(i)&0x8000))FreeCameraInput::keys[i]=true;
 FreeCameraInput::mouseX=FreeCameraInput::mouseY=0;FreeCameraInput::focusLost=false;FreeCameraInput::blocked=true;FreeCameraInput::enabled=true;freeCamera.enabled=true;freeCamera.wrote=false;PumpCameraUi();freeCamera.status="自由相機已啟用；遊戲操作已攔截";presetKeys.fill(true);Log("Free camera enabled; raw input and final camera output connected");return true;
}
bool ResetFreeCamera(){
 auto entity=Call(Main);auto model=entity?Call(Model,entity):nullptr;auto go=model?Call(ModelGo,model):nullptr;auto root=go?Call(GoTransform,go):nullptr;PoseMath::Vector3 position{};PoseMath::Quaternion rotation{};
 if(!Living(root)||!Value(Position,root,position)||!Value(WorldRotation,root,rotation))return false;
 auto offset=PoseMath::Rotate(rotation,{0,1.6f,-3.f});position={position.x+offset.x,position.y+offset.y,position.z+offset.z};return freeCamera.view.Capture(position,rotation);
}
bool ObserveGameCamera(){
 PoseMath::Vector3 position{};PoseMath::Quaternion rotation{};
 if(!Value(Position,freeCamera.transform,position)||!Value(WorldRotation,freeCamera.transform,rotation)||!PoseMath::Finite(position)||!PoseMath::Normalize(rotation))return false;
 if(!freeCamera.wrote||!PoseMath::SamePosition(position,freeCamera.writtenPosition))freeCamera.savedPosition=position;
 if(!freeCamera.wrote||!PoseMath::SameRotation(rotation,freeCamera.writtenRotation))freeCamera.savedRotation=rotation;return true;
}
void ApplyFreeCameraView(){
 if(!WriteValue(SetPosition,freeCamera.transform,freeCamera.view.position)||!WriteValue(SetWorldRotation,freeCamera.transform,freeCamera.view.Rotation()))StopFreeCamera("自由相機寫入失敗，已關閉");else{freeCamera.wrote=true;freeCamera.writtenPosition=freeCamera.view.position;freeCamera.writtenRotation=freeCamera.view.Rotation();}
}
void PumpFreeCamera(float dt){
 int toggles=FreeCameraInput::toggleRequests.exchange(0);if(toggles%2)freeCamera.request=freeCamera.enabled?0:1;
 if(freeCamera.request==0){freeCamera.request=-1;StopFreeCamera();}
 if(freeCamera.request==1&&GameFocused()){freeCamera.request=-1;if(!freeCamera.enabled)BeginFreeCamera();}
 if(!freeCamera.enabled){if(FreeCameraInput::blocked&&FreeCameraInput::AllReleased()){FreeCameraInput::blocked=false;presetKeys.fill(true);}return;}
 if(FreeCameraInput::focusLost.exchange(false)||!GameFocused()){FreeCameraInput::ClearMotion();StopFreeCamera("自由相機已關閉：遊戲失去焦點");return;}
 if(Call(CameraMain)!=freeCamera.camera||!Living(freeCamera.transform)||Call(Main)!=freeCamera.entity){StopFreeCamera("自由相機已關閉：場景或角色已變更");return;}
 if(!WriteValue(InputToggleAll,freeCamera.inputManager,false)){StopFreeCamera("自由相機輸入攔截未接通，未啟用");return;}
 if(freeCamera.reset){freeCamera.reset=false;if(!ResetFreeCamera())freeCamera.status="無法取得角色位置";}
 PumpCameraUi();
 // Track the game's own camera output every frame so exit restores its current view.
 if(!ObserveGameCamera()){StopFreeCamera("自由相機寫入失敗，已關閉");return;}
 auto key=[](int key){return FreeCameraInput::keys[key].load()?1.f:0.f;};
 int dx=FreeCameraInput::mouseX.exchange(0),dy=FreeCameraInput::mouseY.exchange(0);
 if(!InteractionPanel::Editing())freeCamera.view.Step(dt,key('D')-key('A'),key('E')-key('Q'),key('W')-key('S'),float(dx),float(dy),(key(VK_LSHIFT)||key(VK_RSHIFT)||key(VK_SHIFT))?2.f:1.f);else FreeCameraInput::ClearMotion();
 ApplyFreeCameraView();
 static ULONGLONG lastLog=0;auto now=GetTickCount64();if(now-lastLog>=2000){lastLog=now;Log("Free camera input: keyboard="+std::to_string(FreeCameraInput::keyboardPackets.load())+" mouse="+std::to_string(FreeCameraInput::mousePackets.load())+" filtered="+std::to_string(FreeCameraInput::filteredPackets.load())+" W="+std::to_string(FreeCameraInput::keys['W'].load()));}
}
Presets::Json FreeCameraJson(){return {{"hide_ui",freeCamera.hideUi},{"enabled",freeCamera.enabled},{"pending",freeCamera.request==1},{"hotkey",FreeCameraInput::hotkey.load()},{"input_blocked",FreeCameraInput::blocked.load()},{"status",freeCamera.status}};}
