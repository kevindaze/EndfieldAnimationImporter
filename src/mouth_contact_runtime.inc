struct MouthRuntime {
 MouthContact::Attachment target=MouthContact::Attachment::Off;
 bool wrote=false,rebind=false;
 bool loopRequested=false,loopPlaying=false,rootTouching=false;
 std::string status="吸附未啟用",meshName;
 std::filesystem::path meshIni;
 MouthContact::Probe geometry{};
 std::filesystem::path bufferFile;
 ExternalMesh::Bytes sourceBytes,followBytes;
 std::vector<size_t> vertices,triangles;
 std::vector<float> rotationWeights;
 std::vector<PoseMath::Vector3> lipFrameVertices;
 std::vector<float> contactRayHits;
 std::array<std::vector<float>,3> sectionHits;
 using Submit=BOOL (WINAPI*)(const wchar_t*,const void*,UINT);
 using RenderCount=ULONGLONG (WINAPI*)();
 Submit submit=nullptr;
 RenderCount renderCount=nullptr;
 FILETIME baselineTime{};
 bool baselineSidecar=false,submitted=false,following=false;
 PoseMath::Quaternion followRotation{};
 ULONGLONG nextBaselineCheck=0,submittedAt=0,lastTrace=0,renderCountAtSubmit=0,faceWrites=0;
 unsigned long long rootAudioPlays=0;

 std::unique_ptr<Pins> pins;
 void* pelvis=nullptr;
 void* jaw=nullptr;
 void* lipUp=nullptr;
 void* lipDown=nullptr;
 void* lipLeft=nullptr;
 void* lipRight=nullptr;
 void* eyeLeft=nullptr;
 void* eyeRight=nullptr;
 std::array<void*,4> handAnchorBones{};
 void* targetPelvis=nullptr;
 PoseMath::Vector3 pelvicAnchorLocal{};
 MouthContact::AttachmentAxes pelvicAxesLocal{};
 MouthContact::AttachmentOffsets attachmentOffsets{};
 int impactVolume=100,playingVolume=-1;
 ExternalMesh::Bytes impactSource,impactWave;
 struct LowerFaceBone {
  void* bone;void* parent;
  PoseMath::Vector3 basePosition{},writtenPosition{};
  PoseMath::Quaternion baseRotation{},writtenRotation{};
  bool moved=false,rotated=false;
 };
 std::vector<LowerFaceBone> lowerFace;
 struct FaceShape {void* renderer;void* mesh;int index;float base=0,written=0;bool changed=false;};
 std::vector<FaceShape> faceShapes;
 ReferenceMatrix inverseBind{};
 PoseMath::Vector3 mouthOffset{},mouthRight{},mouthUp{},mouthOut{};
 uint64_t sourceLease=0,targetLease=0,attemptedTarget=0;
 float opening=0;
 PoseMath::Quaternion base{},written{};
} mouthContact;

struct AttachmentMarkerState{
 MouthContact::Attachment target=MouthContact::Attachment::Off;
 ULONGLONG expires=0,retryAt=0;
 std::optional<PoseMath::Vector3> world,screen;
 std::unique_ptr<Pins> pins;
 std::string identity;
 void* root=nullptr;
 std::array<void*,4> bones{};
} attachmentMarker;

void RequestAttachmentMarker(MouthContact::Attachment target){
 if(attachmentMarker.target!=target){
  attachmentMarker.identity.clear();
  attachmentMarker.screen.reset();
 }
 attachmentMarker.target=target;
 attachmentMarker.expires=GetTickCount64()+MouthContact::AttachmentMarkerMs;
}

void MarkAttachmentWorld(MouthContact::Attachment target,PoseMath::Vector3 point){
 if(attachmentMarker.target==target&&MouthContact::AttachmentMarkerVisible(GetTickCount64(),attachmentMarker.expires))
  attachmentMarker.world=point;
}

bool PlayContactAudio(const wchar_t* filename,DWORD flags){
 const auto path=moduleFolder/L"audio"/filename;
 if(PlaySoundW(path.c_str(),nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT|flags))return true;
 Log("Contact audio could not start: "+EfmiBridge::Utf8(path.filename()));
 return false;
}

