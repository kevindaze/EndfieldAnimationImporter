// EAI computes after its animation/calibration layer. Animation callbacks only
// replay the cached absolute target; they never integrate or multiply it again.
struct BreastHookState {uint64_t handles[3]{};Tick next[3]{};unsigned long long calls[3]{},matched=0;bool ready=false;std::string error;BE_ResolvedFieldV1 animatorField{};} breastHooks;
void ReplayBreastTarget(){
 if(!breast.enabled||breast.reset||!breast.cached||!breast.root||GetTickCount64()-breast.cachedAt>150)return;
 auto context=BreastContext();
 if(!leases||!(context.borrowed?leases->owns(breast.root,id,context.lease):breast.owned&&leases->owns(breast.root,id,breast.lease)))return;
 if(!Living(breast.right)||!Living(breast.left))return;
 // Capture a fresh native/calibration base for removal, but keep the same
 // absolute output across all callbacks within the verification window.
 PoseMath::Quaternion q{};if(Rotation(breast.right,q)&&!PoseMath::SameRotation(q,breast.writtenR))breast.baseR=q;
 if(Rotation(breast.left,q)&&!PoseMath::SameRotation(q,breast.writtenL))breast.baseL=q;
 bool r=WriteRotation(breast.right,breast.writtenR);breast.wrote=r;
 if(!r||!WriteRotation(breast.left,breast.writtenL)){RemoveBreastLayer();breast.cached=false;breast.status="胸部物理重播失敗";return;}
 breast.wrote=true;++breast.replays;
}
bool BreastAnimatorMatches(void* instance){
 if(!instance||!breast.root||!breastHooks.animatorField.field_info)return false;
 Pins pins;auto animatorObject=pins.Keep(runtime->field_get_value_object(runtime->context,breastHooks.animatorField.field_info,instance));
 auto transform=animatorObject?pins.Keep(Call(ComponentTransform,animatorObject)):nullptr;
 for(int i=0;transform&&i<64;++i){if(transform==breast.root)return true;transform=pins.Keep(Call(Parent,transform));}return false;
}
template<int Stage> void __fastcall BreastAnimationDetour(void* instance,float dt,void* method){
 if(breastHooks.next[Stage])breastHooks.next[Stage](instance,dt,method);
 if(!active.load())return;
 std::unique_lock lock(gate,std::try_to_lock);if(!lock.owns_lock()||!active.load()||!unityThread||unityThread!=GetCurrentThreadId())return;
 ++breastHooks.calls[Stage];if(!breast.enabled||breast.reset||!breast.cached)return;if constexpr(Stage==0){if(!BreastAnimatorMatches(instance))return;++breastHooks.matched;}
 ReplayBreastTarget();
}
void StopBreastHooks(){if(host&&host->hooks)for(auto& handle:breastHooks.handles){if(handle)host->hooks->disable(host->hooks->context,handle);handle=0;}breastHooks.ready=false;}
bool InstallBreastHooks(){
 if(breastHooks.ready)return true;StopBreastHooks();breastHooks=BreastHookState{};
 if(!runtime||!host||!host->hooks){breastHooks.error="胸部物理動畫入口尚未就緒";return false;}
 const char* assemblies[]{game,"NPC.Beyond.dll","Beyond.NPC.dll","Gameplay.NPC.dll","Assembly-CSharp.dll"};
 const char* spaces[]{"Beyond.Gameplay.View.Animation","Beyond.NPC.Animation","Beyond.NPC.Animation"};
 const char* classes[]{"AnimatorMono","NPCCPUAnimator","ScriptAnimationJobSyncMono"};
 const char* names[]{"PreLateTick","LateTick","CalcLayerMainStream"};
 void* detours[]{reinterpret_cast<void*>(&BreastAnimationDetour<0>),reinterpret_cast<void*>(&BreastAnimationDetour<1>),reinterpret_cast<void*>(&BreastAnimationDetour<2>)};
 for(int i=0;i<3;++i){bool installed=false;for(auto assembly:assemblies){
   BE_MethodDescriptorV1 spec{assembly,spaces[i],classes[i],names[i],"System.Single","System.Void",1};BE_ResolvedMethodV1 resolved{};
   if(runtime->resolve_method(runtime->context,&spec,&resolved)!=BE_Result_Ok||!resolved.method_pointer)continue;
   if(i==0){BE_FieldDescriptorV1 field{assembly,spaces[i],classes[i],"m_animator","UnityEngine.Animator"};if(runtime->resolve_field(runtime->context,&field,&breastHooks.animatorField)!=BE_Result_Ok||!breastHooks.animatorField.field_info)continue;}
   auto result=host->hooks->create(host->hooks->context,id,resolved.method_pointer,detours[i],reinterpret_cast<void**>(&breastHooks.next[i]),&breastHooks.handles[i]);
   installed=result==BE_Result_Ok&&breastHooks.handles[i]&&breastHooks.next[i];if(!installed){if(breastHooks.handles[i])host->hooks->disable(host->hooks->context,breastHooks.handles[i]);breastHooks.handles[i]=0;breastHooks.next[i]=nullptr;}
   Log(std::string("Breast animation hook ")+classes[i]+"."+names[i]+"="+(installed?"ready":"failed")+" assembly="+assembly+" result="+std::to_string(int(result)));break;
  }
  if(!installed){Log(std::string("Breast animation contract unavailable: ")+classes[i]+"."+names[i]);if(i==0){breastHooks.error="胸部物理缺少 PreLateTick 動畫入口；未啟用";StopBreastHooks();return false;}}
 }
 breastHooks.ready=true;return true;
}
