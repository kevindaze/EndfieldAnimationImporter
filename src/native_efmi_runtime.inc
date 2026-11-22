// All Unity calls run under gate on the observed Gameplay thread.
struct NativeEfmiPreview {
 std::unique_ptr<Pins> pins;
 void* actor=nullptr;
 void* renderer=nullptr;
 void* original=nullptr;
 void* mesh=nullptr;
 std::vector<float> originalWeights;
 std::optional<NativeEfmi::Profile> request;
 std::optional<NativeEfmi::Geometry> geometry;
 bool restore=false,hasShape=false,inspect=false;
 int actorIndex=0;
 float weight=0,appliedWeight=0;
 std::string status;
};
NativeEfmiPreview nativeEfmi;

void* NativeEfmiActor(int actor,Pins& pins){
 if(actor==0){
  auto entity=pins.Keep(Call(Main));
  auto model=entity?pins.Keep(Call(Model,entity)):nullptr;
  return model?pins.Keep(Call(ModelGo,model)):nullptr;
 }
 void* selected=nullptr;
 int count=0;
 EachPartner(pins,[&](void*,void* go,int){
  if(Presets::Identity(ObjectName(go))==workflow.draft.target){selected=go;++count;}
 });
 return count==1?pins.Keep(selected):nullptr;
}

bool NativeEfmiSet(Key method,void* object,void** args=nullptr){
 bool ok=false;
 Call(method,object,args,&ok);
 return ok;
}

bool RestoreNativeEfmi(){
 if(!nativeEfmi.pins)return true;
 if(Living(nativeEfmi.renderer)&&Call(SharedMesh,nativeEfmi.renderer)==nativeEfmi.mesh){
  if(!Living(nativeEfmi.original))return false;
  void* args[]{nativeEfmi.original};
  if(!NativeEfmiSet(SetSharedMesh,nativeEfmi.renderer,args))return false;
  for(int i=0;i<int(nativeEfmi.originalWeights.size());++i){
   void* weightArgs[]{&i,&nativeEfmi.originalWeights[i]};
   if(!NativeEfmiSet(SetBlendWeight,nativeEfmi.renderer,weightArgs))
    Log("Native EFMI: original blend shape weight could not be restored");
  }
 }
 if(Living(nativeEfmi.mesh)){void* args[]{nativeEfmi.mesh};NativeEfmiSet(DestroyNativeMesh,nullptr,args);}
 nativeEfmi.pins.reset();
 nativeEfmi.actor=nativeEfmi.renderer=nativeEfmi.original=nativeEfmi.mesh=nullptr;
 nativeEfmi.originalWeights.clear();
 nativeEfmi.hasShape=false;
 nativeEfmi.weight=nativeEfmi.appliedWeight=0;
 restored.notify_all();
 return true;
}

struct NativeEfmiArrays {
 void* (*objectNew)(const void*)=nullptr;
 void* (*arrayNew)(const void*,size_t)=nullptr;
 uint32_t (*arrayBytes)(void*)=nullptr;
 int32_t (*valueSize)(const void*,uint32_t*)=nullptr;
 size_t header=0;

 NativeEfmiArrays(){
  auto gameAssembly=GetModuleHandleW(L"GameAssembly.dll");
  if(!gameAssembly)throw std::runtime_error("GameAssembly unavailable");
  objectNew=reinterpret_cast<decltype(objectNew)>(GetProcAddress(gameAssembly,"il2cpp_object_new"));
  arrayNew=reinterpret_cast<decltype(arrayNew)>(GetProcAddress(gameAssembly,"il2cpp_array_new"));
  arrayBytes=reinterpret_cast<decltype(arrayBytes)>(GetProcAddress(gameAssembly,"il2cpp_array_get_byte_length"));
  valueSize=reinterpret_cast<decltype(valueSize)>(GetProcAddress(gameAssembly,"il2cpp_class_value_size"));
  auto headerSize=reinterpret_cast<size_t(*)()>(GetProcAddress(gameAssembly,"il2cpp_array_object_header_size"));
  if(!objectNew||!arrayNew||!arrayBytes||!valueSize||!headerSize)
   throw std::runtime_error("Unity array ABI unavailable");
  header=headerSize();
  if(header<16||header>128)throw std::runtime_error("Unexpected Unity array header");
 }