bool PlayPelvicAudio(){
 if(!mouthContact.impactVolume)return false;
 try{
  if(mouthContact.playingVolume==mouthContact.impactVolume){
   if(PlaySoundW(reinterpret_cast<const wchar_t*>(mouthContact.impactWave.data()),nullptr,
      SND_MEMORY|SND_ASYNC|SND_NODEFAULT))return true;
  }else{
   if(mouthContact.impactSource.empty()){
    const auto path=moduleFolder/L"audio/papaAudio.wav";
    if(std::filesystem::file_size(path)>2*1024*1024)throw std::runtime_error("Contact audio is too large");
    mouthContact.impactSource=ExternalMesh::Read(path);
   }
   auto adjusted=ContactAudio::WithVolume(mouthContact.impactSource,mouthContact.impactVolume);
   if(PlaySoundW(reinterpret_cast<const wchar_t*>(adjusted.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT)){
    // Keep the buffer alive until the next playback replaces it or the module stops.
    mouthContact.impactWave=std::move(adjusted);
    mouthContact.playingVolume=mouthContact.impactVolume;
    return true;
   }
  }
 }catch(const std::exception& error){Log(std::string("Pelvic contact audio: ")+error.what());return false;}
 Log("Contact audio could not start: papaAudio.wav");
 return false;
}

void StopPelvicAudio(){
 if(!mouthContact.impactWave.empty())PlaySoundW(nullptr,nullptr,0);
 mouthContact.impactWave.clear();
 mouthContact.playingVolume=-1;
}

void UpdateMouthAudio(bool touching){
 if(touching==mouthContact.loopRequested)return;
 mouthContact.loopRequested=touching;
 if(touching)mouthContact.loopPlaying=PlayContactAudio(L"mouthAudio.wav",SND_LOOP);
 else if(mouthContact.loopPlaying){
  PlaySoundW(nullptr,nullptr,0);
  mouthContact.loopPlaying=false;
 }
}

Presets::Json AttachmentOffsetsJson(){
 auto data=Presets::Json::object();
 for(size_t i=0;i<MouthContact::AttachmentCount;++i)
  data[std::string(MouthContact::AttachmentKeys[i+1])]=mouthContact.attachmentOffsets[i];
 return data;
}

Presets::Json AttachmentState(){
 using enum MouthContact::Attachment;
 return {{"enabled",mouthContact.target==Mouth},{"target",MouthContact::AttachmentKey(mouthContact.target)},
         {"status",mouthContact.status},{"attachment_offsets_cm",AttachmentOffsetsJson()},
         {"pelvic_offset_cm",mouthContact.attachmentOffsets[1][2]},
         {"pelvic_audio_volume",mouthContact.impactVolume}};
}

void RemoveMouthLayer(){
 for(auto& part:mouthContact.lowerFace){
  if(part.moved){
   PoseMath::Vector3 current{};
   if(Living(part.bone)&&Value(LocalPosition,part.bone,current)&&
      PoseMath::SamePosition(current,part.writtenPosition)){
    void* args[]{&part.basePosition};Call(SetLocalPosition,part.bone,args);
   }
   part.moved=false;
  }
  if(part.rotated){
   PoseMath::Quaternion current{};
   if(Living(part.bone)&&Rotation(part.bone,current)&&PoseMath::SameRotation(current,part.writtenRotation))
    WriteRotation(part.bone,part.baseRotation);
   part.rotated=false;
  }
 }
 for(auto& shape:mouthContact.faceShapes){
  if(!shape.changed)continue;
  shape.changed=false;
  float weight=0;void* get[]{&shape.index};
  if(Living(shape.renderer)&&Call(SharedMesh,shape.renderer)==shape.mesh&&
     Value(GetBlendWeight,shape.renderer,weight,get)&&std::abs(weight-shape.written)<.01f){
   void* set[]{&shape.index,&shape.base};Call(SetBlendWeight,shape.renderer,set);
  }
 }
 if(!mouthContact.wrote)return;
 PoseMath::Quaternion current{};
 if(Living(mouthContact.jaw)&&Rotation(mouthContact.jaw,current)&&
    PoseMath::SameRotation(current,mouthContact.written))
  WriteRotation(mouthContact.jaw,mouthContact.base);
 mouthContact.wrote=false;
}

void ReleaseMouthFollow(){
 if(mouthContact.submitted&&mouthContact.submit)mouthContact.submit(nullptr,nullptr,0);
 mouthContact.submitted=false;
 mouthContact.following=false;
 mouthContact.followRotation={};
}

bool ConnectMouthRenderer(){
 if(mouthContact.submit)return true;
 auto path=(moduleFolder/"../../runtime/efmi/d3d11.dll").lexically_normal();
 auto library=GetModuleHandleW(path.c_str());
 if(!library)return false;
 mouthContact.submit=reinterpret_cast<MouthRuntime::Submit>(GetProcAddress(library,"EaiSubmitMeshFollow"));
 mouthContact.renderCount=reinterpret_cast<MouthRuntime::RenderCount>(GetProcAddress(library,"EaiMeshFollowUpdates"));
 return mouthContact.submit!=nullptr;
}

void RefreshMouthBaseline(){
 const auto now=GetTickCount64();
 if(now<mouthContact.nextBaselineCheck)return;
 mouthContact.nextBaselineCheck=now+250;
 auto live=ExternalMesh::LivePath(mouthContact.bufferFile);
 WIN32_FILE_ATTRIBUTE_DATA info{},original{};
 const bool exists=GetFileAttributesExW(live.c_str(),GetFileExInfoStandard,&info);
 if(!exists&&!mouthContact.baselineSidecar)return;
 if(exists&&mouthContact.baselineSidecar&&!CompareFileTime(&info.ftLastWriteTime,&mouthContact.baselineTime))return;
 try{
  auto bytes=ExternalMesh::Read(exists?live:mouthContact.bufferFile);
  if(exists){
   if(bytes.size()!=mouthContact.followBytes.size()+ExternalMesh::LiveHeaderSize||
      std::memcmp(bytes.data(),ExternalMesh::LiveMagic,sizeof(ExternalMesh::LiveMagic)-1)!=0||
      !GetFileAttributesExW(mouthContact.bufferFile.c_str(),GetFileExInfoStandard,&original))return;
   auto sourceTime=ExternalMesh::Get<FILETIME>(bytes,sizeof(ExternalMesh::LiveMagic)-1);
   if(CompareFileTime(&sourceTime,&original.ftLastWriteTime))return;
   bytes.erase(bytes.begin(),bytes.begin()+ExternalMesh::LiveHeaderSize);
  }
  if(bytes.size()!=mouthContact.followBytes.size())return;
  mouthContact.sourceBytes=std::move(bytes);
  mouthContact.baselineSidecar=exists;
  mouthContact.baselineTime=info.ftLastWriteTime;
 }catch(const std::exception& e){Log(std::string("Mouth follow baseline: ")+e.what());}
}

bool PublishMouthFollow(PoseMath::Vector3 pivot,PoseMath::Quaternion rotation){
 if(!ConnectMouthRenderer())return false;
 auto& bytes=mouthContact.followBytes;
 bytes=mouthContact.sourceBytes;
 for(size_t i=0;i<mouthContact.vertices.size();++i){
  if(mouthContact.rotationWeights[i]<=0)continue;
  const auto offset=mouthContact.vertices[i];
  PoseMath::Vector3 source{ExternalMesh::Get<float>(bytes,offset),ExternalMesh::Get<float>(bytes,offset+4),
                           ExternalMesh::Get<float>(bytes,offset+8)};
  auto point=MouthContact::RotateAttachedVertex(pivot,rotation,source,mouthContact.rotationWeights[i]);
  ExternalMesh::Put<float>(bytes,offset,point.x);
  ExternalMesh::Put<float>(bytes,offset+4,point.y);
  ExternalMesh::Put<float>(bytes,offset+8,point.z);
 }
 if(!mouthContact.submit(mouthContact.bufferFile.c_str(),mouthContact.followBytes.data(),
                        static_cast<UINT>(mouthContact.followBytes.size())))return false;
 if(!mouthContact.submitted){
  mouthContact.submittedAt=GetTickCount64();
  mouthContact.renderCountAtSubmit=mouthContact.renderCount?mouthContact.renderCount():0;
 }
 mouthContact.submitted=true;
 return true;
}

void ClearMouthBinding(){
 UpdateMouthAudio(false);
 mouthContact.rootTouching=false;
 ReleaseMouthFollow();
 mouthContact.faceShapes.clear();
 mouthContact.lowerFace.clear();
 mouthContact.lipUp=mouthContact.lipDown=mouthContact.lipLeft=mouthContact.lipRight=nullptr;
 mouthContact.eyeLeft=mouthContact.eyeRight=nullptr;
 mouthContact.handAnchorBones={};
 mouthContact.targetPelvis=nullptr;
 mouthContact.pins.reset();
 mouthContact.pelvis=mouthContact.jaw=nullptr;
 mouthContact.opening=0;
 mouthContact.sourceLease=mouthContact.targetLease=0;
}

void PrepareMouthGeometry(){
 mouthContact.rebind=true;
 mouthContact.meshIni.clear();
 mouthContact.meshName.clear();
 mouthContact.bufferFile.clear();
 mouthContact.vertices.clear();
 mouthContact.rotationWeights.clear();
 mouthContact.triangles.clear();
 mouthContact.lipFrameVertices.clear();
 mouthContact.sourceBytes.clear();
 mouthContact.followBytes.clear();
 mouthContact.baselineSidecar=false;
 mouthContact.nextBaselineCheck=0;
 const EfmiBridge::Mod* selected=nullptr;
 const EfmiBridge::Mesh* target=nullptr;
 for(auto& mod:efmiScan.mods)for(auto& mesh:mod.meshes){
  auto name=EfmiBridge::Lower(EfmiBridge::MeshLabel(mesh.name));
  if(name!="component 9 / dih"&&name!="component 9 / weap")continue;
  if(selected){mouthContact.status="找到多個 DIH 外型，請只保留一個已啟用的外型";return;}
  selected=&mod;target=&mesh;
 }
 if(!target){mouthContact.status="找不到支援的 DIH 外型";return;}
 try{
  auto& resource=ExternalMesh::PositionResource(*selected,*target);
  auto bufferFile=ExternalMesh::ResourceFile(*selected,resource);
  auto bytes=ExternalMesh::Read(bufferFile);
  if(bytes.size()>8*1024*1024)throw std::runtime_error("DIH follow buffer exceeds 8 MB");
  if(bytes.size()%resource.stride)throw std::runtime_error("Invalid DIH stride");
  if(target->count>300000)throw std::runtime_error("Mesh exceeds the mouth contact triangle budget");
  auto draw=ExternalMesh::DrawIndices(*selected,*target,bytes.size()/resource.stride);
  const std::set<size_t> indices(draw.begin(),draw.end());
  std::vector<ExternalMesh::Vec> points;
  for(auto i:indices)points.push_back(ExternalMesh::Vertex(bytes,i*resource.stride));
  auto [lo,hi]=std::minmax_element(points.begin(),points.end(),[](auto& a,auto& b){return a[1]<b[1];});
  const float span=(*hi)[1]-(*lo)[1];
  if(span<.015f||span>.6f)throw std::runtime_error("Unsupported DIH model scale");
  const auto center=[&](float y,float window){
   float x=0,z=0,count=0;
   for(auto p:points)if(std::abs(p[1]-y)<=window){x+=p[0];z+=p[2];++count;}
   if(count==0)throw std::runtime_error("Missing DIH cross section");
   return PoseMath::Vector3{x/count,y,z/count};
  };
  auto base=center((*hi)[1],span*.06f),tip=center((*lo)[1],span*.06f);
  mouthContact.geometry={base,tip};
  for(auto index:indices)mouthContact.vertices.push_back(index*resource.stride);
  for(const auto& p:points)mouthContact.rotationWeights.push_back(MouthContact::RootRotationWeight(((*hi)[1]-p[1])/span));
  for(auto index:draw){
   const auto it=std::lower_bound(mouthContact.vertices.begin(),mouthContact.vertices.end(),index*resource.stride);
   mouthContact.triangles.push_back(static_cast<size_t>(it-mouthContact.vertices.begin()));
  }
  mouthContact.lipFrameVertices.resize(mouthContact.vertices.size());
  mouthContact.bufferFile=std::move(bufferFile);
  mouthContact.sourceBytes=std::move(bytes);
  mouthContact.followBytes.resize(mouthContact.sourceBytes.size());
  mouthContact.meshIni=selected->file;
  mouthContact.meshName=target->name;
  mouthContact.status="等待男管理員播放配對動畫";
 }catch(const std::exception& e){
  mouthContact.status="無法從 DIH 網格建立接觸範圍";
  Log(std::string("Mouth contact geometry: ")+e.what());
 }
}

PoseMath::Vector3 MouthTransformPoint(const ReferenceMatrix& matrix,PoseMath::Vector3 p){
 auto& m=matrix.m;
 return {m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12],
         m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13],
         m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14]};
}

