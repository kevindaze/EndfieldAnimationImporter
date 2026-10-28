// Visual-only prototype. Never creates gameplay entities or changes the squad.
struct SpawnCharacterState {
 bool selectAfterSpawn=true;int request=0;std::unique_ptr<Pins> pins;void* object=nullptr;void* main=nullptr;void* mainModel=nullptr;
 std::string selected="chr_0036_jsspsi",identity,status="尚未生成角色";
} spawnedCharacter;
struct SpawnContracts {
 BE_ResolvedMethodV1 ready{},hash{},load{},get{},clone{},destroy{},setActive{};
 BE_ResolvedClassV1 gameObject{};bool resolved=false;
} spawnContracts;
bool ResolveSpawnContracts(){
 if(spawnContracts.resolved)return true;
 if(!runtime||!runtime->string_new)return false;
 auto resolve=[](BE_ResolvedMethodV1& out,const char* assembly,const char* space,const char* type,const char* method,const char* args,const char* result,int count){BE_MethodDescriptorV1 spec{assembly,space,type,method,args,result,static_cast<uint32_t>(count)};return runtime->resolve_method(runtime->context,&spec,&out)==BE_Result_Ok&&out.method_info&&out.method_pointer;};
 auto& c=spawnContracts;
 bool ok=resolve(c.ready,"Common.Beyond.dll","Beyond.Resource","ResourceManager","IsInitialized",nullptr,"System.Boolean",0)
 &&resolve(c.hash,"Common.Beyond.dll","Beyond.Resource","StringPathHash","CreateStringPathHash","System.String","Beyond.Resource.StringPathHash",1)
 &&resolve(c.load,"Common.Beyond.dll","Beyond.Resource","I18NAssetLoader","Load","Beyond.Resource.StringPathHash|System.Type|Beyond.Resource.RootCategory","Beyond.Resource.FAssetProxyHandle",3)
 &&resolve(c.get,"Common.Beyond.dll","Beyond.Resource","FAssetProxyHandle","Get",nullptr,"UnityEngine.Object",0)
 &&resolve(c.clone,core,"UnityEngine","Object","Instantiate","UnityEngine.Object","UnityEngine.Object",1)
 &&resolve(c.destroy,core,"UnityEngine","Object","Destroy","UnityEngine.Object","System.Void",1)
 &&resolve(c.setActive,core,"UnityEngine","GameObject","SetActive","System.Boolean","System.Void",1)
 &&runtime->resolve_class(runtime->context,core,"UnityEngine","GameObject",&c.gameObject)==BE_Result_Ok&&c.gameObject.type_object;
 c.resolved=ok;return ok;
}
void* SpawnInvoke(const BE_ResolvedMethodV1& method,void* self=nullptr,void** args=nullptr,bool* success=nullptr){
 if(success)*success=false;void* exception=nullptr;auto value=InvokeGuarded(method.method_info,self,args,&exception);
 if(exception){Log("Spawn invocation failed: managed exception");return nullptr;}if(success)*success=true;return value;
}
bool SpawnableCharacter(const std::string& id){return operatorWhitelist.Allows(id);}
void* GeneratedPartner(const std::string& identity){return !identity.empty()&&identity==spawnedCharacter.identity&&spawnedCharacter.object&&Living(spawnedCharacter.object)?spawnedCharacter.object:nullptr;}
// Generated visuals have no gameplay Entity. Never pass their GameObject to Entity APIs.
template<class Visit> void EachPartner(Pins& pins,Visit visit){
 int count=0;auto manager=Squad(pins,count);auto main=Call(Main);auto generated=GeneratedPartner(spawnedCharacter.identity);
 for(int i=0;manager&&i<count;++i){void* args[]{&i};auto entity=pins.Keep(Call(Member,manager,args));if(!entity||entity==main)continue;
  auto model=pins.Keep(Call(Model,entity));auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;if(!Living(go))continue;
  if(generated&&Presets::Identity(ObjectName(go))==spawnedCharacter.identity)continue;visit(entity,go,i);
 }
 if(generated)visit(nullptr,pins.Keep(generated),1000);
}
bool RemoveSpawnedCharacter(){
 if(pose.pins&&pose.targetModel==spawnedCharacter.object)StopPose("generated partner removed",true);
 if(spawnedCharacter.object)ClearBreast();
 if(spawnedCharacter.object&&Living(spawnedCharacter.object)){
  bool off=false,ok=false;void* disable[]{&off};SpawnInvoke(spawnContracts.setActive,spawnedCharacter.object,disable);
  void* args[]{spawnedCharacter.object};SpawnInvoke(spawnContracts.destroy,nullptr,args,&ok);
  if(!ok){spawnedCharacter.status="生成角色移除失敗，請重新進入場景";return false;}
 }
 spawnedCharacter.pins.reset();spawnedCharacter.object=spawnedCharacter.main=spawnedCharacter.mainModel=nullptr;spawnedCharacter.identity.clear();targetsPending=true;boneCatalog.clear();++boneGeneration;
 return true;
}
void PumpSpawnCharacter(){
 if(spawnedCharacter.object){auto main=Call(Main);auto model=main?Call(Model,main):nullptr;auto go=model?Call(ModelGo,model):nullptr;
  if(!Living(spawnedCharacter.object)||main!=spawnedCharacter.main||go!=spawnedCharacter.mainModel){if(!RemoveSpawnedCharacter())return;spawnedCharacter.status="場景或主控已變更，生成角色已移除";}}
 int request=spawnedCharacter.request;spawnedCharacter.request=0;if(!request)return;
 if(request==2){if(RemoveSpawnedCharacter())spawnedCharacter.status="生成角色已移除";return;}
 if(spawnedCharacter.object){if(spawnedCharacter.identity==spawnedCharacter.selected){spawnedCharacter.status="角色已生成；請先移除再重新生成";return;}if(!RemoveSpawnedCharacter())return;}
 if(!SpawnableCharacter(spawnedCharacter.selected)){spawnedCharacter.status="請選擇有效幹員";return;}
 if(!ResolveSpawnContracts()){spawnedCharacter.status="角色生成接口不可用，請查看日誌";return;}
 Pins local;auto ready=SpawnInvoke(spawnContracts.ready);auto readyData=ready?runtime->object_unbox(runtime->context,ready):nullptr;
 if(!readyData||!*static_cast<bool*>(readyData)){spawnedCharacter.status="遊戲角色資源尚未就緒";return;}
 auto main=local.Keep(Call(Main));auto model=main?local.Keep(Call(Model,main)):nullptr;auto go=model?local.Keep(Call(ModelGo,model)):nullptr;auto transform=Living(go)?local.Keep(Call(GoTransform,go)):nullptr;
 PoseMath::Vector3 position{};PoseMath::Quaternion rotation{};
 if(!transform||!Value(Position,transform,position)||!Value(WorldRotation,transform,rotation)||!PoseMath::Normalize(rotation)){spawnedCharacter.status="請先進入可操作的遊戲場景";return;}
 // Conventional postmodel path; the runtime index and returned asset name are authoritative.
 auto resourcePath="assets/beyond/dynamicassets/gameplay/actors/postmodels/characters/"+spawnedCharacter.selected+"_postmodel.prefab";
 auto path=local.Keep(runtime->string_new(runtime->context,resourcePath.c_str()));
 void* hashArgs[]{path};auto boxedHash=local.Keep(SpawnInvoke(spawnContracts.hash,nullptr,hashArgs));auto hash=boxedHash?runtime->object_unbox(runtime->context,boxedHash):nullptr;
 if(!hash){spawnedCharacter.status="找不到所選幹員的模型資源索引";return;}
 uint8_t category=0;void* loadArgs[]{hash,spawnContracts.gameObject.type_object,&category};auto boxedHandle=local.Keep(SpawnInvoke(spawnContracts.load,nullptr,loadArgs));auto handle=boxedHandle?runtime->object_unbox(runtime->context,boxedHandle):nullptr;
 auto prefab=handle?local.Keep(SpawnInvoke(spawnContracts.get,handle)):nullptr;
 if(!Living(prefab)||!ObjectName(prefab).starts_with(spawnedCharacter.selected+"_postmodel")){spawnedCharacter.status="幹員模型載入失敗；目前客戶端可能未包含此資源";return;}
 void* cloneArgs[]{prefab};auto object=local.Keep(SpawnInvoke(spawnContracts.clone,nullptr,cloneArgs));
 if(!Living(object)){spawnedCharacter.status="幹員模型建立失敗";return;}
 spawnedCharacter.pins=std::make_unique<Pins>();spawnedCharacter.object=spawnedCharacter.pins->Keep(object);spawnedCharacter.main=spawnedCharacter.pins->Keep(main);spawnedCharacter.mainModel=spawnedCharacter.pins->Keep(go);
 // Keep the prefab and resource handle alive for the lifetime of the visual instance.
 spawnedCharacter.pins->Keep(prefab);spawnedCharacter.pins->Keep(boxedHandle);
 bool off=false,on=true,ok=false;void* disable[]{&off};SpawnInvoke(spawnContracts.setActive,object,disable);
 auto output=local.Keep(Call(GoTransform,object));position=Handshake::Add(position,PoseMath::Rotate(rotation,{0,0,2.f}));
 if(!spawnedCharacter.object||!output||!WriteValue(SetPosition,output,position)||!WriteValue(SetWorldRotation,output,PoseMath::Multiply(rotation,PoseMath::YawPitch(180,0)))){RemoveSpawnedCharacter();spawnedCharacter.status="生成角色定位失敗";return;}
 void* enable[]{&on};SpawnInvoke(spawnContracts.setActive,object,enable,&ok);
 if(!ok){RemoveSpawnedCharacter();spawnedCharacter.status="生成角色顯示失敗";return;}
 spawnedCharacter.identity=spawnedCharacter.selected;if(spawnedCharacter.selectAfterSpawn){workflow.draft.target=spawnedCharacter.identity;selectedIdentity=spawnedCharacter.identity;selectedSlot=-1;selectedModel.clear();boneTarget.Reset(1);workflow.draft.bones=boneTarget;boneCatalog.clear();++boneGeneration;workflow.Invalidate();pendingRoleSelection=spawnedCharacter.identity;}targetsPending=true;pairedUiPending=true;
 spawnedCharacter.status="已生成並選為互動角色："+CharacterNames::Name(spawnedCharacter.identity);Log("Spawn character: "+spawnedCharacter.identity+" visual instance created");
}
Presets::Json SpawnCharacterJson(){auto characters=Presets::Json::array();for(auto& [id,name]:CharacterNames::All())if(SpawnableCharacter(id))characters.push_back({{"id",id},{"name",name}});return {{"spawned",spawnedCharacter.object!=nullptr},{"status",spawnedCharacter.status},{"character",spawnedCharacter.selected},{"identity",spawnedCharacter.identity},{"characters",characters}};}

// Called only by the gameplay-thread playback/selection pipeline.
bool EnsureInteractionPartner(const std::string& target){
 int matches=0;{Pins pins;EachPartner(pins,[&](void*,void* model,int){if(Presets::Identity(ObjectName(model))==target)++matches;});}
 if(matches==1)return true;if(matches>1){panelStatus="互動角色不唯一，請重新選擇";return false;}
 if(!SpawnableCharacter(target)){panelStatus="此角色不在生成角色白名單中";return false;}
 auto selection=spawnedCharacter.selected;spawnedCharacter.selected=target;spawnedCharacter.selectAfterSpawn=false;spawnedCharacter.request=1;PumpSpawnCharacter();spawnedCharacter.selectAfterSpawn=true;spawnedCharacter.selected=selection;
 if(!GeneratedPartner(target)){panelStatus=spawnedCharacter.status;return false;}return true;
}