 const void* Class(const char* assembly,const char* space,const char* name)const{
  BE_ResolvedClassV1 result{};
  if(runtime->resolve_class(runtime->context,assembly,space,name,&result)!=BE_Result_Ok||!result.class_info)
   throw std::runtime_error(std::string("Unity class unavailable: ")+name);
  return result.class_info;
 }

 template<class T> void* Copy(Pins& pins,const void* type,const std::vector<T>& values)const{
  uint32_t alignment=0;
  if(valueSize(type,&alignment)!=sizeof(T))throw std::runtime_error("Unexpected Unity array element size");
  auto array=pins.Keep(arrayNew(type,values.size()));
  if(!array||arrayBytes(array)!=values.size()*sizeof(T))throw std::runtime_error("Unity array allocation failed");
  std::memcpy(static_cast<unsigned char*>(array)+header,values.data(),values.size()*sizeof(T));
  return array;
 }
};

struct NativeEfmiBoneWeight {float weight[4]{};int32_t index[4]{};};

void CheckNativeEfmiWeightLayout(){
 int offset=-1;
 for(int i=0;i<8;++i){
  auto field=std::string(i<4?"m_Weight":"m_BoneIndex")+std::to_string(i%4);
  BE_FieldDescriptorV1 descriptor{core,"UnityEngine","BoneWeight",field.c_str(),i<4?"System.Single":"System.Int32"};
  BE_ResolvedFieldV1 resolved{};
  if(runtime->resolve_field(runtime->context,&descriptor,&resolved)!=BE_Result_Ok||!resolved.field_info||
     (i&&resolved.offset!=offset+4*i))
   throw std::runtime_error("Unity BoneWeight layout is unsupported");
  if(!i)offset=resolved.offset;
 }
}

void* NativeEfmiRenderers(void* actor,Pins& pins,int& count){
 if(!Living(actor)||!skinnedRenderer.type_object)throw std::runtime_error("Selected actor is unavailable");
 bool inactive=true;
 void* args[]{skinnedRenderer.type_object,&inactive};
 auto renderers=pins.Keep(Call(Components,actor,args));
 int dim=0;void* lengthArgs[]{&dim};
 if(!renderers||!Value(Length,renderers,count,lengthArgs)||count<1||count>256)
  throw std::runtime_error("No supported skinned renderers on selected actor");
 return renderers;
}

void InspectNativeEfmi(){
 Pins pins;
 for(int actor=0;actor<2;++actor){
  try{
   int count=0;
   auto model=NativeEfmiActor(actor,pins);
   auto renderers=NativeEfmiRenderers(model,pins,count);
   auto character=Presets::Identity(ObjectName(model));
   for(int i=0;i<count;++i){
    void* itemArgs[]{&i};
    auto renderer=pins.Keep(Call(Item,renderers,itemArgs));
    auto bones=renderer?pins.Keep(Call(MeshBones,renderer)):nullptr;
    int dimension=0,countBones=0;void* lengthArgs[]{&dimension};
    if(!bones||!Value(Length,bones,countBones,lengthArgs)||countBones<1||countBones>1200)continue;
    std::string line="Native EFMI actor="+std::to_string(actor)+" character="+character+" renderer="+ObjectName(renderer)+" bones:";
    for(int j=0;j<countBones;++j){void* boneArgs[]{&j};auto bone=Call(Item,bones,boneArgs);if(bone)line+=" "+ObjectName(bone);}
    Log(line);
   }
  }catch(const std::exception& e){Log(std::string("Native EFMI inspection: ")+e.what());}
 }
}