bool MouthInverseBind(Pins& pins,void* pelvis,ReferenceMatrix& result){
 if(!skinnedRenderer.type_object||!methods[BindPoses].resolved.method_info)return false;
 bool inactive=true;void* args[]{skinnedRenderer.type_object,&inactive};
 auto renderers=pins.Keep(Call(Components,pose.model,args));
 int dim=0,count=0;void* lengthArgs[]{&dim};
 if(!renderers||!Value(Length,renderers,count,lengthArgs)||count<1||count>256)return false;
 float best=.45f;
 for(int i=0;i<count;++i){
  void* item[]{&i};auto renderer=pins.Keep(Call(Item,renderers,item));
  auto mesh=renderer?pins.Keep(Call(SharedMesh,renderer)):nullptr;
  auto palette=renderer?pins.Keep(Call(MeshBones,renderer)):nullptr;
  auto bind=mesh?pins.Keep(Call(BindPoses,mesh)):nullptr;
  int n=0,m=0;
  if(!palette||!bind||!Value(Length,palette,n,lengthArgs)||!Value(Length,bind,m,lengthArgs)||n!=m||n<1||n>1200)continue;
  for(int j=0;j<n;++j){
   void* index[]{&j};
   if(Call(Item,palette,index)!=pelvis)continue;
   ReferenceMatrix matrix{};
   if(!Value(Item,bind,matrix,index))break;
   // EFMI's saved VB0 positions are already in the renderer's bind space.
   auto a=MouthTransformPoint(matrix,mouthContact.geometry.base);
   auto b=MouthTransformPoint(matrix,mouthContact.geometry.tip);
   float distance=MouthContact::Length(a),span=MouthContact::Length(MouthContact::Subtract(b,a));
   if(distance<best&&span>.01f&&span<.65f){result=matrix;best=distance;}
   break;
  }
 }
 return best<.45f;
}

void BindMouthShapes(Pins& pins){
 if(!skinnedRenderer.type_object||!methods[MeshBlendCount].resolved.method_info||
    !methods[MeshBlendName].resolved.method_info||!methods[GetBlendWeight].resolved.method_info||
    !methods[SetBlendWeight].resolved.method_info)return;
 bool inactive=true;void* args[]{skinnedRenderer.type_object,&inactive};
 auto array=pins.Keep(Call(Components,pose.targetModel,args));
 int dimension=0,count=0;void* lengthArgs[]{&dimension};
 if(!array||!Value(Length,array,count,lengthArgs)||count<1||count>256)return;
 for(int i=0;i<count;++i){
  void* item[]{&i};auto renderer=pins.Keep(Call(Item,array,item));
  auto mesh=renderer?pins.Keep(Call(SharedMesh,renderer)):nullptr;
  int shapes=0;
  if(!mesh||!Value(MeshBlendCount,mesh,shapes)||shapes<1||shapes>512)continue;
  int best=-1,priority=0;
  for(int j=0;j<shapes;++j){
   void* index[]{&j};char buffer[192]{};
   auto name=Call(MeshBlendName,mesh,index);
   if(!name)continue;
   runtime->copy_managed_string(runtime->context,name,buffer,sizeof(buffer));
   auto key=EfmiBridge::Lower(buffer);
   std::erase_if(key,[](char c){return c=='_'||c=='-'||c=='.'||c==' ';});
   int rank=key=="jawopen"||key=="facsjawopen"?3:key=="mouthopen"||key=="moutha"||key=="lipopen"?2:key=="visemeaa"||key=="visemea"?1:0;
   if(rank>priority){best=j;priority=rank;}
  }
  if(best>=0)mouthContact.faceShapes.push_back({renderer,mesh,best});
 }
}
bool BindMouth(){
 using namespace MouthContact;
 auto pins=std::make_unique<Pins>();
 int budget=1400;
 auto pelvis=BreastFind(pose.root,"Bip001_Pelvis",*pins,budget);
 if(!pelvis||!MouthInverseBind(*pins,pelvis,mouthContact.inverseBind)){
  mouthContact.status="找不到男管理員骨盆綁定";
  return false;
 }
 std::vector<Pose::FullBone> bones;
 if(!WalkBones(pose.reverse.root,1,*pins,bones))return false;
 const auto finish=[&]{
  mouthContact.pelvis=pelvis;
  mouthContact.sourceLease=pose.lease;
  mouthContact.targetLease=pose.reverse.lease;
  mouthContact.pins=std::move(pins);
 };
 using enum Attachment;
 if(mouthContact.target!=Mouth){
  const auto findUnique=[&](std::string_view name)->void*{
   void* result=nullptr;
   for(const auto& bone:bones)if(bone.info.name==name){
    if(result&&result!=bone.bone)return nullptr;
    result=bone.bone;
   }
   return result;
  };
  if(mouthContact.target==Pelvis){
   auto hips=findUnique("Bip001_Pelvis"),left=findUnique("Bip001_L_Thigh"),
        right=findUnique("Bip001_R_Thigh"),spine=findUnique("Bip001_Spine");
   Vector3 leftPosition{},rightPosition{},spinePosition{},anchor{};
   AttachmentAxes axes{};
   ReferenceMatrix inverse{};
   PoseMath::Quaternion bodyRotation{};
   if(!hips||!left||!right||!spine||!Value(WorldToLocal,hips,inverse)||
      !Value(Position,left,leftPosition)||!Value(Position,right,rightPosition)||
      !Value(Position,spine,spinePosition)||!Value(WorldRotation,pose.reverse.root,bodyRotation)||
      !PoseMath::Normalize(bodyRotation)||!PelvicAnchor(leftPosition,rightPosition,spinePosition,
       PoseMath::Rotate(bodyRotation,{0,0,1}),anchor,&axes)){
    mouthContact.status="互動角色缺少骨盆定位所需的骨架";
    return false;
   }
   mouthContact.pelvicAnchorLocal=MouthTransformPoint(inverse,anchor);
   const auto localAxis=[&](Vector3 axis){
    return Subtract(MouthTransformPoint(inverse,Add(anchor,axis)),mouthContact.pelvicAnchorLocal);
   };
   mouthContact.pelvicAxesLocal={localAxis(axes.right),localAxis(axes.up),localAxis(axes.forward)};
   if(!Finite(mouthContact.pelvicAnchorLocal)||!Finite(mouthContact.pelvicAxesLocal.right)||
      !Finite(mouthContact.pelvicAxesLocal.up)||!Finite(mouthContact.pelvicAxesLocal.forward)){
    mouthContact.status="吸附目標骨架姿勢已失效";
    return false;
   }
   mouthContact.targetPelvis=hips;
  }else{
   const bool left=mouthContact.target==LeftHand;
   mouthContact.handAnchorBones={findUnique(left?"Bip001_L_Hand":"Bip001_R_Hand"),
                                 findUnique(left?"Bip001_L_Finger2":"Bip001_R_Finger2"),
                                 findUnique(left?"Bip001_L_Finger1":"Bip001_R_Finger1"),
                                 findUnique(left?"Bip001_L_Finger4":"Bip001_R_Finger4")};
   if(!mouthContact.handAnchorBones[0]||!mouthContact.handAnchorBones[1]){
    mouthContact.status=left?"互動角色缺少左手掌骨架":"互動角色缺少右手掌骨架";
    return false;
   }
  }
  Log("Attachment binding: "+std::string(AttachmentKey(mouthContact.target)));
  finish();
  return true;
 }
 void* jaw=nullptr;
 std::set<void*> lower;
 for(auto& bone:bones){
  auto name=EfmiBridge::Lower(bone.info.name);
  if((name.starts_with("lip")&&name.ends_with("joint")&&name.find("dn")!=name.npos)||
     name=="facemdjawdnjoint"||name=="facemdtoothdnjoint")lower.insert(bone.bone);
  if(name=="lipmupjoint")mouthContact.lipUp=bone.bone;
  if(name=="lipmdnjoint")mouthContact.lipDown=bone.bone;
  if(name=="liplup1joint")mouthContact.lipLeft=bone.bone;
  if(name=="liprup1joint")mouthContact.lipRight=bone.bone;
  if(name=="eyelfjoint")mouthContact.eyeLeft=bone.bone;
  if(name=="eyertjoint")mouthContact.eyeRight=bone.bone;
  std::erase_if(name,[](char c){return c=='_'||c=='-'||c==' ';});
  if(name=="jawjoint"){jaw=bone.bone;continue;}
  if(!jaw&&(name=="bip001jaw"||name=="jaw"||name=="jawjnt"||name=="jaw0skinjnt"||
            name=="mandible"||name=="lowerjaw"||name=="jawbone"))jaw=bone.bone;
 }
 Vector3 head{},neck{};
 PoseMath::Quaternion headRotation{},bodyRotation{};
 auto neckBone=pins->Keep(Call(Parent,pose.targetHead));
 if(!neckBone||!Value(Position,neckBone,neck)||!Value(Position,pose.targetHead,head)||
    !Value(WorldRotation,pose.targetHead,headRotation)||
    !Value(WorldRotation,pose.reverse.root,bodyRotation)||!PoseMath::Normalize(headRotation)||
    !PoseMath::Normalize(bodyRotation)){mouthContact.status="無法定位嘴部感應範圍";return false;}
 auto up=Subtract(head,neck);
 if(!Normalize(up))return false;
 auto bodyForward=PoseMath::Rotate(bodyRotation,{0,0,1});
 const bool hasPivot=Living(jaw);
 auto out=Subtract(bodyForward,Scale(up,Dot(bodyForward,up)));
 if(!Normalize(out))return false;
 auto right=Cross(up,out);
 if(!Normalize(right))return false;
 auto inverse=PoseMath::Inverse(headRotation);
 auto center=Subtract(Scale(out,.09f),Scale(up,.045f));
 mouthContact.mouthOffset=PoseMath::Rotate(inverse,center);
 mouthContact.mouthRight=PoseMath::Rotate(inverse,right);
 mouthContact.mouthUp=PoseMath::Rotate(inverse,up);
 mouthContact.mouthOut=PoseMath::Rotate(inverse,out);
 if(hasPivot)for(auto bone:lower){
  if(bone==jaw)continue;
  auto parent=pins->Keep(Call(Parent,bone));
  if(!Living(parent))continue;
  bool followsHinge=false;
  auto ancestor=parent;
  for(int i=0;ancestor&&i<48;++i){
   if(ancestor==jaw||lower.contains(ancestor)){followsHinge=true;break;}
   ancestor=Call(Parent,ancestor);
  }
  if(!followsHinge)mouthContact.lowerFace.push_back({bone,parent});
 }
 BindMouthShapes(*pins);
 Log("Mouth binding: actor="+Presets::Identity(ObjectName(pose.targetModel))+
     " shapes="+std::to_string(mouthContact.faceShapes.size())+
     " jaw="+(hasPivot?ObjectName(jaw):std::string("none"))+
     " lip_reference="+std::to_string(mouthContact.lipUp&&mouthContact.lipDown)+
     " hinge_followers="+std::to_string(mouthContact.lowerFace.size()));
 mouthContact.jaw=hasPivot?jaw:nullptr;
 finish();
 return true;
}