void ApplyNativeEfmi(const NativeEfmi::Profile& profile,const NativeEfmi::Geometry& geometry){
 for(auto key:{SharedMesh,MeshBones,BindPoses,SetSharedMesh,MeshConstructor,MeshSetVertices,MeshSetTriangles,
               MeshSetBindPoses,MeshSetBoneWeights,MeshRecalculateNormals,MeshRecalculateBounds,MeshSubMeshCount,
               DestroyNativeMesh,MeshBlendCount,GetBlendWeight,SetBlendWeight})
  if(!methods[key].resolved.method_info)throw std::runtime_error("Native EFMI mesh API is unavailable");
 const bool hasShape=!geometry.deltas.empty();
 if(hasShape&&!methods[MeshAddBlendFrame].resolved.method_info)
  throw std::runtime_error("Unity blend shape creation API is unavailable");
 if(!geometry.uv.empty()&&!methods[MeshSetUv].resolved.method_info)
  throw std::runtime_error("Unity mesh UV API is unavailable");
 auto pins=std::make_unique<Pins>();
 auto actor=NativeEfmiActor(profile.actor,*pins);
 if(!Living(actor)||Presets::Identity(ObjectName(actor))!=profile.character)
  throw std::runtime_error("EFMI profile character does not match the selected actor");
 int count=0;
 auto renderers=NativeEfmiRenderers(actor,*pins,count);
 void* renderer=nullptr;
 for(int i=0;i<count;++i){
  void* args[]{&i};
  auto candidate=pins->Keep(Call(Item,renderers,args));
  if(!candidate||ObjectName(candidate)!=profile.renderer)continue;
  if(renderer)throw std::runtime_error("Renderer name is ambiguous");
  renderer=candidate;
 }
 if(!renderer)throw std::runtime_error("Named renderer not found");
 bool samePreview=renderer==nativeEfmi.renderer&&Call(SharedMesh,renderer)==nativeEfmi.mesh;
 auto original=pins->Keep(samePreview?nativeEfmi.original:Call(SharedMesh,renderer));
 auto bones=pins->Keep(Call(MeshBones,renderer));
 auto bindposes=original?pins->Keep(Call(BindPoses,original)):nullptr;
 int dimension=0,boneCount=0,poseCount=0,submeshes=0,blendCount=0;void* lengthArgs[]{&dimension};
 if(!bones||!bindposes||!Value(Length,bones,boneCount,lengthArgs)||!Value(Length,bindposes,poseCount,lengthArgs)||
    boneCount!=poseCount||boneCount<1||boneCount>1200||!Value(MeshSubMeshCount,original,submeshes)||submeshes!=1||
    !Value(MeshBlendCount,original,blendCount)||blendCount<0||blendCount>256)
  throw std::runtime_error("Expected a single-submesh renderer with a compatible bone palette");
 std::vector<float> weights=samePreview?nativeEfmi.originalWeights:std::vector<float>(blendCount);
 if(!samePreview)for(int i=0;i<blendCount;++i){void* args[]{&i};if(!Value(GetBlendWeight,renderer,weights[i],args))throw std::runtime_error("Cannot capture original shape weights");}
 int boneIndex=-1;
 for(int i=0;i<boneCount;++i){
  void* args[]{&i};auto bone=Call(Item,bones,args);
  if(!bone||ObjectName(bone)!=profile.bone)continue;
  if(boneIndex>=0)throw std::runtime_error("Bone name is ambiguous");
  boneIndex=i;
 }
 if(boneIndex<0)throw std::runtime_error("Named bone is outside the renderer palette");
 NativeEfmiArrays arrays;
 CheckNativeEfmiWeightLayout();
 auto vectorType=arrays.Class(core,"UnityEngine","Vector3");
 auto positions=arrays.Copy(*pins,vectorType,geometry.positions);
 auto triangles=arrays.Copy(*pins,arrays.Class("mscorlib.dll","System","Int32"),geometry.triangles);
 NativeEfmiBoneWeight rigid;rigid.weight[0]=1;rigid.index[0]=boneIndex;
 auto boneWeights=arrays.Copy(*pins,arrays.Class(core,"UnityEngine","BoneWeight"),
                              std::vector<NativeEfmiBoneWeight>(geometry.positions.size(),rigid));
 auto mesh=pins->Keep(arrays.objectNew(arrays.Class(core,"UnityEngine","Mesh")));
 if(!mesh||!NativeEfmiSet(MeshConstructor,mesh))throw std::runtime_error("Cannot construct Unity mesh");
 try{
  auto set=[&](Key key,void* value){void* args[]{value};if(!NativeEfmiSet(key,mesh,args))throw std::runtime_error("Unity mesh assignment failed");};
  set(MeshSetVertices,positions);
  set(MeshSetTriangles,triangles);
  if(!geometry.uv.empty())set(MeshSetUv,arrays.Copy(*pins,arrays.Class(core,"UnityEngine","Vector2"),geometry.uv));
  set(MeshSetBindPoses,bindposes);
  set(MeshSetBoneWeights,boneWeights);
  if(!NativeEfmiSet(MeshRecalculateNormals,mesh)||!NativeEfmiSet(MeshRecalculateBounds,mesh))
   throw std::runtime_error("Unity mesh recalculation failed");
  if(hasShape){
   auto delta=arrays.Copy(*pins,vectorType,geometry.deltas);
   if(!runtime->string_new)throw std::runtime_error("Unity string API unavailable");
   auto name=pins->Keep(runtime->string_new(runtime->context,"EAI_EFMI_Position"));
   float weight=100;
   void* args[]{name,&weight,delta,nullptr,nullptr};
   if(!name||!NativeEfmiSet(MeshAddBlendFrame,mesh,args))throw std::runtime_error("Cannot install EFMI shape delta");
  }
  if(nativeEfmi.pins&&!RestoreNativeEfmi())throw std::runtime_error("Could not restore previous preview");
  void* meshArgs[]{mesh};
  if(!NativeEfmiSet(SetSharedMesh,renderer,meshArgs))throw std::runtime_error("Cannot replace renderer mesh");
 }catch(...){if(Living(mesh)){void* args[]{mesh};NativeEfmiSet(DestroyNativeMesh,nullptr,args);}throw;}
 nativeEfmi.actor=actor;
 nativeEfmi.actorIndex=profile.actor;
 nativeEfmi.renderer=renderer;
 nativeEfmi.original=original;
 nativeEfmi.mesh=mesh;
 nativeEfmi.originalWeights=std::move(weights);
 nativeEfmi.pins=std::move(pins);
 nativeEfmi.hasShape=hasShape;
 nativeEfmi.weight=nativeEfmi.appliedWeight=0;
}