bool UpdateMouthEntrance(MouthContact::Entrance& entrance,PoseMath::Vector3 head){
 using namespace MouthContact;
 Vector3 upper{},lower{},left{},right{};
 if(!mouthContact.lipUp||!mouthContact.lipDown||
    !Value(Position,mouthContact.lipUp,upper)||!Value(Position,mouthContact.lipDown,lower)||
    !Finite(upper)||!Finite(lower))return false;
 auto center=PoseMath::Lerp(upper,lower,.5f);
 if(MouthContact::Length(Subtract(center,head))>.25f)return false;
 entrance.center=center;
 entrance.gap=std::max(0.f,Dot(Subtract(upper,lower),entrance.up));
 if(!mouthContact.eyeLeft||!mouthContact.eyeRight||
    !Value(Position,mouthContact.eyeLeft,left)||!Value(Position,mouthContact.eyeRight,right))return true;
 auto lateral=Subtract(right,left),up=Subtract(PoseMath::Lerp(left,right,.5f),center);
 Vector3 lipLeft{},lipRight{};
 if(mouthContact.lipLeft&&mouthContact.lipRight&&
    Value(Position,mouthContact.lipLeft,lipLeft)&&Value(Position,mouthContact.lipRight,lipRight)){
  const auto lipLine=Subtract(lipRight,lipLeft);
  const auto width=MouthContact::Length(lipLine);
  if(width>.015f&&width<.055f){
   lateral=Scale(lipLine,1/width);
   entrance.width=std::clamp(width*.5f,.009f,.023f);
   const auto sideCorrection=Dot(Subtract(PoseMath::Lerp(lipLeft,lipRight,.5f),center),lateral);
   entrance.center=Add(center,Scale(lateral,std::clamp(sideCorrection,-.006f,.006f)));
  }
 }
 if(!Normalize(lateral))return true;
 up=Subtract(up,Scale(lateral,Dot(up,lateral)));
 if(!Normalize(up))return true;
 auto outward=Cross(lateral,up);
 if(!Normalize(outward))return true;
 if(Dot(outward,Subtract(center,head))<0){outward=Scale(outward,-1);lateral=Scale(lateral,-1);}
 entrance.right=lateral;entrance.up=up;entrance.outward=outward;
 entrance.gap=std::max(0.f,Dot(Subtract(upper,lower),up));
 return true;
}

bool ReadPalmAnchor(const std::array<void*,4>& bones,bool left,const MouthContact::AttachmentOffset& offset,
                    PoseMath::Vector3& anchor){
 using namespace MouthContact;
 const auto [hand,middle,index,little]=bones;
 Vector3 wristPosition{},middlePosition{};
 if(!Living(hand)||!Living(middle)||!Value(Position,hand,wristPosition)||!Value(Position,middle,middlePosition))return false;
 anchor=PalmAnchor(wristPosition,middlePosition);
 if(offset==AttachmentOffset{})return Finite(anchor);
 AttachmentAxes axes{};
 Vector3 indexPosition{},littlePosition{};
 if(!Living(index)||!Living(little)||!Value(Position,index,indexPosition)||!Value(Position,little,littlePosition)||
    !PalmAxes(wristPosition,middlePosition,indexPosition,littlePosition,left,axes)){
  PoseMath::Quaternion rotation{};
  if(!Value(WorldRotation,hand,rotation)||!PoseMath::Normalize(rotation))return false;
  axes={PoseMath::Rotate(rotation,{1,0,0}),PoseMath::Rotate(rotation,{0,1,0}),PoseMath::Rotate(rotation,{0,0,1})};
 }
 anchor=OffsetAnchor(anchor,axes,offset);
 return Finite(anchor);
}

bool BindAttachmentMarker(const std::string& identity,ULONGLONG now){
 using enum MouthContact::Attachment;
 auto& marker=attachmentMarker;
 if(marker.identity==identity&&marker.pins&&Living(marker.root)&&
    (!pose.pins||!Living(pose.reverse.root)||pose.reverse.root==marker.root))return true;
 if(marker.identity!=identity)marker.retryAt=0;
 marker.identity=identity;
 marker.pins.reset();marker.root=nullptr;marker.bones={};
 if(now<marker.retryAt)return false;
 marker.retryAt=now+500;
 Pins scan;
 int matches=0;
 void* root=nullptr;
 if(pose.pins&&Living(pose.reverse.root)){root=pose.reverse.root;matches=1;}
 else EachPartner(scan,[&](void*,void* model,int){
  if(Presets::Identity(ObjectName(model))!=identity)return;
  root=scan.Keep(Call(GoTransform,model));
  ++matches;
 });
 if(matches!=1||!Living(root))return false;
 std::vector<Pose::FullBone> hierarchy;
 if(!WalkBones(root,1,scan,hierarchy))return false;
 auto pins=std::make_unique<Pins>();
 const auto find=[&](std::string_view name)->void*{
  void* found=nullptr;
  for(const auto& bone:hierarchy)if(EfmiBridge::Lower(bone.info.name)==name){
   if(found&&found!=bone.bone)return nullptr;
   found=bone.bone;
  }
  return found?pins->Keep(found):nullptr;
 };
 auto& b=marker.bones;
 if(marker.target==Mouth)
  b={find("bip001_head"),find("bip001_neck"),find("lipmupjoint"),find("lipmdnjoint")};
 else if(marker.target==Pelvis)
  b={find("bip001_l_thigh"),find("bip001_r_thigh"),find("bip001_spine")};
 else {
  const std::string side=marker.target==LeftHand?"bip001_l_":"bip001_r_";
  b={find(side+"hand"),find(side+"finger2"),find(side+"finger1"),find(side+"finger4")};
 }
 marker.root=pins->Keep(root);
 marker.pins=std::move(pins);
 return Living(marker.root);
}