void PumpNativeEfmi(){
 if(nativeEfmi.inspect){nativeEfmi.inspect=false;InspectNativeEfmi();nativeEfmi.status="已將 renderer 與骨骼名稱寫入日誌";}
 if(nativeEfmi.pins){
  Pins pins;
  bool changed=!Living(nativeEfmi.renderer)||Call(SharedMesh,nativeEfmi.renderer)!=nativeEfmi.mesh||
               NativeEfmiActor(nativeEfmi.actorIndex,pins)!=nativeEfmi.actor;
  if(changed||nativeEfmi.restore){
   if(!RestoreNativeEfmi()){nativeEfmi.status="模型還原失敗，請重啟遊戲";return;}
   nativeEfmi.status="已結束 EFMI 網格預覽";
  }
 }
 nativeEfmi.restore=false;
 if(nativeEfmi.request&&nativeEfmi.geometry){
  try{
   ApplyNativeEfmi(*nativeEfmi.request,*nativeEfmi.geometry);
   nativeEfmi.status="已載入 EFMI 網格預覽";
  }catch(const std::exception& e){nativeEfmi.status=e.what();Log("Native EFMI: "+nativeEfmi.status);}
  nativeEfmi.request.reset();nativeEfmi.geometry.reset();
 }
 if(nativeEfmi.pins&&nativeEfmi.hasShape&&nativeEfmi.weight!=nativeEfmi.appliedWeight){
  int index=0;void* args[]{&index,&nativeEfmi.weight};
  if(NativeEfmiSet(SetBlendWeight,nativeEfmi.renderer,args))nativeEfmi.appliedWeight=nativeEfmi.weight;
  else{nativeEfmi.weight=nativeEfmi.appliedWeight;nativeEfmi.status="套用 EFMI 形變失敗";}
 }
}

void QueueNativeEfmi(const std::string& path){
 if(!hook)throw std::runtime_error("請先連接遊戲入口");
 auto profile=NativeEfmi::ReadProfile(std::filesystem::u8path(path));
 auto geometry=NativeEfmi::ReadGeometry(profile);
 nativeEfmi.request=std::move(profile);
 nativeEfmi.geometry=std::move(geometry);
 nativeEfmi.status="等待遊戲更新載入 EFMI 網格";
}

void CancelNativeEfmi(){
 nativeEfmi.request.reset();
 nativeEfmi.geometry.reset();
 nativeEfmi.restore=true;
 nativeEfmi.status=nativeEfmi.pins?"等待遊戲更新還原原始網格":"已結束 EFMI 網格預覽";
}

void QueueNativeEfmiInspection(){
 if(!hook)throw std::runtime_error("請先連接遊戲入口");
 nativeEfmi.inspect=true;
 nativeEfmi.status="等待遊戲更新列出 renderer 與骨骼";
}

void SetNativeEfmiWeight(float weight){
 if(!nativeEfmi.hasShape||!std::isfinite(weight)||weight<0||weight>100)
  throw std::runtime_error("EFMI 形變權重無效");
 nativeEfmi.weight=weight;
}

Presets::Json NativeEfmiState(){
 return {{"detail",nativeEfmi.status},{"shape",nativeEfmi.hasShape},{"weight",nativeEfmi.weight}};
}