bool ReadAttachmentMarkerWorld(ULONGLONG now,PoseMath::Vector3& point){
 using namespace MouthContact;
 using enum Attachment;
 const auto target=attachmentMarker.target;
 if(target==Off||!BindAttachmentMarker(workflow.draft.target,now))return false;
 const auto& b=attachmentMarker.bones;
 const auto& offset=mouthContact.attachmentOffsets[static_cast<size_t>(target)-1];
 if(target==LeftHand||target==RightHand)return ReadPalmAnchor({b[0],b[1],b[2],b[3]},target==LeftHand,offset,point);
 Vector3 a{},c{},d{};
 PoseMath::Quaternion body{};
 if(!Value(WorldRotation,attachmentMarker.root,body)||!PoseMath::Normalize(body)||
    !Living(b[0])||!Living(b[1])||!Value(Position,b[0],a)||!Value(Position,b[1],c))return false;
 auto forward=PoseMath::Rotate(body,{0,0,1});
 AttachmentAxes axes{};
 if(target==Pelvis){
  if(!Living(b[2])||!Value(Position,b[2],d)||!PelvicAnchor(a,c,d,forward,point,&axes))return false;
 }else{
  auto up=Subtract(a,c);
  if(!Normalize(up))return false;
  forward=Subtract(forward,Scale(up,Dot(forward,up)));
  if(!Normalize(forward))return false;
  axes={Cross(up,forward),up,forward};
  point=Add(a,Subtract(Scale(forward,.09f),Scale(up,.045f)));
  Vector3 upper{},lower{};
  if(Living(b[2])&&Living(b[3])&&Value(Position,b[2],upper)&&Value(Position,b[3],lower)&&
     MouthContact::Length(Subtract(PoseMath::Lerp(upper,lower,.5f),a))<=.25f)point=PoseMath::Lerp(upper,lower,.5f);
 }
 point=OffsetAnchor(point,axes,offset);
 return Finite(point);
}

void PumpAttachmentMarker(){
 auto& marker=attachmentMarker;
 const auto now=GetTickCount64();
 if(!MouthContact::AttachmentMarkerVisible(now,marker.expires)){
  if(marker.target!=MouthContact::Attachment::Off)marker=AttachmentMarkerState{};
  return;
 }
 marker.screen.reset();
 if(!methods[WorldToViewportPoint].resolved.method_info)return;
 PoseMath::Vector3 point{},screen{};
 if(marker.world)point=*marker.world;
 else if(!ReadAttachmentMarkerWorld(now,point))return;
 auto camera=Call(CameraMain);
 void* args[]{&point};
 if(camera&&Value(WorldToViewportPoint,camera,screen,args)&&PoseMath::Finite(screen)&&
    screen.z>0&&screen.x>=0&&screen.x<=1&&screen.y>=0&&screen.y<=1)marker.screen=screen;
}

void ApplyLowerFaceHinge(PoseMath::Vector3 pivot,PoseMath::Quaternion worldDelta){
 for(auto& part:mouthContact.lowerFace){
  ReferenceMatrix parentInverse{};
  PoseMath::Vector3 worldPosition{};
  PoseMath::Quaternion parentRotation{};
  if(!Living(part.bone)||!Living(part.parent)||
     !Value(WorldToLocal,part.parent,parentInverse)||!Value(WorldRotation,part.parent,parentRotation)||
     !PoseMath::Normalize(parentRotation)||!Value(Position,part.bone,worldPosition)||
     !Value(LocalPosition,part.bone,part.basePosition)||!Rotation(part.bone,part.baseRotation))continue;
  part.writtenPosition=MouthTransformPoint(parentInverse,MouthContact::RotateAround(pivot,worldDelta,worldPosition));
  if(!PoseMath::Finite(part.writtenPosition))continue;
  void* args[]{&part.writtenPosition};
  Call(SetLocalPosition,part.bone,args,&part.moved);
  const auto localDelta=PoseMath::Multiply(PoseMath::Multiply(PoseMath::Inverse(parentRotation),worldDelta),parentRotation);
  part.writtenRotation=PoseMath::Multiply(localDelta,part.baseRotation);
  part.rotated=WriteRotation(part.bone,part.writtenRotation);
 }
}

void PumpMouth(float dt){
 using namespace MouthContact;
 using enum Attachment;
 if(mouthContact.rebind){ClearMouthBinding();mouthContact.attemptedTarget=0;mouthContact.rebind=false;}
 if(mouthContact.target==Off||mouthContact.meshIni.empty()){
  ClearMouthBinding();
  if(mouthContact.target==Off)mouthContact.status="吸附未啟用";
  return;
 }
 if(!pose.pins||pose.mode!=Mode::Custom||lastControlled!="chr_0002_endminm"||
    !leases->owns(pose.root,id,pose.lease)||!leases->owns(pose.reverse.root,id,pose.reverse.lease)){
  ClearMouthBinding();mouthContact.attemptedTarget=0;
  mouthContact.status="等待男管理員播放配對動畫";
  return;
 }
 if(mouthContact.sourceLease!=pose.lease||mouthContact.targetLease!=pose.reverse.lease)ClearMouthBinding();
 if(!mouthContact.pins){
  if(mouthContact.attemptedTarget==pose.reverse.lease)return;
  mouthContact.attemptedTarget=pose.reverse.lease;
  if(!BindMouth())return;
 }
 ReferenceMatrix pelvisWorld{};
 if(!Living(mouthContact.pelvis)||
    !Value(LocalToWorld,mouthContact.pelvis,pelvisWorld)){
  ClearMouthBinding();mouthContact.status="男管理員骨盆姿勢已失效";return;
 }
 const EfmiBridge::Mesh* mesh=nullptr;
 for(auto& mod:efmiScan.mods)if(mod.file==mouthContact.meshIni)
  for(auto& candidate:mod.meshes)if(candidate.name==mouthContact.meshName)mesh=&candidate;
 if(!mesh){UpdateMouthAudio(false);mouthContact.rootTouching=false;return;}
 const bool audioPlaying=!pose.scrubbing&&((!pose.partnerStopped&&!pose.clipPaused)||
  (pose.controlledClip&&!pose.mainStopped&&!pose.mainPaused));
 auto bufferWorld=[&](Vector3 a){
  return MouthTransformPoint(pelvisWorld,MouthTransformPoint(mouthContact.inverseBind,a));
 };
 auto point=[&](Vector3 a){
  auto v=ExternalMesh::Transform({a.x,a.y,a.z},mesh->values);
  return Vector3{v[0],v[1],v[2]};
 };
 auto geometry=mouthContact.geometry;
 auto pivot=point(geometry.base),tip=point(geometry.tip);
 Probe probe{bufferWorld(pivot),bufferWorld(tip)};
 RefreshMouthBaseline();
 const bool rendererReady=ConnectMouthRenderer();
 auto origin=bufferWorld({});
 auto x=Subtract(bufferWorld({1,0,0}),origin);
 auto y=Subtract(bufferWorld({0,1,0}),origin);
 auto z=Subtract(bufferWorld({0,0,1}),origin);
 const auto aim=[&](Vector3 worldDirection){
  Vector3 localDirection{};
  if(!ToLocalDelta(x,y,z,worldDirection,localDirection))return PoseMath::Quaternion{};
  return AimRotation(Subtract(tip,pivot),localDirection);
 };
 const float step=std::isfinite(dt)&&dt>0?1-std::exp(-24.f*std::min(dt,.05f)):0;
 const auto publish=[&]{
  const auto& r=mouthContact.followRotation;
  const bool rotated=r.x*r.x+r.y*r.y+r.z*r.z>1e-7f;
  const bool published=rotated&&PublishMouthFollow(pivot,r);
  if(!published&&(!mouthContact.following||mouthContact.submitted)){
   const bool keepEngaged=mouthContact.following&&!rotated;
   ReleaseMouthFollow();
   mouthContact.following=keepEngaged;
  }
  return published;
 };
 const auto& offset=mouthContact.attachmentOffsets[static_cast<size_t>(mouthContact.target)-1];
 if(mouthContact.target!=Mouth){
  UpdateMouthAudio(false);
  Vector3 anchor{},audioAnchor{};
  bool valid=false;
  const bool pelvic=mouthContact.target==Pelvis;
  if(pelvic){
   ReferenceMatrix transform{};
   valid=Living(mouthContact.targetPelvis)&&Value(LocalToWorld,mouthContact.targetPelvis,transform);
   if(valid){
    audioAnchor=MouthTransformPoint(transform,mouthContact.pelvicAnchorLocal);
    anchor=MouthTransformPoint(transform,OffsetAnchor(mouthContact.pelvicAnchorLocal,mouthContact.pelvicAxesLocal,offset));
   }
  }else valid=ReadPalmAnchor(mouthContact.handAnchorBones,mouthContact.target==LeftHand,offset,anchor);
  valid=valid&&Finite(anchor);
  if(valid)MarkAttachmentWorld(mouthContact.target,anchor);
  const auto direction=Subtract(anchor,probe.base);
  Vector3 relative{};
  if(valid&&pelvic)valid=ToLocalDelta(x,y,z,direction,relative);
  const auto approach=valid&&pelvic?FollowPelvis({pivot,tip},Add(pivot,relative)):PelvicApproach{};
  const auto gate=valid&&!pelvic?FollowPoint(probe,anchor,mouthContact.following):approach.gate;
  mouthContact.following=rendererReady&&gate==FollowGate::Accepted;
  if(pelvic&&mouthContact.following)mouthContact.followRotation=approach.rotation;
  else{
   const auto rotation=mouthContact.following?aim(direction):PoseMath::Quaternion{};
   mouthContact.followRotation=PoseMath::Nlerp(mouthContact.followRotation,rotation,step);
  }
  const bool published=publish();
  float rootGap=0;
  if(pelvic&&valid&&rendererReady&&ToLocalDelta(x,y,z,Subtract(audioAnchor,probe.base),relative)){
   const auto audioAim=FollowPelvis({pivot,tip},Add(pivot,relative));
   if(audioAim.gate==FollowGate::Accepted){
    const auto audioTip=bufferWorld(RotateAround(pivot,audioAim.rotation,tip));
    rootGap=RootSurfaceDistance({probe.base,audioTip},audioAnchor);
    const bool entered=EnterRootContact(rootGap,mouthContact.rootTouching);
    if(entered&&audioPlaying&&PlayPelvicAudio())++mouthContact.rootAudioPlays;
   }else mouthContact.rootTouching=false;
  }else mouthContact.rootTouching=false;
  const auto now=GetTickCount64();
  if(!rendererReady)mouthContact.status="DIH 追蹤需要本版內建 EFMI";
  else if(!valid)mouthContact.status="吸附目標骨架姿勢已失效";
  else if(published&&mouthContact.renderCount&&now-mouthContact.submittedAt>500&&
          mouthContact.renderCount()==mouthContact.renderCountAtSubmit)
   mouthContact.status="已送出追蹤資料，等待 EFMI 繪製 DIH";
  else if(mouthContact.following)mouthContact.status=pelvic?"DIH 軸線已對準骨盆定位點":"DIH 末端追蹤接觸點";
  else mouthContact.status="等待 DIH 靠近吸附點";
  if(now-mouthContact.lastTrace>1500){
   mouthContact.lastTrace=now;
   Log("Attachment follow: target="+std::string(AttachmentKey(mouthContact.target))+
       " gate="+FollowGateName(gate)+" distance_cm="+std::to_string(MouthContact::Length(Subtract(anchor,probe.tip))*100)+
       (pelvic?" reach_cm="+std::to_string(approach.reach*100)+" length_cm="+std::to_string(approach.length*100)+
        " root_gap_cm="+std::to_string(rootGap*100)+" root_touch="+std::to_string(mouthContact.rootTouching)+
        " audio_plays="+std::to_string(mouthContact.rootAudioPlays):"")+
       " submitted="+std::to_string(published)+" gpu_updates="+
       std::to_string(mouthContact.renderCount?mouthContact.renderCount():0));
  }
  return;
 }
 Vector3 head{};
 PoseMath::Quaternion headRotation{};
 if(!Value(Position,pose.targetHead,head)||!Value(WorldRotation,pose.targetHead,headRotation)||
    !PoseMath::Normalize(headRotation)){
  ClearMouthBinding();mouthContact.status="嘴部姿勢已失效";return;
 }
 Entrance entrance{
  Add(head,PoseMath::Rotate(headRotation,mouthContact.mouthOffset)),
  PoseMath::Rotate(headRotation,mouthContact.mouthRight),
  PoseMath::Rotate(headRotation,mouthContact.mouthUp),
  PoseMath::Rotate(headRotation,mouthContact.mouthOut)};
 const bool lipReference=UpdateMouthEntrance(entrance,head);
 entrance.center=OffsetAnchor(entrance.center,{entrance.right,entrance.up,entrance.outward},offset);
 MarkAttachmentWorld(Mouth,entrance.center);
 Vector3 desired{};
 const float separation=MouthContact::Length(Subtract(entrance.center,probe.tip));
 const auto gate=FollowTarget(entrance,probe,mouthContact.following,desired);
 mouthContact.following=rendererReady&&gate==FollowGate::Accepted;
 const auto targetRotation=mouthContact.following?aim(Add(Subtract(probe.tip,probe.base),desired)):PoseMath::Quaternion{};
 mouthContact.followRotation=PoseMath::Nlerp(mouthContact.followRotation,targetRotation,step);
 auto smoothedTip=bufferWorld(RotateAround(pivot,mouthContact.followRotation,tip));
 if(std::abs(Dot(Subtract(smoothedTip,probe.tip),entrance.outward))>.0004f){
  auto constrained=ProjectToTipDepth(entrance,probe,smoothedTip);
  mouthContact.followRotation=aim(Subtract(constrained,probe.base));
  auto corrected=bufferWorld(RotateAround(pivot,mouthContact.followRotation,tip));
  if(std::abs(Dot(Subtract(corrected,probe.tip),entrance.outward))>.0004f)mouthContact.followRotation={};
 }
 const auto& rotation=mouthContact.followRotation;
 const bool published=publish();
 if(published)probe.tip=bufferWorld(RotateAround(pivot,rotation,tip));
 float target=0,thickness=0;
 bool touching=false;
 if(lipReference){
  const auto& bytes=published?mouthContact.followBytes:mouthContact.sourceBytes;
  auto zero=Subtract(origin,entrance.center);
  for(size_t i=0;i<mouthContact.vertices.size();++i){
   auto v=mouthContact.vertices[i];
   auto point=Add(zero,Add(Scale(x,ExternalMesh::Get<float>(bytes,v)),
                          Add(Scale(y,ExternalMesh::Get<float>(bytes,v+4)),
                              Scale(z,ExternalMesh::Get<float>(bytes,v+8)))));
   mouthContact.lipFrameVertices[i]={Dot(point,entrance.right),Dot(point,entrance.up),Dot(point,entrance.outward)};
  }
  const auto lipFrame=[&](Vector3 world){
   const auto p=Subtract(world,entrance.center);
   return Vector3{Dot(p,entrance.right),Dot(p,entrance.up),Dot(p,entrance.outward)};
  };
  const Probe contactProbe{lipFrame(probe.base),lipFrame(probe.tip)};
  const Vector3 half{entrance.width*.72f,entrance.height,.0015f};
  touching=LipBoxContact(mouthContact.lipFrameVertices,mouthContact.triangles,half,mouthContact.contactRayHits);
  if(touching){
   thickness=ShaftThickness(mouthContact.lipFrameVertices,mouthContact.triangles,contactProbe,mouthContact.sectionHits);
   target=OpeningForThickness(thickness);
  }
 }
 UpdateMouthAudio(audioPlaying&&rendererReady&&touching);
 mouthContact.opening=target>0?PoseMath::Smooth(mouthContact.opening,target,dt):0;
 auto now=GetTickCount64();
 if(!rendererReady)mouthContact.status="DIH 追蹤需要本版內建 EFMI";
 else if(published&&mouthContact.renderCount&&now-mouthContact.submittedAt>500&&
         mouthContact.renderCount()==mouthContact.renderCountAtSubmit)
  mouthContact.status="已送出追蹤資料，等待 EFMI 繪製 DIH";
 else if(mouthContact.following)mouthContact.status="DIH 末端追蹤嘴部";
 else mouthContact.status=target>0?"嘴部接觸中":"等待 DIH 靠近嘴部正面";
 if(now-mouthContact.lastTrace>1500){
  mouthContact.lastTrace=now;
  Log("Mouth follow: distance_cm="+std::to_string(separation*100)+
      " gate="+FollowGateName(gate)+" base_front_cm="+std::to_string(Dot(Subtract(probe.base,entrance.center),entrance.outward)*100)+
      " tip_front_cm="+std::to_string(Dot(Subtract(probe.tip,entrance.center),entrance.outward)*100)+
      " lip_reference="+std::to_string(lipReference)+" open_gap_cm="+std::to_string(entrance.gap*100)+
      " lip_collision="+std::to_string(touching)+" section_mm="+std::to_string(thickness*1000)+
      " requested_gap_mm="+std::to_string(target*1000)+" submitted="+std::to_string(published)+
      " face_writes="+std::to_string(mouthContact.faceWrites)+
      " gpu_updates="+std::to_string(mouthContact.renderCount?mouthContact.renderCount():0));
 }
 if(mouthContact.opening<.002f)return;
 bool applied=false;
 for(auto& shape:mouthContact.faceShapes){
  if(!Living(shape.renderer)||Call(SharedMesh,shape.renderer)!=shape.mesh)continue;
  void* get[]{&shape.index};
  if(!Value(GetBlendWeight,shape.renderer,shape.base,get)||!std::isfinite(shape.base))continue;
  shape.written=std::max(shape.base,std::clamp(mouthContact.opening/.065f,0.f,1.f)*100);
  if(shape.written==shape.base){applied=true;continue;}
  void* set[]{&shape.index,&shape.written};bool succeeded=false;
  Call(SetBlendWeight,shape.renderer,set,&succeeded);
  shape.changed=succeeded;applied|=succeeded;
  if(succeeded)++mouthContact.faceWrites;
 }
 if(applied)return;
 PoseMath::Quaternion jawRotation{},animationBase{};
 if(!Living(mouthContact.jaw)||!Value(WorldRotation,mouthContact.jaw,jawRotation)||
    !PoseMath::Normalize(jawRotation)||!Rotation(mouthContact.jaw,animationBase)){
  mouthContact.status=published?"DIH 正在追蹤，角色缺少可用張嘴形變":"角色張嘴形變或下顎骨已失效";
  return;
 }
 auto axis=PoseMath::Rotate(PoseMath::Inverse(jawRotation),entrance.right);
 Vector3 jawPivot{};
 if(!Value(Position,mouthContact.jaw,jawPivot))return;
 const auto lever=std::abs(Dot(Subtract(entrance.center,jawPivot),entrance.outward));
 const float angle=JawAngle(mouthContact.opening,entrance.gap,lever)*.5f;
 if(angle<.001f)return;
 auto delta=PoseMath::Quaternion{axis.x*std::sin(angle),axis.y*std::sin(angle),axis.z*std::sin(angle),std::cos(angle)};
 mouthContact.base=animationBase;
 mouthContact.written=PoseMath::Multiply(animationBase,delta);
 mouthContact.wrote=WriteRotation(mouthContact.jaw,mouthContact.written);
 if(mouthContact.wrote){
  ++mouthContact.faceWrites;
  const auto worldDelta=PoseMath::Quaternion{
   entrance.right.x*std::sin(angle),entrance.right.y*std::sin(angle),
   entrance.right.z*std::sin(angle),std::cos(angle)};
  ApplyLowerFaceHinge(jawPivot,worldDelta);
 }
}
