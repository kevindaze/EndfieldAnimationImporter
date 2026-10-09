#include "character_names.h"
#include "external_models.h"
#include "efmi_bridge.h"
#include "external_mesh.h"

#include <BetterEndfield/ThirdPartyModule.h>
#include <BetterEndfield/PoseLease.h>
#include "pose_math.h"
#include "handshake_motion.h"
#include "grip_settings.h"
#include "preset_settings.h"
#include "overlay_panel.h"
#include "free_camera_raw_input.h"
#include "imported_animation.h"
#include "workflow_settings.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "follow_metadata.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <memory>
#include <condition_variable>
#include <chrono>

namespace {
constexpr char id[]="local.endfield.interaction";
constexpr char core[]="UnityEngine.CoreModule.dll", game[]="Gameplay.Beyond.dll";
const BE_ThirdPartyHostV1* host=nullptr;
const BE_HostApiV1* runtime=nullptr;
std::mutex gate;
std::atomic_bool active{false}, pending{false};
std::atomic_bool restoreRequested{false}, targetsPending{false};
int selectedSlot=-1;
std::string selectedModel,selectedIdentity;
Presets::Bank presetBank;
std::array<bool,9> presetKeys{};
int activePreset=-1,pendingPreset=-1,pendingStart=-1;
std::map<int,std::string> targetChoices;
std::string panelBaseline;bool currentUiSettings=false;
std::string panelStatus="請先連接並掃描隊伍",lastControlled;
InteractionPanel::Snapshot PanelRead();void PanelExecute(InteractionPanel::Command command);bool SavePanelSettings();void LoadPanelSettings();std::filesystem::path PanelSettingsPath();
std::condition_variable restored;
bool armed=false,keyDown=false,poseContracts=false;
enum class Mode {HeadTest,LookAt,Mutual,Standing,Handshake,Custom};
Handshake::Clip handshakeA,handshakeB;
std::filesystem::path moduleFolder;
std::string externalFolder="G:/Project/endfield-mod/EFMI/EFMI/Mods";EfmiBridge::Scan efmiScan;
std::vector<ExternalModels::Part> externalParts;Presets::Json externalCatalog=Presets::Json::array();
std::map<std::string,std::shared_ptr<ImportedAnimation::Clip>> animationLibrary;
std::string supportTarget="none";std::string selectedControlledClip,controlledPose="inherit",controlledSupport="none";bool controlledLoop=false;std::string selectedClip,lastExportPath,lastSkeletonReferencePath;Bones::Settings boneTarget;bool sceneAuditPending=false;PoseMath::Vector3 sceneBefore{};uint64_t sceneRevision=0,sceneAuditUntil=0,sceneAuditLast=0;Presets::Json sceneBeforeActors;bool pendingTimeline=false;std::optional<float> pendingSeek,pendingMainSeek;bool pendingMainStop=false,pendingPartnerStop=false;bool bonesPending=false;struct BoneInfo{std::string key,name,group;int actor=0,depth=0;};std::vector<BoneInfo> boneCatalog;uint64_t boneGeneration=0;
std::vector<std::string> animationOrder;Workflow::State workflow;bool pendingPreview=false;void StoreTimeline(bool main=false);
bool DeleteAnimation(const std::string& key);bool ReorderAnimations(const Presets::Json& order);void LoadAnimationOrder();bool SaveAnimationOrder();
bool ImportAnimation(const Presets::Json& data);void LoadAnimationLibrary();
bool IsPlaced(Mode m){return m==Mode::Standing||m==Mode::Handshake||m==Mode::Custom;}
bool IsDual(Mode m){return m==Mode::Mutual||IsPlaced(m);}
Mode armedMode=Mode::HeadTest;
bool lookContracts=false;
bool standingContracts=false;
Grip::Settings gripTarget,gripCurrent;bool calibrationMode=false,calibrationLoop=false;
bool movementContracts=false;
std::atomic_bool movementPending{false};
std::atomic_bool followPending{false};
DWORD unityThread=0;
const BE_PoseLeaseApiV1* leases=nullptr;
using Tick=void(__fastcall*)(void*,float,void*);
Tick next=nullptr;
uint64_t hook=0;
std::ofstream output;
struct Method {BE_MethodDescriptorV1 spec; BE_ResolvedMethodV1 resolved{};};
Method methods[]{
 {{game,"Beyond.Gameplay.View","CameraManager","TailLateTick",nullptr,"System.Void",1}},
 {{game,"Beyond.Gameplay.Core","PlayerController","GetMainCharacter",nullptr,nullptr,0}},
 {{game,"Beyond.Gameplay.Core","Entity","get_modelCom",nullptr,nullptr,0}},
 {{game,"Beyond.Gameplay.View","BaseModelComponent","GetModelGo",nullptr,"UnityEngine.GameObject",0}},
 {{core,"UnityEngine","Object","get_name",nullptr,"System.String",0}},
 {{core,"UnityEngine","GameObject","get_transform",nullptr,"UnityEngine.Transform",0}},
 {{core,"UnityEngine","Component","get_transform",nullptr,"UnityEngine.Transform",0}},
 {{core,"UnityEngine","GameObject","GetComponentsInChildren","System.Type|System.Boolean","UnityEngine.Component[]",2}},
 {{"mscorlib.dll","System","Array","GetLength","System.Int32","System.Int32",1}},
 {{"mscorlib.dll","System","Array","GetValue","System.Int32","System.Object",1}},
 {{core,"UnityEngine","Transform","get_childCount",nullptr,"System.Int32",0}},
 {{core,"UnityEngine","Transform","GetChild","System.Int32","UnityEngine.Transform",1}},
 {{game,"Beyond.Gameplay","GameInstance","get_player",nullptr,"Beyond.Gameplay.GamePlayer",0}},
 {{game,"Beyond.Gameplay.Core","SquadManager","get_slotCount",nullptr,"System.Int32",0}},
 {{game,"Beyond.Gameplay.Core","SquadManager","GetMemberBySlot","System.Int32","Beyond.Gameplay.Core.Entity",1}},
 {{game,"Beyond.Gameplay.Core","SquadManager","get_isSquadLoading",nullptr,"System.Boolean",0}},
 {{core,"UnityEngine","GameObject","get_scene",nullptr,"UnityEngine.SceneManagement.Scene",0}},
 {{core,"UnityEngine.SceneManagement","Scene","get_name",nullptr,"System.String",0}},
 {{core,"UnityEngine","Object","op_Implicit","UnityEngine.Object","System.Boolean",1}},
 {{core,"UnityEngine","Transform","get_parent",nullptr,"UnityEngine.Transform",0}},
 {{core,"UnityEngine","Transform","get_localRotation",nullptr,"UnityEngine.Quaternion",0}},
 {{core,"UnityEngine","Transform","set_localRotation","UnityEngine.Quaternion","System.Void",1}},
 {{core,"UnityEngine","Transform","get_position",nullptr,"UnityEngine.Vector3",0}},
 {{core,"UnityEngine","Transform","get_rotation",nullptr,"UnityEngine.Quaternion",0}},
 {{core,"UnityEngine","Transform","set_position","UnityEngine.Vector3","System.Void",1}},
 {{core,"UnityEngine","Transform","set_rotation","UnityEngine.Quaternion","System.Void",1}},
 {{core,"UnityEngine","Transform","get_localPosition",nullptr,"UnityEngine.Vector3",0}},
 {{core,"UnityEngine","Transform","set_localPosition","UnityEngine.Vector3","System.Void",1}},
 {{game,"Beyond.Gameplay.Core","Entity","IsValid",nullptr,"System.Boolean",0}},
 {{game,"Beyond.Gameplay.Core","Entity","get_movementComponent",nullptr,"Beyond.Gameplay.Core.MovementComponent",0}},
 {{game,"Beyond.Gameplay.Core","MovementComponent","get_isMovingOnGround",nullptr,"System.Boolean",0}},
 {{game,"Beyond.Gameplay.Core","MovementComponent","get_isInAir",nullptr,"System.Boolean",0}},
 {{game,"Beyond.Gameplay.Core","MovementComponent","get_moveMode",nullptr,"Beyond.Gameplay.Core.MovementComponent.MoveMode",0}},
 {{game,"Beyond.Gameplay.Core","Entity","get_charCtrl",nullptr,"Beyond.Gameplay.Core.CharacterController",0}},
 {{core,"UnityEngine","Transform","get_localScale",nullptr,"UnityEngine.Vector3",0}},
 {{core,"UnityEngine","Transform","set_localScale","UnityEngine.Vector3","System.Void",1}},
 {{core,"UnityEngine","SkinnedMeshRenderer","get_sharedMesh",nullptr,"UnityEngine.Mesh",0}},
 {{core,"UnityEngine","SkinnedMeshRenderer","get_bones",nullptr,"UnityEngine.Transform[]",0}},
 {{core,"UnityEngine","Mesh","get_bindposes",nullptr,"UnityEngine.Matrix4x4[]",0}},
 {{core,"UnityEngine","Transform","get_localToWorldMatrix",nullptr,"UnityEngine.Matrix4x4",0}},
 {{core,"UnityEngine","Transform","get_worldToLocalMatrix",nullptr,"UnityEngine.Matrix4x4",0}},
 {{core,"UnityEngine","Camera","get_main",nullptr,"UnityEngine.Camera",0}},
 {{"Input.Beyond.dll","Beyond.Input","InputManager","get_rootGroupId",nullptr,"System.Int32",0}},
 {{"Input.Beyond.dll","Beyond.Input","InputManager","IsGroupEnabled","System.Int32","System.Boolean",1}},
 {{"Input.Beyond.dll","Beyond.Input","InputManager","ToggleAllInput","System.Boolean","System.Void",1}},
 {{core,"UnityEngine","Object","FindObjectsOfType","System.Type","UnityEngine.Object[]",1}},
 {{core,"UnityEngine","Behaviour","get_enabled",nullptr,"System.Boolean",0}},
 {{core,"UnityEngine","Behaviour","set_enabled","System.Boolean","System.Void",1}},
};
enum Key {Tail,Main,Model,ModelGo,Name,GoTransform,ComponentTransform,Components,Length,Item,ChildCount,Child,Player,SlotCount,Member,Loading,Scene,SceneName,Alive,Parent,LocalRotation,SetLocalRotation,Position,WorldRotation,SetPosition,SetWorldRotation,LocalPosition,SetLocalPosition,EntityValid,Movement,Moving,Airborne,MoveMode,CharacterController,LocalScale,SetLocalScale,SharedMesh,MeshBones,BindPoses,LocalToWorld,WorldToLocal,CameraMain,InputRootGroup,InputGroupEnabled,InputToggleAll,FindCanvases,UiEnabled,SetUiEnabled};
BE_ResolvedClassV1 animator{},skinnedRenderer{},uiCanvas{};
BE_ResolvedFieldV1 squad{};
void Log(const std::string& s){if(host&&host->log)host->log(host->context,s.c_str());if(output){output<<s<<'\n';output.flush();}}
void* InvokeGuarded(const void* method,void* self,void** args,void** exception){
 __try{return runtime->runtime_invoke(runtime->context,method,self,args,exception);}
 __except(EXCEPTION_EXECUTE_HANDLER){*exception=reinterpret_cast<void*>(1);return nullptr;}
}
void* Call(Key key,void* self=nullptr,void** args=nullptr,bool* success=nullptr){
 if(success)*success=false;
 if(!methods[key].resolved.method_info)return nullptr;
 void* exception=nullptr;
 auto value=InvokeGuarded(methods[key].resolved.method_info,self,args,&exception);
 if(exception){Log(std::string("Invocation failed: ")+methods[key].spec.method_name);return nullptr;}
 if(success)*success=true;return value;
}
template<class T> bool Value(Key key,void* self,T& value,void** args=nullptr){
 auto boxed=Call(key,self,args);auto data=boxed?runtime->object_unbox(runtime->context,boxed):nullptr;
 if(!data)return false;value=*static_cast<T*>(data);return true;
}
std::string ObjectName(void* object){char text[1024]{};auto str=Call(Name,object);if(str)runtime->copy_managed_string(runtime->context,str,text,sizeof(text));return text;}
struct Pins {
 std::vector<uint32_t> handles;
 void* Keep(void* object){if(!object)return nullptr;auto h=runtime->gchandle_new(runtime->context,object,1);if(!h)return nullptr;handles.push_back(h);return object;}
 ~Pins(){for(auto h:handles)runtime->gchandle_free(runtime->context,h);}
};
struct MovementState {bool ready=false,moving=false,airborne=false;int mode=-1;};
MovementState ReadMovement(void* entity){
 MovementState state;if(!movementContracts||!entity)return state;
 bool valid=false;if(!Value(EntityValid,entity,valid)||!valid)return state;
 Pins pins;auto component=pins.Keep(Call(Movement,entity));
 state.ready=component&&Value(Moving,component,state.moving)&&Value(Airborne,component,state.airborne)&&Value(MoveMode,component,state.mode);
 return state;
}
std::string MovementJson(const MovementState& s){return std::string("{\"ready\":")+(s.ready?"true":"false")+",\"moving\":"+(s.moving?"true":"false")+",\"airborne\":"+(s.airborne?"true":"false")+",\"move_mode\":"+std::to_string(s.mode)+"}";}
struct HeadControl {
 void* root=nullptr;void* parent=nullptr;void* head=nullptr;
 uint64_t lease=0;float yaw=0,pitch=0;
 PoseMath::Quaternion base{},written{};bool wrote=false;
};
struct SkeletonReference{PoseMath::Vector3 position{};PoseMath::Quaternion rotation{};};std::map<std::string,SkeletonReference> skeletonReferences;
Presets::Json EncodeSkeletonReferences(){auto data=Presets::Json::object();for(auto& [key,r]:skeletonReferences)data[key]={{"position",{r.position.x,r.position.y,r.position.z}},{"rotation",{r.rotation.x,r.rotation.y,r.rotation.z,r.rotation.w}}};return data;}
void ReadSkeletonReferences(const Presets::Json& data){if(!data.is_object()||data.size()>8192)return;for(auto& [key,v]:data.items()){try{auto p=v.at("position"),q=v.at("rotation");if(p.size()!=3||q.size()!=4)continue;SkeletonReference r{{p[0],p[1],p[2]},{q[0],q[1],q[2],q[3]}};if(key.starts_with("chr_")&&PoseMath::Finite(r.position)&&PoseMath::Normalize(r.rotation))skeletonReferences[key]=r;}catch(...){}}}
struct RootPlacement {
 void* root=nullptr;
 PoseMath::Vector3 start{},anchor{},baseLocal{},writtenLocal{};
 PoseMath::Quaternion startRotation{},anchorRotation{},baseLocalRotation{},writtenLocalRotation{};
 bool positioned=false,rotated=false;
};
struct ArmBone {void* bone=nullptr;void* child=nullptr;void* root=nullptr;uint64_t lease=0;PoseMath::Quaternion base{},written{},reference{},restLocal{};PoseMath::Vector3 axis{};float length=0;bool wrote=false;PoseMath::Vector3 planeNormal{};};
struct FingerControl {void* bone=nullptr;void* root=nullptr;uint64_t lease=0;PoseMath::Quaternion rest{},offset{},base{},written{};bool wrote=false;PoseMath::Vector3 axis{};int actor=0,digit=0,joint=0;};
struct SupportState {
 struct Leg {void* point=nullptr;PoseMath::Vector3 anchor{};};
 std::string mode="none";Leg legs[2];int count=0;bool ready=false;PoseMath::Vector3 captureScale{1,1,1},offset{};
};
struct Pose {
 std::unique_ptr<Pins> pins;
 void* entity=nullptr;void* model=nullptr;void* root=nullptr;void* head=nullptr;
 void* targetEntity=nullptr;void* targetModel=nullptr;void* targetHead=nullptr;void* parent=nullptr;
 Mode mode=Mode::HeadTest;
 float yaw=0,pitch=0;
 uint64_t lease=0,started=0;
 PoseMath::Quaternion base{},written{};
 bool wrote=false;
 HeadControl reverse{};
 RootPlacement placement{},partnerPlacement{};float placementTime=0,clipTime=0;float rangeStart=0,rangeEnd=0;bool rangeLoop=false,scrubbing=false;bool clipPaused=false;unsigned loopCount=0;uint64_t playbackAuditAt=0;std::string playbackAudit;
 PoseMath::Vector3 managerContact[2]{},managerBend[2]{};bool managerContactReady[2]{};float managerTangent[2]{};std::vector<ArmBone> arms;PoseMath::Vector3 managerBodyRight{},managerBodyForward{};void* waist=nullptr;void* waistSpine=nullptr;float tangentSign=0;PoseMath::Vector3 bendNormalLocal{};
 struct Twist{size_t finger=0;void* upper=nullptr;PoseMath::Vector3 axis{};PoseMath::Quaternion upperRest{},offset{};float weight=.5f;};std::vector<Twist> twists;std::vector<FingerControl> fingers;PoseMath::Vector3 palmNormal[2]{};
 PoseMath::Vector3 contact{};bool contactReady=false;uint64_t contactLog=0;
 struct CustomBone {void* bone=nullptr;void* root=nullptr;uint64_t lease=0;PoseMath::Quaternion rest{},base{},written{};bool wrote=false;size_t track=0;int actor=0,field=-1;void* parent=nullptr;PoseMath::Vector3 positionBase{},positionWritten{};bool positionWrote=false,holdPosition=false,holdRotation=false;PoseMath::Vector3 heldPosition{},axis{};};
 struct FullBone {BoneInfo info;void* bone=nullptr;void* root=nullptr;uint64_t lease=0;PoseMath::Vector3 basePosition{},writtenPosition{},baseScale{1,1,1},writtenScale{1,1,1};PoseMath::Quaternion baseRotation{},writtenRotation{};bool pos=false,rot=false,scale=false;};std::vector<FullBone> fullBones;
 SupportState support,mainSupport;float mainTime=0,mainStart=0,mainEnd=0;bool mainPaused=false,mainRangeLoop=false,mainScrubbing=false,mainStopped=false,partnerStopped=false;std::shared_ptr<ImportedAnimation::Clip> controlledClip;std::vector<CustomBone> customBones;std::shared_ptr<ImportedAnimation::Clip> clip;
} pose;
bool WalkBones(void* root,int actor,Pins& pins,std::vector<Pose::FullBone>& result){int budget=1200;auto walk=[&](auto&& self,void* node,const std::string& path,int depth,bool inSkeleton)->void{if(!node||depth>48||budget--<=0)return;auto name=ObjectName(node);bool externalNode=std::any_of(externalParts.begin(),externalParts.end(),[&](auto& p){return p.backend=="unity_transform"&&p.bone==name&&p.character==(actor?workflow.draft.target:lastControlled);});bool isSkeleton=inSkeleton||name.starts_with("Bip001")||externalNode;if(depth==0||isSkeleton){Pose::FullBone bone;bone.info={std::to_string(actor)+"|"+path,depth==0?"@root":name,externalNode?"模型骨架":Bones::Group(depth==0?"@root":name),actor,depth};bone.bone=node;bone.root=root;result.push_back(bone);}int count=0;if(!Value(ChildCount,node,count)||count<0||count>2048)return;std::map<std::string,int> siblings;for(int i=0;i<count&&budget>0;++i){void* args[]{&i};auto child=pins.Keep(Call(Child,node,args));if(!child)continue;auto childName=ObjectName(child);auto segment=childName+"["+std::to_string(siblings[childName]++)+"]";self(self,child,path=="@root"?segment:path+"/"+segment,depth+1,isSkeleton);}};walk(walk,root,"@root",0,false);return budget>0;}
void Event(const char* status){Log(std::string("Pose status: ")+status);panelStatus=status;if(host&&host->emit){const std::string json=std::string("{\"pose_status\":\"")+status+"\"}";host->emit(host->context,json.c_str());}}
void CharacterMismatch(const char* status,const std::string& controlled,const std::string& target,const std::string& actual={},bool missing=false){std::string detail;if(std::string(status)=="preset_controlled_character_mismatch")detail="請將主控切換為「"+CharacterNames::Name(controlled)+"」；互動角色需為「"+CharacterNames::Name(target)+"」";else if(missing)detail="請將「"+CharacterNames::Name(target)+"」加入隊伍作為互動角色，再更新隊伍角色；若有重複實例，請重新掃描";else detail="此動畫需要互動角色「"+CharacterNames::Name(target)+"」，請選擇並套用該角色";if(!actual.empty())detail+="；目前為「"+CharacterNames::Name(actual)+"」";Log(std::string(status)+": "+detail);panelStatus=detail;if(host&&host->emit){auto data=Presets::Json{{"pose_status",status},{"detail",detail},{"required_controlled",controlled},{"required_target",target},{"actual_character",actual}}.dump();host->emit(host->context,data.c_str());}}

bool Living(void* object){bool living=false;void* args[]{object};return object&&Value(Alive,nullptr,living,args)&&living;}
bool Rotation(void* head,PoseMath::Quaternion& q){return Value(LocalRotation,head,q)&&PoseMath::Normalize(q);}
bool WriteRotation(void* head,PoseMath::Quaternion q){
 if(!PoseMath::Normalize(q))return false;void* args[]{&q};bool ok=false;Call(SetLocalRotation,head,args,&ok);return ok;
}
template<class T> bool WriteValue(Key key,void* object,T value){void* args[]{&value};bool ok=false;Call(key,object,args,&ok);return ok;}
#include "scene_layout.h"
#include "support_motion.inc"
bool RestorePlacement(RootPlacement& p,bool mainThread,uint64_t lease){
 if(!p.positioned&&!p.rotated)return true;
 if(!mainThread||GetCurrentThreadId()!=unityThread||!Living(p.root)||!leases->owns(p.root,id,lease))return false;
 bool ok=true;PoseMath::Vector3 position{};PoseMath::Quaternion rotation{};
 if(p.positioned){
  if(!Value(LocalPosition,p.root,position))ok=false;
  else if(PoseMath::SamePosition(position,p.writtenLocal))ok=WriteValue(SetLocalPosition,p.root,p.baseLocal)&&ok;
 }
 if(p.rotated){
  if(!Rotation(p.root,rotation))ok=false;
  else if(PoseMath::SameRotation(rotation,p.writtenLocalRotation))ok=WriteRotation(p.root,p.baseLocalRotation)&&ok;
 }return ok;
}
bool RestoreHead(void* root,void* head,uint64_t lease,bool wrote,const PoseMath::Quaternion& written,const PoseMath::Quaternion& base,bool mainThread){
 bool clean=!wrote;
 if(mainThread&&GetCurrentThreadId()==unityThread&&wrote&&Living(head)&&leases&&leases->owns(root,id,lease)){
  PoseMath::Quaternion current{};
  if(Rotation(head,current)){
   // Restore only our still-visible write. A fresh animator pose is already
   // restored; never replace it with an obsolete activation-time snapshot.
   clean=!PoseMath::SameRotation(current,written)||WriteRotation(head,base);
  }
 }
 if(leases&&lease)leases->release(root,id,lease);return clean;
}
bool RemoveBoneLayer(bool mainThread){bool clean=true;for(auto& b:pose.fullBones){if(!b.pos&&!b.rot&&!b.scale)continue;if(!mainThread||!Living(b.bone)||!leases->owns(b.root,id,b.lease)){clean=false;continue;}PoseMath::Vector3 v{};PoseMath::Quaternion q{};if(b.pos){if(!Value(LocalPosition,b.bone,v))clean=false;else if(!PoseMath::SamePosition(v,b.writtenPosition))b.pos=false;else if(WriteValue(SetLocalPosition,b.bone,b.basePosition))b.pos=false;else clean=false;}if(b.rot){if(!Rotation(b.bone,q))clean=false;else if(!PoseMath::SameRotation(q,b.writtenRotation))b.rot=false;else if(WriteRotation(b.bone,b.baseRotation))b.rot=false;else clean=false;}if(b.scale){if(!Value(LocalScale,b.bone,v))clean=false;else if(!PoseMath::SamePosition(v,b.writtenScale))b.scale=false;else if(WriteValue(SetLocalScale,b.bone,b.baseScale))b.scale=false;else clean=false;}}return clean;}

void ApplyBoneLayer(){if(!pose.pins)return;for(auto& b:pose.fullBones){if(b.info.actor==0&&pose.mainStopped||b.info.actor==1&&pose.partnerStopped)continue;auto values=boneTarget.Get(b.info.key);if(pose.mode==Mode::Custom&&b.info.name=="@root"){for(int i=0;i<6;++i)values[i]=0;}if(values==Bones::Defaults())continue;if(!Living(b.bone)||!leases->owns(b.root,id,b.lease)){restoreRequested=true;Event("bone_layer_invalid");return;}PoseMath::Vector3 current{};PoseMath::Quaternion rotation{};
 if(values[0]!=0||values[1]!=0||values[2]!=0){if(!Value(LocalPosition,b.bone,b.basePosition)){restoreRequested=true;return;}b.writtenPosition=Handshake::Add(b.basePosition,{values[0]*.01f,values[1]*.01f,values[2]*.01f});if(!WriteValue(SetLocalPosition,b.bone,b.writtenPosition)){restoreRequested=true;return;}b.pos=true;}
 if(values[3]!=0||values[4]!=0||values[5]!=0){if(!Rotation(b.bone,b.baseRotation)){restoreRequested=true;return;}b.writtenRotation=PoseMath::Multiply(b.baseRotation,Handshake::XYZ(values[3],values[4],values[5]));if(!WriteRotation(b.bone,b.writtenRotation)){restoreRequested=true;return;}b.rot=true;}
 if(values[6]!=1||values[7]!=1||values[8]!=1){if(!Value(LocalScale,b.bone,b.baseScale)){restoreRequested=true;Event("bone_scale_unavailable");return;}b.writtenScale={b.baseScale.x*values[6],b.baseScale.y*values[7],b.baseScale.z*values[8]};if(!WriteValue(SetLocalScale,b.bone,b.writtenScale)){restoreRequested=true;return;}b.scale=true;}
 }}
void StopPose(const char* reason,bool mainThread){
 if(!pose.pins){calibrationMode=false;return;}
 StoreTimeline();StoreTimeline(true);pendingMainSeek.reset();pendingSeek.reset();
 bool layerClean=RemoveBoneLayer(mainThread);
 bool placementClean=RestorePlacement(pose.placement,mainThread,pose.lease);
 placementClean=RestorePlacement(pose.partnerPlacement,mainThread,pose.reverse.lease)&&placementClean;
 bool armClean=true;
 for(auto& a:pose.arms){if(!a.wrote)continue;if(!mainThread||!Living(a.bone)||!leases->owns(a.root,id,a.lease)){armClean=false;continue;}PoseMath::Quaternion q{};if(!Rotation(a.bone,q)){armClean=false;continue;}if(PoseMath::SameRotation(q,a.written))armClean=WriteRotation(a.bone,a.base)&&armClean;}
 for(auto& f:pose.fingers){if(!f.wrote)continue;if(!mainThread||!Living(f.bone)||!leases->owns(f.root,id,f.lease)){armClean=false;continue;}PoseMath::Quaternion q{};if(!Rotation(f.bone,q)){armClean=false;continue;}if(PoseMath::SameRotation(q,f.written))armClean=WriteRotation(f.bone,f.base)&&armClean;}
 for(auto& b:pose.customBones){if(b.positionWrote&&mainThread&&Living(b.bone)&&leases->owns(b.root,id,b.lease)){PoseMath::Vector3 current{};if(Value(LocalPosition,b.bone,current)&&PoseMath::SamePosition(current,b.positionWritten))armClean=WriteValue(SetLocalPosition,b.bone,b.positionBase)&&armClean;}if(!b.wrote)continue;if(!mainThread||!Living(b.bone)||!leases->owns(b.root,id,b.lease)){armClean=false;continue;}PoseMath::Quaternion current{};if(!Rotation(b.bone,current)){armClean=false;continue;}if(PoseMath::SameRotation(current,b.written))armClean=WriteRotation(b.bone,b.base)&&armClean;}
 bool clean=RestoreHead(pose.root,pose.head,pose.lease,pose.wrote,pose.written,pose.base,mainThread);
 auto& r=pose.reverse;
 const bool reverseClean=RestoreHead(r.root,r.head,r.lease,r.wrote,r.written,r.base,mainThread);
 clean=clean&&reverseClean&&placementClean&&armClean&&layerClean;
 Log(std::string("Head test stopped: ")+reason+(clean?"; override removed":"; restoration not verified, animator must resume"));
 Event(clean?"off":"off_restoration_unverified");pose=Pose{};activePreset=-1;calibrationMode=false;
}
void StopAnimationActor(int actor){
 if(!pose.pins)return;if(!RemoveBoneLayer(true)){StopPose("actor layer restore failed",true);return;}
 bool clean=true;for(auto& b:pose.customBones){if(b.actor!=actor)continue;if(!Living(b.bone)||!leases->owns(b.root,id,b.lease)){clean=false;continue;}PoseMath::Vector3 p{};PoseMath::Quaternion q{};
 if(b.positionWrote){if(!Value(LocalPosition,b.bone,p))clean=false;else if(PoseMath::SamePosition(p,b.positionWritten))clean=WriteValue(SetLocalPosition,b.bone,b.positionBase)&&clean;b.positionWrote=false;}
 if(b.wrote){if(!Rotation(b.bone,q))clean=false;else if(PoseMath::SameRotation(q,b.written))clean=WriteRotation(b.bone,b.base)&&clean;b.wrote=false;}}
 auto root=actor?pose.reverse.root:pose.root;for(auto& a:pose.arms)if(a.root==root&&a.wrote){PoseMath::Quaternion q{};if(!Rotation(a.bone,q))clean=false;else if(PoseMath::SameRotation(q,a.written))clean=WriteRotation(a.bone,a.base)&&clean;a.wrote=false;}for(auto& f:pose.fingers)if(f.root==root&&f.wrote){PoseMath::Quaternion q{};if(!Rotation(f.bone,q))clean=false;else if(PoseMath::SameRotation(q,f.written))clean=WriteRotation(f.bone,f.base)&&clean;f.wrote=false;}
 if(!clean){StopPose("actor restore failed",true);return;}(actor?pose.partnerStopped:pose.mainStopped)=true;(actor?pose.clipPaused:pose.mainPaused)=true;
 if(actor==0&&pose.clip&&pose.clip->absolute&&pose.clip->targetActor==0)pose.partnerStopped=true;if(actor==1&&!pose.controlledClip)pose.mainStopped=true;if(pose.mainStopped&&pose.partnerStopped)StopPose("both actors stopped",true);
}
void FindHead(void* transform,int depth,int& budget,Pins& pins,std::vector<void*>& found){
 if(!transform||depth>48||budget<=0)return;--budget;
 if(ObjectName(transform)=="Bip001_Head"){
  auto parent=pins.Keep(Call(Parent,transform));if(parent&&ObjectName(parent)=="Bip001_Neck")found.push_back(transform);
 }
 int count=0;if(!Value(ChildCount,transform,count)||count<0||count>2048)return;
 for(int i=0;i<count&&budget>0;++i){void* args[]{&i};FindHead(pins.Keep(Call(Child,transform,args)),depth+1,budget,pins,found);}
}
void* Squad(Pins& pins,int& count){
 if(!squad.field_info)return nullptr;
 auto player=pins.Keep(Call(Player));
 auto manager=player?pins.Keep(runtime->field_get_value_object(runtime->context,squad.field_info,player)):nullptr;
 bool loading=true;
 return manager&&Value(Loading,manager,loading)&&!loading&&Value(SlotCount,manager,count)&&count>0&&count<=16?manager:nullptr;
}
bool BindLookTarget(Pose& candidate){
 if(!lookContracts){Log("LookAt unavailable: squad/position/rotation contracts missing");Event("lookat_unavailable");return false;}
 auto& pins=*candidate.pins;int count=0;auto manager=Squad(pins,count);int matches=0;
 for(int i=0;manager&&i<count;++i){
  void* args[]{&i};auto entity=pins.Keep(Call(Member,manager,args));if(!entity||entity==candidate.entity)continue;
  auto model=pins.Keep(Call(Model,entity));auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;if(!Living(go))continue;
  if(!selectedIdentity.empty()){if(Presets::Identity(ObjectName(go))!=selectedIdentity)continue;}
  else if(selectedSlot>=0){if(i!=selectedSlot||ObjectName(go)!=selectedModel)continue;}
  else if(!ObjectName(go).starts_with("chr_0004_pelica_postmodel("))continue;
  auto root=pins.Keep(Call(GoTransform,go));std::vector<void*> found;int budget=1200;
  FindHead(root,0,budget,pins,found);if(found.size()!=1||budget<=0)continue;
  candidate.targetEntity=entity;candidate.targetModel=go;candidate.targetHead=found[0];++matches;
 }
 candidate.parent=pins.Keep(Call(Parent,candidate.head));
 if(matches!=1||!Living(candidate.parent)){Log("LookAt refused: unique instantiated Endministrator head not found in squad");Event("target_missing_or_ambiguous");return false;}
 PoseMath::Vector3 from{},to{};PoseMath::Quaternion rootRotation{};float yaw=0,pitch=0;
 if(!Value(Position,candidate.head,from)||!Value(Position,candidate.targetHead,to)||!Value(WorldRotation,candidate.root,rootRotation)||!PoseMath::Normalize(rootRotation)||
  (!IsPlaced(candidate.mode)&&!PoseMath::LookAngles(PoseMath::Rotate(PoseMath::Inverse(rootRotation),{to.x-from.x,to.y-from.y,to.z-from.z}),yaw,pitch))){
  Log("LookAt refused: invalid target distance/direction");Event("target_direction_invalid");return false;
 }
 Log("LookAt target="+ObjectName(candidate.targetModel)+"/Bip001_Head");return true;
}
bool TargetValid(){
 if(!Living(pose.targetModel)||!Living(pose.targetHead)||!Living(pose.parent))return false;
 Pins pins;int count=0;auto manager=Squad(pins,count);
 for(int i=0;manager&&i<count;++i){void* args[]{&i};auto entity=Call(Member,manager,args);if(entity!=pose.targetEntity)continue;
  auto model=Call(Model,entity);return model&&Call(ModelGo,model)==pose.targetModel;
 }return false;
}
void* BodyRoot(void* model,Pins& pins){
 auto modelRoot=pins.Keep(Call(GoTransform,model));bool inactive=true;void* args[]{animator.type_object,&inactive};auto array=pins.Keep(Call(Components,model,args));
 int dim=0,count=0;void* lengthArgs[]{&dim};if(!array||!Value(Length,array,count,lengthArgs)||count<1||count>128)return nullptr;
 int bodies=0;void* picked=nullptr;
 for(int i=0;i<count;++i){void* itemArgs[]{&i};auto a=pins.Keep(Call(Item,array,itemArgs));auto root=a?pins.Keep(Call(ComponentTransform,a)):nullptr;
  if(root&&root==modelRoot){picked=root;++bodies;}
 }return bodies==1?picked:nullptr;
}
#include "manager_motion.inc"
#include "arm_twists.inc"
void DetectAdministrator(){Pins pins;auto entity=pins.Keep(Call(Main));auto model=entity?pins.Keep(Call(Model,entity)):nullptr;auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;auto detected=Living(go)?Presets::Identity(ObjectName(go)):std::string{};if(detected==lastControlled)return;auto previous=lastControlled;lastControlled=detected;if(Presets::Administrator(detected)){if(workflow.draft.controlled!=detected){if(pose.pins)StopPose("administrator changed",true);boneTarget.Reset(0);boneCatalog.clear();++boneGeneration;Grip::Settings defaults;std::copy_n(defaults.values.begin(),21,gripTarget.values.begin());}workflow.draft.controlled=detected;workflow.draft.bones=boneTarget;workflow.draft.grip=gripTarget;}workflow.Invalidate();if(host&&host->emit){auto data=Presets::Json{{"type","controlled_changed"},{"controlled",Presets::Administrator(detected)?detected:std::string{}},{"workflow",Workflow::EntryJson(workflow.draft)}}.dump();host->emit(host->context,data.c_str());}}
bool BeginPose(Mode mode=Mode::HeadTest){
 if(!poseContracts||!leases){Log("Head test unavailable: pose contracts or pose leases missing");Event("unavailable");return false;}
 Pose candidate;candidate.pins=std::make_unique<Pins>();auto& pins=*candidate.pins;
 candidate.entity=pins.Keep(Call(Main));
 // Entity is a managed gameplay object, not a UnityEngine.Object.
 if(!candidate.entity){Log("Head test: no controlled Gameplay character");Event("no_character");return false;}
 auto model=pins.Keep(Call(Model,candidate.entity));candidate.model=model?pins.Keep(Call(ModelGo,model)):nullptr;
 if(!Living(candidate.model)||!Presets::Administrator(Presets::Identity(ObjectName(candidate.model)))){
  Log("Head test refused: control an Endministrator model first");Event("control_endminf_first");return false;
 }
 auto modelRoot=pins.Keep(Call(GoTransform,candidate.model));
 bool inactive=true;void* args[]{animator.type_object,&inactive};auto array=pins.Keep(Call(Components,candidate.model,args));
 int dim=0,count=0;void* lengthArgs[]{&dim};
 if(!array||!Value(Length,array,count,lengthArgs)||count<1||count>128)return false;
 int bodies=0;
 for(int i=0;i<count;++i){void* itemArgs[]{&i};auto a=pins.Keep(Call(Item,array,itemArgs));
  auto root=a?pins.Keep(Call(ComponentTransform,a)):nullptr;
  if(root&&root==modelRoot){candidate.root=root;++bodies;}
 }
 if(bodies!=1){Log("Head test refused: body Animator on model root is ambiguous/missing");Event("body_animator_ambiguous");return false;}
 std::vector<void*> found;int budget=1200;FindHead(candidate.root,0,budget,pins,found);
 if(found.size()!=1||budget<=0){Log("Head test refused: unique Head under Neck not verified");Event("head_ambiguous");return false;}
 candidate.head=found[0];if(!Rotation(candidate.head,candidate.base))return false;
 candidate.mode=mode;
 if(mode!=Mode::HeadTest&&!BindLookTarget(candidate))return false;
 if(IsDual(mode)){
  auto& r=candidate.reverse;r.head=candidate.targetHead;r.parent=pins.Keep(Call(Parent,r.head));r.root=BodyRoot(candidate.targetModel,pins);
  if(!Living(r.root)||!Living(r.parent)||r.root==candidate.root||!Rotation(r.head,r.base)){
   Log("Mutual LookAt refused: partner body Animator/Head is invalid or ambiguous");Event("partner_animator_unavailable");return false;
  }
 }
 if(IsPlaced(mode)){
  if(!standingContracts){Log("Standing preview unavailable: root transform contracts missing");Event("standing_unavailable");return false;}
  // Getter semantics are not confirmed as physical displacement. Keep the
  // raw state diagnostic-only; don't gate the verified placement path on it.
  Log("Standing raw movement flags (diagnostic only): controlled="+MovementJson(ReadMovement(candidate.entity))+" partner="+MovementJson(ReadMovement(candidate.targetEntity)));
  auto& p=candidate.placement;auto& r=candidate.partnerPlacement;p.root=candidate.root;r.root=candidate.reverse.root;
  if(!Value(Position,p.root,p.start)||!Value(Position,r.root,r.start)||!Value(WorldRotation,p.root,p.startRotation)||!Value(WorldRotation,r.root,r.startRotation)||
   !PoseMath::Normalize(p.startRotation)||!PoseMath::Normalize(r.startRotation)||
   !PoseMath::StandingAnchors(p.start,r.start,r.anchor,p.anchorRotation,r.anchorRotation,p.startRotation)){
   Log("Placement refused: invalid root position/rotation");Event("placement_transform_invalid");return false;
  }p.anchor=p.start;
 }
 if(mode==Mode::Handshake){
  if(handshakeA.frames.empty()||handshakeB.frames.size()!=handshakeA.frames.size()){Event("handshake_assets_unavailable");return false;}
  for(void* root:{candidate.root,candidate.reverse.root}){
   std::vector<void*> upper,fore,hand,finger;
   auto search=[&](auto&& self,void* t,int depth,int& remaining)->void{if(!t||depth>48||remaining--<=0)return;auto n=ObjectName(t);if(n=="Bip001_R_UpperArm")upper.push_back(t);if(n=="Bip001_R_Forearm")fore.push_back(t);if(n=="Bip001_R_Hand")hand.push_back(t);if(n=="Bip001_R_Finger2")finger.push_back(t);int count=0;if(!Value(ChildCount,t,count)||count<0||count>2048)return;for(int i=0;i<count&&remaining>0;++i){void* args[]{&i};self(self,pins.Keep(Call(Child,t,args)),depth+1,remaining);}};
   int remaining=1200;search(search,root,0,remaining);
   if(remaining<=0||upper.size()!=1||fore.size()!=1||hand.size()!=1||finger.size()!=1||Call(Parent,finger[0])!=hand[0]||Call(Parent,fore[0])!=upper[0]||Call(Parent,hand[0])!=fore[0]){Log("Handshake: right arm mapping missing/ambiguous");Event("right_arm_mapping_unavailable");return false;}
   candidate.arms.push_back({upper[0],fore[0],root});candidate.arms.push_back({fore[0],hand[0],root});candidate.arms.push_back({hand[0],finger[0],root});
  }
 }
 if(mode==Mode::Handshake){
  // Fixed reference twist and bone axes, calibrated before any pose write.
  for(auto& a:candidate.arms){PoseMath::Vector3 start{},end{};PoseMath::Quaternion world{},root{};
   if(!Value(Position,a.bone,start)||!Value(Position,a.child,end)||!Value(WorldRotation,a.bone,world)||!Value(WorldRotation,a.root,root)||!Rotation(a.bone,a.restLocal)||!PoseMath::Normalize(world)||!PoseMath::Normalize(root))return false;
   auto vector=Handshake::Sub(end,start);a.length=Handshake::Length(vector);
   if(a.length<0.01f||a.length>0.8f||!Handshake::Unit(vector)){Event("arm_calibration_invalid");return false;}
   a.axis=PoseMath::Rotate(PoseMath::Inverse(world),vector);a.reference=PoseMath::Multiply(PoseMath::Inverse(root),world);
  }
  for(int actor=0;actor<2;++actor){auto& hand=candidate.arms[actor*3+2];std::map<std::string,std::vector<void*>> nodes;
   auto gather=[&](auto&& self,void* t,int depth,int& budget)->void{if(!t||depth>8||budget--<=0)return;nodes[ObjectName(t)].push_back(t);int count=0;if(!Value(ChildCount,t,count)||count<0||count>256)return;for(int i=0;i<count&&budget>0;++i){void* args[]{&i};self(self,pins.Keep(Call(Child,t,args)),depth+1,budget);}};
   int budget=256;gather(gather,hand.bone,0,budget);
   auto unique=[&](const std::string& n)->void*{auto it=nodes.find(n);return it!=nodes.end()&&it->second.size()==1?it->second[0]:nullptr;};
   PoseMath::Vector3 wrist{},middle{},index{},little{};PoseMath::Quaternion handWorld{};
   auto indexBone=unique("Bip001_R_Finger1"),littleBone=unique("Bip001_R_Finger4");
   if(budget<=0||!indexBone||!littleBone||!Value(Position,hand.bone,wrist)||!Value(Position,hand.child,middle)||!Value(Position,indexBone,index)||!Value(Position,littleBone,little)||!Value(WorldRotation,hand.bone,handWorld)){
    Log("Grip geometry unavailable for actor="+std::to_string(actor)+"; retain original hand aiming");continue;}
   auto normal=Handshake::Cross(Handshake::Sub(middle,wrist),Handshake::Sub(index,little));
   if(!Handshake::Unit(normal)||!PoseMath::Normalize(handWorld))continue;
   candidate.palmNormal[actor]=PoseMath::Rotate(PoseMath::Inverse(handWorld),normal);
   std::vector<FingerControl> controls;bool complete=true;
   for(int digit=0;digit<5&&complete;++digit){std::string prefix="Bip001_R_Finger"+std::to_string(digit);
    void* chain[]{unique(prefix),unique(prefix+"1"),unique(prefix+"2"),unique(prefix+"Nub")};
    for(int joint=0;joint<3;++joint){auto bone=chain[joint],child=chain[joint+1];PoseMath::Vector3 start{},end{};PoseMath::Quaternion world{},rest{};
     if(!bone||!child||Call(Parent,child)!=bone||!Value(Position,bone,start)||!Value(Position,child,end)||!Value(WorldRotation,bone,world)||!Rotation(bone,rest)||!PoseMath::Normalize(world)){complete=false;break;}
     auto axis=Handshake::Cross(Handshake::Sub(end,start),normal);if(!Handshake::Unit(axis)){complete=false;break;}
     axis=PoseMath::Rotate(PoseMath::Inverse(world),axis);
     const float curls[5][3]{{12,20,12},{25,40,25},{35,50,30},{40,55,30},{45,60,35}};
     FingerControl control{bone,hand.root,0,rest,Handshake::AngleAxis(axis,curls[digit][joint])};control.axis=axis;control.actor=actor;control.digit=digit;control.joint=joint;controls.push_back(control);
    }
   }
   if(complete){candidate.fingers.insert(candidate.fingers.end(),controls.begin(),controls.end());Log("Grip calibrated 15 joints actor="+std::to_string(actor));}
   else Log("Incomplete finger chain actor="+std::to_string(actor)+"; skip curling, retain palm alignment");
  }
  auto line=Handshake::Sub(candidate.partnerPlacement.anchor,candidate.placement.anchor);if(!Handshake::Unit(line))return false;
  PoseMath::Vector3 shoulders[2]{};
  if(!Value(Position,candidate.arms[0].bone,shoulders[0])||!Value(Position,candidate.arms[3].bone,shoulders[1]))return false;
  auto& a=candidate.placement;auto& b=candidate.partnerPlacement;
  auto offsetA=PoseMath::Rotate(a.anchorRotation,PoseMath::Rotate(PoseMath::Inverse(a.startRotation),Handshake::Sub(shoulders[0],a.start)));
  auto offsetB=PoseMath::Rotate(b.anchorRotation,PoseMath::Rotate(PoseMath::Inverse(b.startRotation),Handshake::Sub(shoulders[1],b.start)));
  const float reachA=candidate.arms[0].length+candidate.arms[1].length,reachB=candidate.arms[3].length+candidate.arms[4].length;
  float distance=std::clamp((reachA+reachB)*0.70f,0.32f,0.80f);
  b.anchor=Handshake::Add(a.anchor,Handshake::Add(Handshake::Scale(line,distance),Handshake::Sub(offsetA,offsetB)));b.anchor.y=a.anchor.y;
  Log("Handshake measured arms A="+std::to_string(reachA)+" B="+std::to_string(reachB)+" shoulder gap target="+std::to_string(distance));
 }
 if(mode==Mode::Custom){auto it=animationLibrary.find(selectedClip);if(it==animationLibrary.end()){Event("custom_animation_missing");return false;}candidate.clip=it->second;
  if(!selectedControlledClip.empty()){auto main=animationLibrary.find(selectedControlledClip);if(main==animationLibrary.end()||!main->second->absolute||main->second->targetActor!=0){Event("controlled_animation_missing");return false;}candidate.controlledClip=main->second;}
  if(candidate.clip->absolute&&candidate.clip->targetActor==0)candidate.controlledClip=candidate.clip;
  if(candidate.controlledClip&&candidate.controlledClip->targetCharacter!=Presets::Identity(ObjectName(candidate.model))){CharacterMismatch("preset_controlled_character_mismatch",candidate.controlledClip->targetCharacter,workflow.draft.target,Presets::Identity(ObjectName(candidate.model)));return false;}
  candidate.mainEnd=candidate.controlledClip?candidate.controlledClip->duration:0;
  if(candidate.controlledClip||controlledPose!="inherit"){candidate.clip=std::make_shared<ImportedAnimation::Clip>(*candidate.clip);candidate.clip->managerAction=candidate.controlledClip||controlledPose=="imported"?"none":controlledPose;}
  if(candidate.clip->absolute&&candidate.clip->targetCharacter!=Presets::Identity(ObjectName(candidate.clip->targetActor==0?candidate.model:candidate.targetModel))){CharacterMismatch("custom_target_character_mismatch",workflow.draft.controlled,candidate.clip->targetCharacter,Presets::Identity(ObjectName(candidate.targetModel)));return false;}
  for(int actor=0;actor<2;++actor){auto actorClip=actor==0&&candidate.controlledClip?candidate.controlledClip:candidate.clip;void* root=actor==0?candidate.root:candidate.reverse.root;std::map<std::string,std::vector<void*>> nodes;int budget=1200;auto walk=[&](auto&& self,void* bone,int depth)->void{if(!bone||depth>48||budget--<=0)return;nodes[ObjectName(bone)].push_back(bone);int count=0;if(!Value(ChildCount,bone,count)||count<0||count>2048)return;for(int i=0;i<count&&budget>0;++i){void* args[]{&i};self(self,pins.Keep(Call(Child,bone,args)),depth+1);}};walk(walk,root,0);if(budget<=0){Event("custom_skeleton_too_large");return false;}
   for(size_t i=0;i<actorClip->tracks.size();++i){auto& track=actorClip->tracks[i];if(track.actor!=actor)continue;auto found=nodes.find(track.bone);if(found==nodes.end()||found->second.size()!=1){Event("custom_bone_missing_or_ambiguous");return false;}Pose::CustomBone bone;bone.bone=found->second[0];bone.root=root;bone.track=i;bone.actor=actor;if(!Rotation(bone.bone,bone.rest)){Event("custom_rotation_unavailable");return false;}bone.base=bone.rest;if(!track.keys.empty()&&track.keys.front().position){if(!Value(LocalPosition,bone.bone,bone.positionBase))return false;}candidate.customBones.push_back(bone);}
   auto uniqueNode=[&](const std::string& name)->void*{auto it=nodes.find(name);return it!=nodes.end()&&it->second.size()==1?it->second[0]:nullptr;};
   if(actorClip->absolute){
    // Rotation-only tracks must not inherit idle translations. Unanimated ancestors
    // must also use a consistent local reference or their motion shifts the whole chain.
    std::vector<void*> chain;auto addChain=[&](void* node){for(int depth=0;node&&node!=root&&depth<48;++depth){if(std::find(chain.begin(),chain.end(),node)==chain.end())chain.push_back(node);node=pins.Keep(Call(Parent,node));}};
    addChain(uniqueNode("Bip001"));for(auto& bone:candidate.customBones)if(bone.actor==actor)addChain(bone.bone);
    auto character=Presets::Identity(ObjectName(actor==0?candidate.model:candidate.targetModel));
    for(auto node:chain){auto name=ObjectName(node);if(uniqueNode(name)!=node){Event("custom_bone_missing_or_ambiguous");return false;}
     auto existing=std::find_if(candidate.customBones.begin(),candidate.customBones.end(),[&](auto& bone){return bone.bone==node;});
     if(existing==candidate.customBones.end()){Pose::CustomBone hold;hold.bone=node;hold.root=root;hold.actor=actor;hold.track=actorClip->tracks.size();hold.holdRotation=true;if(!Rotation(node,hold.rest))return false;hold.base=hold.rest;candidate.customBones.push_back(hold);existing=std::prev(candidate.customBones.end());}
     bool positionTrack=existing->track<actorClip->tracks.size()&&actorClip->tracks[existing->track].keys.front().position.has_value();
     if(!positionTrack){existing->holdPosition=true;if(!Value(LocalPosition,node,existing->heldPosition))return false;existing->positionBase=existing->heldPosition;}
     auto key=character+(name=="Bip001"?std::string{}:"|"+name);auto reference=skeletonReferences.find(key);
     if(reference!=skeletonReferences.end()){if(existing->holdPosition)existing->heldPosition=reference->second.position;if(existing->holdRotation)existing->rest=reference->second.rotation;}
    }
    Log("FBX stable local reference for "+character+": "+std::to_string(chain.size())+" animated-chain nodes");continue;
   }
   std::vector<std::pair<std::string,int>> calibratable{{"Bip001_R_Hand",3}};for(int digit=0;digit<5;++digit)for(int joint=0;joint<3;++joint)calibratable.push_back({"Bip001_R_Finger"+std::to_string(digit)+(joint?std::to_string(joint):""),6+digit*3+joint});
   for(auto& [name,field]:calibratable){auto found=nodes.find(name);if(found==nodes.end()||found->second.size()!=1)continue;auto existing=std::find_if(candidate.customBones.begin(),candidate.customBones.end(),[&](const auto& b){return b.bone==found->second[0];});if(existing!=candidate.customBones.end()){existing->field=field;if(field==3){existing->parent=pins.Keep(Call(Parent,existing->bone));Value(LocalPosition,existing->bone,existing->positionBase);}continue;}Pose::CustomBone bone;bone.bone=found->second[0];bone.root=root;bone.actor=actor;bone.field=field;bone.track=actorClip->tracks.size();if(!Rotation(bone.bone,bone.rest))continue;bone.base=bone.rest;if(field==3){bone.parent=pins.Keep(Call(Parent,bone.bone));if(!Value(LocalPosition,bone.bone,bone.positionBase))continue;}candidate.customBones.push_back(bone);}
   auto hand=uniqueNode("Bip001_R_Hand"),middle=uniqueNode("Bip001_R_Finger2"),index=uniqueNode("Bip001_R_Finger1"),little=uniqueNode("Bip001_R_Finger4");PoseMath::Vector3 handPosition{},middlePosition{},indexPosition{},littlePosition{},normal{};bool geometry=hand&&middle&&index&&little&&Value(Position,hand,handPosition)&&Value(Position,middle,middlePosition)&&Value(Position,index,indexPosition)&&Value(Position,little,littlePosition);if(geometry){normal=Handshake::Cross(Handshake::Sub(middlePosition,handPosition),Handshake::Sub(indexPosition,littlePosition));geometry=Handshake::Unit(normal);}
   for(auto& bone:candidate.customBones){if(bone.actor!=actor||bone.field<6)continue;int digit=(bone.field-6)/3,joint=(bone.field-6)%3;auto child=uniqueNode("Bip001_R_Finger"+std::to_string(digit)+(joint==2?"Nub":std::to_string(joint+1)));PoseMath::Vector3 from{},to{};PoseMath::Quaternion world{};if(!geometry||!child||Call(Parent,child)!=bone.bone||!Value(Position,bone.bone,from)||!Value(Position,child,to)||!Value(WorldRotation,bone.bone,world)){bone.field=-1;Log("Custom finger calibration unavailable; retain clip track");continue;}auto axis=Handshake::Cross(Handshake::Sub(to,from),normal);if(!Handshake::Unit(axis)){bone.field=-1;continue;}bone.axis=PoseMath::Rotate(PoseMath::Inverse(world),axis);}
  }
  auto& a=candidate.placement;auto& b=candidate.partnerPlacement;auto delta=Handshake::Sub(b.anchor,a.anchor);if(!Handshake::Unit(delta)){Event("custom_alignment_invalid");return false;}b.anchor=Handshake::Add(a.anchor,Handshake::Scale(delta,candidate.clip->distance));b.anchor=Handshake::Add(a.anchor,PoseMath::Rotate(a.startRotation,{0,0,candidate.clip->distance}));a.anchorRotation=a.startRotation;b.anchorRotation=PoseMath::Multiply(a.startRotation,PoseMath::YawPitch(180,0));if(!BindManagerMotion(candidate))return false;
 }
 if(!WalkBones(candidate.root,0,pins,candidate.fullBones)||(IsDual(mode)&&!WalkBones(candidate.reverse.root,1,pins,candidate.fullBones))){Event("bone_catalog_too_large");return false;}for(auto& [key,offset]:boneTarget.offsets){auto found=std::find_if(candidate.fullBones.begin(),candidate.fullBones.end(),[&](auto& b){return b.info.key==key;});if(found==candidate.fullBones.end()){Event("configured_bone_missing");return false;}if((offset[6]!=1||offset[7]!=1||offset[8]!=1)&&(!methods[LocalScale].resolved.method_info||!methods[SetLocalScale].resolved.method_info)){Event("bone_scale_unavailable");return false;}}
 if(mode==Mode::Custom&&(!BindArmTwists(candidate)||!BindSupport(candidate)||!BindSupport(candidate,0)))return false;
 candidate.lease=leases->acquire(candidate.root,id);
 if(!candidate.lease){Log("Head test refused: another module owns the character pose");Event("pose_busy");return false;}
 if(IsDual(mode)){
  candidate.reverse.lease=leases->acquire(candidate.reverse.root,id);
  if(!candidate.reverse.lease){leases->release(candidate.root,id,candidate.lease);Log("Mutual LookAt refused: partner pose is busy; first lease rolled back");Event("partner_pose_busy");return false;}
 }
 for(auto& b:candidate.fullBones)b.lease=b.root==candidate.root?candidate.lease:candidate.reverse.lease;
 for(auto& b:candidate.customBones)b.lease=b.root==candidate.root?candidate.lease:candidate.reverse.lease;
 for(auto& a:candidate.arms)a.lease=a.root==candidate.root?candidate.lease:candidate.reverse.lease;
 for(auto& f:candidate.fingers)f.lease=f.root==candidate.root?candidate.lease:candidate.reverse.lease;
 if(candidate.clip)candidate.rangeEnd=candidate.clip->duration;candidate.started=GetTickCount64();pendingSeek.reset();pendingMainSeek.reset();pose=std::move(candidate);
 if(mode==Mode::Custom){bool changed=false;for(auto& bone:pose.customBones)if(bone.holdPosition||bone.holdRotation){auto character=Presets::Identity(ObjectName(bone.actor==0?pose.model:pose.targetModel));auto name=ObjectName(bone.bone);auto key=character+(name=="Bip001"?std::string{}:"|"+name);if(!skeletonReferences.contains(key)){PoseMath::Vector3 position{};if(Value(LocalPosition,bone.bone,position)){skeletonReferences[key]={bone.holdPosition?bone.heldPosition:position,bone.rest};changed=true;}}}if(changed&&!SavePanelSettings())Log("Skeleton reference cache save failed; retained for this session");sceneAuditPending=true;++sceneRevision;Log("Playback reference "+EncodeSkeletonReferences().dump());Event("custom_animation_on");return true;}
 if(mode==Mode::Handshake){Log("Handshake contact IK ON; fixed calibration, measured shoulder alignment, 20s contact hold; palm proxy needs visual verification");Event("handshake_preview_on");return true;}
 Log(mode==Mode::HeadTest?"Head test ON: local yaw +20 degrees":mode==Mode::LookAt?"LookAt ON: Female Endministrator -> selected squad character":mode==Mode::Mutual?"Mutual LookAt ON: both leases acquired":"Standing PREVIEW ON: visual roots only, 1.2m spacing, 10 seconds; move to stop");Event(mode==Mode::HeadTest?"on":mode==Mode::LookAt?"lookat_on":mode==Mode::Mutual?"mutual_lookat_on":"standing_preview_on");return true;
}
bool GameFocused(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);return pid==GetCurrentProcessId();}
#include "free_camera_runtime.inc"
bool LookRotation(void* head,void* targetHead,void* root,void* parent,const PoseMath::Quaternion& base,float& currentYaw,float& currentPitch,float dt,PoseMath::Quaternion& target){
 PoseMath::Vector3 from{},to{};PoseMath::Quaternion rootRotation{},parentRotation{};float yaw=0,pitch=0;
 if(!Value(Position,head,from)||!Value(Position,targetHead,to)||!Value(WorldRotation,root,rootRotation)||!Value(WorldRotation,parent,parentRotation)||
  !PoseMath::Normalize(rootRotation)||!PoseMath::Normalize(parentRotation)||
  !PoseMath::LookAngles(PoseMath::Rotate(PoseMath::Inverse(rootRotation),{to.x-from.x,to.y-from.y,to.z-from.z}),yaw,pitch))return false;
 currentYaw=PoseMath::Smooth(currentYaw,yaw,dt);currentPitch=PoseMath::Smooth(currentPitch,pitch,dt);
 auto frame=PoseMath::Multiply(PoseMath::Inverse(parentRotation),rootRotation);
 auto offset=PoseMath::Multiply(PoseMath::Multiply(frame,PoseMath::YawPitch(currentYaw,currentPitch)),PoseMath::Inverse(frame));
 target=PoseMath::Multiply(offset,base);return PoseMath::Normalize(target);
}
bool PlaceRoot(RootPlacement& p,float t,PoseMath::Vector3 anchor,PoseMath::Quaternion facing){
 PoseMath::Vector3 local{};PoseMath::Quaternion rotation{};
 if(!Living(p.root)||!Value(LocalPosition,p.root,local)||!PoseMath::Finite(local)||!Rotation(p.root,rotation))return false;
 if(!p.positioned||!PoseMath::SamePosition(local,p.writtenLocal))p.baseLocal=local;
 if(!p.rotated||!PoseMath::SameRotation(rotation,p.writtenLocalRotation))p.baseLocalRotation=rotation;
 if(!WriteValue(SetPosition,p.root,t>=1.f?anchor:PoseMath::Lerp(p.start,anchor,t)))return false;
 p.positioned=true;
 if(!Value(LocalPosition,p.root,p.writtenLocal))return false;
 if(!WriteValue(SetWorldRotation,p.root,PoseMath::Nlerp(p.startRotation,facing,t)))return false;
 p.rotated=true;return Rotation(p.root,p.writtenLocalRotation);
}
Presets::Json SceneActors(){
 auto vec=[](PoseMath::Vector3 v){return Presets::Json::array({v.x,v.y,v.z});};auto actors=Presets::Json::array();
 for(int actor=0;actor<2;++actor){auto root=actor?pose.partnerPlacement.root:pose.placement.root;Presets::Json entry{{"actor",actor}};PoseMath::Vector3 value{};if(Living(root)&&Value(Position,root,value))entry["root_world"]=vec(value);if(Living(root)&&Value(LocalPosition,root,value))entry["root_local"]=vec(value);Pins pins;auto parent=root?pins.Keep(Call(Parent,root)):nullptr;if(parent&&Living(parent)){if(Value(Position,parent,value))entry["parent_world"]=vec(value);if(Value(LocalScale,parent,value))entry["parent_local_scale"]=vec(value);}
 for(auto& bone:pose.fullBones)if(bone.info.actor==actor&&(bone.info.name=="Bip001"||bone.info.name=="Bip001_Pelvis"||bone.info.name=="Bip001_L_Foot"||bone.info.name=="Bip001_R_Foot")){Presets::Json item;if(Living(bone.bone)&&Value(Position,bone.bone,value))item["world"]=vec(value);if(Living(bone.bone)&&Value(LocalPosition,bone.bone,value))item["local"]=vec(value);entry[bone.info.name]=item;}actors.push_back(entry);
 }return actors;
}
void AuditScene(){
 if(!pose.pins||pose.mode!=Mode::Custom)return;auto now=GetTickCount64();if(sceneAuditPending){sceneAuditUntil=now+10000;}if(!sceneAuditPending&&(now>sceneAuditUntil||now-sceneAuditLast<100))return;sceneAuditPending=false;sceneAuditLast=now;
 auto main=boneTarget.Get("0|@root"),passive=boneTarget.Get("1|@root");auto scene=SceneLayout::Build(pose.placement.anchor,pose.placement.anchorRotation,pose.partnerPlacement.anchor,pose.partnerPlacement.anchorRotation,main,passive);
 auto expected=Handshake::Add(scene.partnerPosition,pose.support.offset);auto expectedMain=Handshake::Add(scene.mainPosition,pose.mainSupport.offset);PoseMath::Vector3 actual{},actualMain{};if(!Value(Position,pose.reverse.root,actual)||!Value(Position,pose.root,actualMain))return;
 auto vec=[](PoseMath::Vector3 v){return Presets::Json::array({v.x,v.y,v.z});};auto data=Presets::Json{{"type","scene_audit"},{"revision",sceneRevision},{"root_y_cm",passive[1]},{"main_root_y_cm",main[1]},{"time",pose.clipTime},{"paused",pose.clipPaused},{"expected_world",vec(expected)},{"actual_world",vec(actual)},{"main_expected_world",vec(expectedMain)},{"main_actual_world",vec(actualMain)},{"before_world",vec(sceneBefore)},{"root_error_m",Handshake::Length(Handshake::Sub(actual,expected))},{"main_root_error_m",Handshake::Length(Handshake::Sub(actualMain,expectedMain))},{"before_actors",sceneBeforeActors},{"after_actors",SceneActors()}};
 auto text=data.dump();Log("Root applied "+text);if(host&&host->emit)host->emit(host->context,text.c_str());
}
void ApplyPose(float dt=1.0f/60){
 if(!pose.pins)return;
 if(IsPlaced(pose.mode)){
  if(pose.mode==Mode::Custom&&(sceneAuditPending||GetTickCount64()<=sceneAuditUntil)){Value(Position,pose.reverse.root,sceneBefore);sceneBeforeActors=SceneActors();}
  if(!TargetValid()||!leases->owns(pose.root,id,pose.lease)||!leases->owns(pose.reverse.root,id,pose.reverse.lease)){
   StopPose("standing target/ownership invalid",true);return;
  }
  if(std::isfinite(dt)&&dt>0)pose.placementTime+=std::min(dt,0.05f);
  const float smooth=1.f; // Place immediately, including distant party members.
  auto mainAnchor=pose.placement.anchor,partnerAnchor=pose.partnerPlacement.anchor;auto mainFacing=pose.placement.anchorRotation,partnerFacing=pose.partnerPlacement.anchorRotation;
  if(pose.mode==Mode::Custom){auto scene=SceneLayout::Build(mainAnchor,mainFacing,partnerAnchor,partnerFacing,boneTarget.Get("0|@root"),boneTarget.Get("1|@root"));mainAnchor=scene.mainPosition;partnerAnchor=scene.partnerPosition;mainFacing=scene.mainRotation;partnerFacing=scene.partnerRotation;}
  if(!PlaceRoot(pose.placement,smooth,mainAnchor,mainFacing)||!PlaceRoot(pose.partnerPlacement,smooth,partnerAnchor,partnerFacing)){
   StopPose("placement write failed; restore both roots",true);return;
  }
 }
 if(pose.mode==Mode::Custom){gripCurrent.SmoothTo(gripTarget,dt);if(!pose.clipPaused&&std::isfinite(dt)&&dt>0)pose.clipTime+=std::min(dt,.05f);float elapsed=pose.clipTime;if(!pose.clip){StopPose("custom animation missing",true);return;}bool loop=activePreset>=0||pose.rangeLoop||(calibrationMode&&calibrationLoop);float rangeEnd=pose.rangeLoop?pose.rangeEnd:pose.clip->duration;float rangeStart=pose.rangeLoop?pose.rangeStart:0.f;if(elapsed>=rangeEnd&&!pose.scrubbing&&!pose.partnerStopped){if(!loop){if(!pose.controlledClip){StopPose("custom animation complete",true);return;}pose.clipTime=elapsed=rangeEnd;pose.clipPaused=true;}else pose.clipTime=elapsed=rangeStart+std::fmod(elapsed-rangeStart,rangeEnd-rangeStart);++pose.loopCount;}if(pose.controlledClip&&!pose.mainStopped){if(!pose.mainPaused&&std::isfinite(dt)&&dt>0)pose.mainTime+=std::min(dt,.05f);float end=pose.mainRangeLoop?pose.mainEnd:pose.controlledClip->duration,start=pose.mainRangeLoop?pose.mainStart:0;bool mainLoop=activePreset>=0||pose.mainRangeLoop||(calibrationMode&&controlledLoop);if(pose.mainTime>=end&&!pose.mainScrubbing){if(mainLoop)pose.mainTime=start+std::fmod(pose.mainTime-start,end-start);else{pose.mainTime=end;pose.mainPaused=true;}}}
 float blend=pose.controlledClip?1.f:pose.scrubbing?1.f:(loop?1.f:std::clamp((pose.clip->duration-elapsed)/.2f,0.f,1.f));for(auto& bone:pose.customBones){if(bone.actor==0&&pose.mainStopped||bone.actor==1&&pose.partnerStopped)continue;auto clip=bone.actor==0&&pose.controlledClip?pose.controlledClip:pose.clip;float sampleTime=bone.actor==0&&pose.controlledClip?pose.mainTime:elapsed;if(!Living(bone.bone)||!leases->owns(bone.root,id,bone.lease)){StopPose("custom bone/lease invalid",true);return;}PoseMath::Quaternion current{};if(!Rotation(bone.bone,current)){StopPose("custom getter failed",true);return;}if(!bone.wrote||!PoseMath::SameRotation(current,bone.written))bone.base=current;auto delta=bone.track<clip->tracks.size()?clip->tracks[bone.track].Sample(sampleTime):PoseMath::Quaternion{};Grip::Settings defaults;
 if(bone.holdPosition){PoseMath::Vector3 currentPosition{};if(!Value(LocalPosition,bone.bone,currentPosition)){StopPose("skeleton root position unavailable",true);return;}if(!bone.positionWrote||!PoseMath::SamePosition(currentPosition,bone.positionWritten))bone.positionBase=currentPosition;if(!WriteValue(SetLocalPosition,bone.bone,bone.heldPosition)){StopPose("skeleton root position write failed",true);return;}bone.positionWritten=bone.heldPosition;bone.positionWrote=true;}
 if(bone.field==3){delta=PoseMath::Multiply(delta,Handshake::XYZ(gripCurrent.Get(bone.actor,3),gripCurrent.Get(bone.actor,4),gripCurrent.Get(bone.actor,5)));PoseMath::Vector3 currentPosition{};PoseMath::Quaternion rootRotation{},parentRotation{};if(!Value(LocalPosition,bone.bone,currentPosition)||!Value(WorldRotation,bone.root,rootRotation)||!Value(WorldRotation,bone.parent,parentRotation)){StopPose("custom palm calibration unavailable",true);return;}if(!bone.positionWrote||!PoseMath::SamePosition(currentPosition,bone.positionWritten))bone.positionBase=currentPosition;auto offset=PoseMath::Rotate(PoseMath::Inverse(parentRotation),PoseMath::Rotate(rootRotation,{gripCurrent.Get(bone.actor,0)*.01f,gripCurrent.Get(bone.actor,1)*.01f,gripCurrent.Get(bone.actor,2)*.01f}));auto position=Handshake::Add(bone.positionBase,Handshake::Scale(offset,blend));if(!WriteValue(SetLocalPosition,bone.bone,position)){StopPose("custom palm setter failed",true);return;}bone.positionWritten=position;bone.positionWrote=true;}
 else if(bone.field>=6&&!clip->absolute){float correction=gripCurrent.Get(bone.actor,bone.field)-defaults.Get(bone.actor,bone.field);delta=PoseMath::Multiply(delta,Handshake::AngleAxis(bone.axis,correction));}
 if(bone.track<clip->tracks.size()&&clip->tracks[bone.track].keys.front().position){PoseMath::Vector3 current{};if(!Value(LocalPosition,bone.bone,current)){StopPose("custom position getter failed",true);return;}if(!bone.positionWrote||!PoseMath::SamePosition(current,bone.positionWritten))bone.positionBase=current;auto desiredPosition=PoseMath::Lerp(bone.positionBase,clip->tracks[bone.track].SamplePosition(sampleTime),blend);if(!WriteValue(SetLocalPosition,bone.bone,desiredPosition)){StopPose("custom position setter failed",true);return;}bone.positionWritten=desiredPosition;bone.positionWrote=true;}
 auto targetRotation=clip->absolute&&bone.track<clip->tracks.size()?delta:PoseMath::Multiply(bone.rest,delta);auto desired=bone.holdRotation?bone.rest:PoseMath::Nlerp(bone.base,targetRotation,blend);if(!WriteRotation(bone.bone,desired)){StopPose("custom setter failed",true);return;}bone.written=desired;bone.wrote=true;}if(!ApplyManagerMotion(dt,blend)||!ApplyArmTwists()){StopPose("manager waist layer unavailable",true);return;}return;}
 if(pose.mode==Mode::Handshake){
  float elapsed=std::max(0.0f,pose.placementTime-0.6f);
  gripCurrent.SmoothTo(gripTarget,dt);
  const float duration=(calibrationMode||InteractionPanel::Editing())?120.0f:20.0f; // extended contact hold for visual diagnosis
  if(activePreset>=0)elapsed=std::min(elapsed,duration-0.6f);else if(elapsed>duration+0.5f){StopPose("handshake complete",true);return;}
  if(pose.placementTime<0.6f)return;
  PoseMath::Vector3 shoulder[2]{};PoseMath::Quaternion roots[2]{};
  for(int actor=0;actor<2;++actor){auto& upper=pose.arms[actor*3];
   if(!Living(upper.bone)||!Value(Position,upper.bone,shoulder[actor])||!Value(WorldRotation,upper.root,roots[actor])||!PoseMath::Normalize(roots[actor])){StopPose("shoulder invalid",true);return;}}
  PoseMath::Vector3 centers[2]{},forward[2]{},palmOffsets[2]{},adjustments[2]{};PoseMath::Quaternion handRotations[2]{};
  for(int actor=0;actor<2;++actor){auto& hand=pose.arms[actor*3+2];forward[actor]=PoseMath::Rotate(roots[actor],{0,0,1});
   auto reference=PoseMath::Multiply(roots[actor],hand.reference);auto normal=pose.palmNormal[actor];
   auto aligned=Handshake::Unit(normal)?Handshake::PalmRotation(reference,hand.axis,normal,forward[actor],PoseMath::Rotate(roots[actor],{-1,0,0})):PoseMath::Multiply(Handshake::Aim(PoseMath::Rotate(reference,hand.axis),forward[actor]),reference);
   handRotations[actor]=PoseMath::Multiply(aligned,Handshake::XYZ(gripCurrent.Get(actor,3),gripCurrent.Get(actor,4),gripCurrent.Get(actor,5)));
   palmOffsets[actor]=PoseMath::Rotate(handRotations[actor],Handshake::Scale(hand.axis,hand.length*0.5f));
   adjustments[actor]=PoseMath::Rotate(roots[actor],{gripCurrent.Get(actor,0)*0.01f,gripCurrent.Get(actor,1)*0.01f,gripCurrent.Get(actor,2)*0.01f});
   centers[actor]=Handshake::Sub(Handshake::Add(shoulder[actor],palmOffsets[actor]),adjustments[actor]);
  }
  float reachA=pose.arms[0].length+pose.arms[1].length,reachB=pose.arms[3].length+pose.arms[4].length;
  PoseMath::Vector3 fresh{};
  if(!Handshake::Contact(centers[0],centers[1],reachA,reachB,fresh)){
   Log("Handshake unreachable actual center gap="+std::to_string(Handshake::Length(Handshake::Sub(centers[0],centers[1])))+" reach A="+std::to_string(reachA)+" B="+std::to_string(reachB));StopPose("contact unreachable",true);Event("contact_unreachable");return;
  }
  auto sourceA=handshakeA.Sample(elapsed,60),sourceB=handshakeB.Sample(elapsed,60);
  fresh.y+=std::clamp((sourceA.fore.y-handshakeA.frames[0].fore.y+sourceB.fore.y-handshakeB.frames[0].fore.y)*0.008f,-0.008f,0.008f);
  if(!pose.contactReady){pose.contact=fresh;pose.contactReady=true;}
  else if(std::isfinite(dt)&&dt>0)pose.contact=PoseMath::Lerp(pose.contact,fresh,1-std::exp(-10*std::min(dt,0.05f)));
  auto target=pose.contact;
  float weight=std::min(std::clamp(elapsed/0.5f,0.0f,1.0f),std::clamp((duration+0.5f-elapsed)/0.5f,0.0f,1.0f));weight=weight*weight*(3-2*weight);
  PoseMath::Quaternion desired[6]{};
  for(int actor=0;actor<2;++actor){int index=actor*3;auto& upper=pose.arms[index];auto& fore=pose.arms[index+1];auto& hand=pose.arms[index+2];
   desired[index+2]=handRotations[actor];
   auto wrist=Handshake::Sub(Handshake::Add(target,adjustments[actor]),palmOffsets[actor]);
   PoseMath::Vector3 elbow{};auto pole=PoseMath::Rotate(roots[actor],{0,-1,-0.35f});
   if(!Handshake::Elbow(shoulder[actor],wrist,upper.length,fore.length,pole,elbow)){StopPose("IK target outside arm reach",true);Event("contact_unreachable");return;}
   PoseMath::Vector3 directions[]{Handshake::Sub(elbow,shoulder[actor]),Handshake::Sub(wrist,elbow)};
   for(int segment=0;segment<2;++segment){auto& arm=pose.arms[index+segment];auto reference=PoseMath::Multiply(roots[actor],arm.reference);
    desired[index+segment]=PoseMath::Multiply(Handshake::Aim(PoseMath::Rotate(reference,arm.axis),directions[segment]),reference);}
  }
  // All six targets are computed before mutations. Fixed calibration prevents
  // Animator's frame-to-frame twist changes from feeding back into the solver.
  for(size_t i=0;i<pose.arms.size();++i){auto& arm=pose.arms[i];PoseMath::Quaternion current{},parent{};
   auto parentObject=Call(Parent,arm.bone);
   if(!Living(arm.bone)||!Living(arm.child)||!leases->owns(arm.root,id,arm.lease)||!Rotation(arm.bone,current)||!parentObject||!Value(WorldRotation,parentObject,parent)||!PoseMath::Normalize(parent)){StopPose("arm invalid",true);return;}
   arm.base=PoseMath::AnimationBase(current,arm.written,arm.base,arm.wrote);
   auto local=PoseMath::Multiply(PoseMath::Inverse(parent),desired[i]);
   auto blended=PoseMath::Nlerp(arm.restLocal,local,weight);
   if(!WriteRotation(arm.bone,blended)){StopPose("arm write failed; rollback pair",true);return;}
   arm.written=blended;arm.wrote=true;
  }
  float grip=std::clamp((elapsed-0.5f)/0.6f,0.0f,1.0f)*std::clamp((duration-elapsed)/0.5f,0.0f,1.0f);grip=grip*grip*(3-2*grip);
  for(auto& f:pose.fingers){PoseMath::Quaternion current{};
   if(!Living(f.bone)||!leases->owns(f.root,id,f.lease)||!Rotation(f.bone,current)){StopPose("grip bone invalid",true);return;}
   f.base=PoseMath::AnimationBase(current,f.written,f.base,f.wrote);
   auto curl=Handshake::AngleAxis(f.axis,gripCurrent.Get(f.actor,6+f.digit*3+f.joint));
   auto target=PoseMath::Multiply(f.rest,PoseMath::Nlerp(PoseMath::Quaternion{},curl,grip));
   if(!WriteRotation(f.bone,target)){StopPose("grip setter failed",true);return;}f.written=target;f.wrote=true;
  }
  if(weight>0.99f&&GetTickCount64()-pose.contactLog>500){
   pose.contactLog=GetTickCount64();PoseMath::Vector3 palms[2]{};bool read=true;
   for(int actor=0;actor<2;++actor){auto& hand=pose.arms[actor*3+2];PoseMath::Vector3 wrist{},finger{};read=Value(Position,hand.bone,wrist)&&Value(Position,hand.child,finger)&&read;palms[actor]=PoseMath::Lerp(wrist,finger,0.5f);}
   if(read)Log("Handshake post-write palm-proxy gap m="+std::to_string(Handshake::Length(Handshake::Sub(palms[0],palms[1]))));
  }return;
 }
 PoseMath::Quaternion current{};
 if(!Rotation(pose.head,current)){StopPose("rotation read failed",true);return;}
 pose.base=PoseMath::AnimationBase(current,pose.written,pose.base,pose.wrote);
 auto target=PoseMath::Multiply(pose.base,PoseMath::Yaw20());
 PoseMath::Quaternion reverseTarget{};
 if(pose.mode!=Mode::HeadTest){
  if(!TargetValid()){StopPose("LookAt target removed or reloaded",true);return;}
  if(!LookRotation(pose.head,pose.targetHead,pose.root,pose.parent,pose.base,pose.yaw,pose.pitch,dt,target)){
   StopPose("LookAt direction invalid or target outside 0.2-20m",true);return;
  }
  if(IsDual(pose.mode)){
   auto& r=pose.reverse;PoseMath::Quaternion observed{};
   if(!Living(r.head)||!Living(r.parent)||!leases->owns(r.root,id,r.lease)||!Rotation(r.head,observed)){
    StopPose("partner invalid or pose ownership lost",true);return;
   }
   r.base=PoseMath::AnimationBase(observed,r.written,r.base,r.wrote);
   // Compute both outputs before either write; the second must not chase a
   // head position that was changed earlier in this same frame.
   if(!LookRotation(r.head,pose.head,r.root,r.parent,r.base,r.yaw,r.pitch,dt,reverseTarget)){
    StopPose("partner direction invalid",true);return;
   }
  }
 }
 if(!WriteRotation(pose.head,target)){StopPose("rotation write failed",true);return;}
 PoseMath::Normalize(target);pose.written=target;pose.wrote=true;
 if(IsDual(pose.mode)){
  auto& r=pose.reverse;
  if(!WriteRotation(r.head,reverseTarget)){StopPose("partner setter failed; rollback both",true);return;}
  r.written=reverseTarget;r.wrote=true;
 }
}
Mode PresetMode(const std::string& value){return value.starts_with("clip:")?Mode::Custom:value=="handshake"?Mode::Handshake:value=="standing"?Mode::Standing:value=="mutual"?Mode::Mutual:value=="lookat"?Mode::LookAt:Mode::HeadTest;}
bool PrepareEntry(const Presets::Entry& entry){
 if(!entry.enabled){Event("preset_slot_disabled");return false;}
 Pins pins;auto entity=pins.Keep(Call(Main));auto model=entity?pins.Keep(Call(Model,entity)):nullptr;auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;
 if(!Living(go)||Presets::Identity(ObjectName(go))!=entry.controlled){CharacterMismatch("preset_controlled_character_mismatch",entry.controlled,entry.target,Living(go)?Presets::Identity(ObjectName(go)):std::string{});return false;}
 // Validate a unique live partner before changing settings or selecting it.
 if(entry.animation!="head"){int count=0,matches=0;auto manager=Squad(pins,count);for(int i=0;manager&&i<count;++i){void* args[]{&i};auto e=pins.Keep(Call(Member,manager,args));if(!e||e==entity)continue;auto m=pins.Keep(Call(Model,e));auto g=m?pins.Keep(Call(ModelGo,m)):nullptr;if(Living(g)&&Presets::Identity(ObjectName(g))==entry.target)++matches;}if(matches!=1){CharacterMismatch("preset_target_missing_or_ambiguous",entry.controlled,entry.target,{},true);return false;}}
 if(entry.animation.starts_with("clip:")){auto partner=animationLibrary.find(entry.animation.substr(5));if(partner==animationLibrary.end()||partner->second->targetActor!=1){Event("custom_animation_missing");return false;}}auto requestedControlledClip=entry.controlledPose=="imported"&&entry.controlledAnimation.starts_with("clip:")?entry.controlledAnimation.substr(5):std::string{};if(entry.controlledPose=="imported"&&requestedControlledClip.empty()){Event("controlled_animation_missing");return false;}if(!requestedControlledClip.empty()){auto main=animationLibrary.find(requestedControlledClip);if(main==animationLibrary.end()||main->second->targetActor!=0){Event("controlled_animation_missing");return false;}if(main->second->targetCharacter!=entry.controlled){CharacterMismatch("preset_controlled_character_mismatch",main->second->targetCharacter,entry.target,entry.controlled);return false;}}controlledPose=entry.controlledPose;controlledSupport=entry.controlledSupport;selectedControlledClip=requestedControlledClip;supportTarget=entry.support;boneTarget=entry.bones;selectedSlot=-1;selectedModel.clear();selectedIdentity=entry.target;selectedClip=entry.animation.starts_with("clip:")?entry.animation.substr(5):std::string{};if(gripTarget.Encode()!=entry.grip.Encode())workflow.Invalidate();gripTarget=entry.grip;gripCurrent=entry.grip;if(selectedClip.empty()&&!selectedControlledClip.empty())selectedClip=selectedControlledClip;armedMode=selectedControlledClip.empty()?PresetMode(entry.animation):Mode::Custom;armed=true;keyDown=true;return true;
}
bool PrepareCalibration(Presets::Entry entry){
 if(entry.animation.starts_with("clip:")){auto found=animationLibrary.find(entry.animation.substr(5));if(found==animationLibrary.end()){Event("custom_animation_missing");return false;}if(found->second->absolute&&found->second->targetActor==1&&!found->second->targetCharacter.empty())entry.target=found->second->targetCharacter;}
 // PrepareEntry verifies a unique live squad member before selecting it.
 if(!PrepareEntry(entry))return false;
 if(workflow.draft.target!=entry.target){workflow.draft.target=entry.target;workflow.Invalidate();targetsPending=true;boneCatalog.clear();++boneGeneration;
  if(host&&host->emit){auto json=Presets::Json{{"type","calibration_target_selected"},{"target",entry.target},{"target_name",CharacterNames::Name(entry.target)}}.dump();host->emit(host->context,json.c_str());}}
 return true;
}
bool PreparePreset(int index){return index>=0&&index<9&&PrepareEntry(presetBank.slots[index]);}
#include "timeline_edit.inc"
void PresetEvent(int index,bool running){if(!host||!host->emit)return;auto& entry=presetBank.slots[index];auto data=Presets::Json{{"type",running?"preset_started":"preset_applied"},{"preset_index",index},{"controlled",entry.controlled},{"target",entry.target},{"animation",entry.animation},{"grip_values",entry.grip.Encode()}}.dump();host->emit(host->context,data.c_str());}
void TriggerPreset(int index){
 if(index<0||index>=int(presetBank.slots.size()))return;
 pendingPreview=false;pendingTimeline=false;pendingSelectionStop=false;pendingSeek.reset();calibrationMode=false;
 if(pose.pins&&activePreset==index){StopPose("preset toggle",true);activePreset=-1;return;}
 if(!presetBank.slots[index].enabled){Event("preset_slot_disabled");return;}
 if(pose.pins)StopPose("switch preset",true);activePreset=-1;
 if(PreparePreset(index)&&BeginPose(armedMode)){activePreset=index;PresetEvent(index,true);}
}
void PumpPresetKeys(const std::array<bool,9>& down,bool focused){
 bool triggered=false;for(int i=0;i<9;++i){bool edge=Presets::Rising(down[i],presetKeys[i]);if(edge&&focused&&presetBank.enabled&&presetBank.slots[i].enabled&&!triggered){TriggerPreset(i);triggered=true;}}
}
float RotationError(PoseMath::Quaternion a,PoseMath::Quaternion b){return 2*std::acos(std::clamp(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w),0.f,1.f))*57.29577951f;}
void PlaybackAudit(float dt,bool before){
 if(!pose.pins||pose.mode!=Mode::Custom||!pose.clip)return;
 if(before){if(GetTickCount64()<pose.playbackAuditAt)return;pose.playbackAuditAt=GetTickCount64()+1000;}
 else if(pose.playbackAudit.empty())return;
 Presets::Json row{{"phase",before?"before_write":"after_write"},{"clip",pose.clip->id},{"target",pose.clip->targetCharacter},{"time",pose.clipTime},{"duration",pose.clip->duration},{"dt",std::isfinite(dt)?dt:0.f},{"dt_finite",std::isfinite(dt)},{"placement_time",pose.placementTime},{"paused",pose.clipPaused},{"loop",calibrationMode&&calibrationLoop}};
 auto joints=Presets::Json::array();
 for(auto& b:pose.customBones){if(b.actor!=1||b.track>=pose.clip->tracks.size())continue;auto& track=pose.clip->tracks[b.track];if(!track.bone.ends_with("_UpperArm")&&!track.bone.ends_with("_Forearm")&&!track.bone.ends_with("_Hand"))continue;
  PoseMath::Quaternion actual{};if(!Rotation(b.bone,actual))continue;auto sample=track.Sample(pose.clipTime);auto q=[](auto v){return Presets::Json::array({v.x,v.y,v.z,v.w});};
  joints.push_back({{"bone",track.bone},{"sample",q(sample)},{"actual",q(actual)},{"sample_change_deg",RotationError(sample,track.keys.front().rotation)},{"sample_error_deg",RotationError(actual,sample)},{"previous_write_error_deg",b.wrote?RotationError(actual,b.written):0.f},{"keys",track.keys.size()}});
 }
 row["joints"]=joints;Log("Playback audit "+row.dump());
 if(before)pose.playbackAudit="診斷：動畫時間 "+std::to_string(pose.clipTime)+" / "+std::to_string(pose.clip->duration)+(pose.clipPaused?" 秒（暫停）":" 秒");
 else {pose.playbackAudit.clear();}
}
void PumpPose(float dt=1.0f/60){
 DetectAdministrator();
 if(restoreRequested.exchange(false)){const bool hadPose=bool(pose.pins);StopPose("restore requested",true);if(!hadPose)Event("off");armed=false;restored.notify_all();}
 if(pose.pins){
  auto entity=Call(Main);auto model=entity?Call(Model,entity):nullptr;auto go=model?Call(ModelGo,model):nullptr;
  const uint64_t limit=(calibrationMode||InteractionPanel::Editing())?130000:pose.mode==Mode::Custom?uint64_t((pose.clip?pose.clip->duration:0)*1000+5000):pose.mode==Mode::LookAt||pose.mode==Mode::Mutual||pose.mode==Mode::Handshake?30000:10000;
  bool movement=false;if(activePreset<0&&IsPlaced(pose.mode)&&GameFocused()&&!InteractionPanel::Editing()&&!FreeCameraInput::blocked){constexpr int keys[]{'W','A','S','D',VK_SPACE};for(int key:keys)movement=movement||(GetAsyncKeyState(key)&0x8000)!=0;bool mouse=((GetAsyncKeyState(VK_LBUTTON)|GetAsyncKeyState(VK_RBUTTON))&0x8000)!=0;movement=movement||InteractionPanel::MouseCancelsPose(mouse,InteractionPanel::MouseOver());}
  if((activePreset<0&&!GameFocused()&&!calibrationMode)||!Living(pose.head)||entity!=pose.entity||go!=pose.model||(activePreset<0&&pose.mode!=Mode::Custom&&GetTickCount64()-pose.started>=limit)||!leases->owns(pose.root,id,pose.lease)||
   ((IsDual(pose.mode))&&!leases->owns(pose.reverse.root,id,pose.reverse.lease))||movement)
   StopPose("focus/character/model/lease changed or timeout",true);
 }
 std::array<bool,9> pressed{};for(int i=0;i<9;++i)pressed[i]=(GetAsyncKeyState(VK_NUMPAD1+i)&0x8000)!=0;PumpPresetKeys(pressed,GameFocused()&&!InteractionPanel::Editing()&&!FreeCameraInput::blocked);
 if(pendingMainStop||pendingPartnerStop){if(pose.mode==Mode::Custom){if(pendingMainStop)StopAnimationActor(0);if(pendingPartnerStop)StopAnimationActor(1);}else StopPose("actor stop",true);pendingMainStop=pendingPartnerStop=false;}PlaybackAudit(dt,true);if(!RemoveBoneLayer(true)){StopPose("bone layer restoration failed",true);return;}if(pendingMainSeek&&pose.pins&&pose.controlledClip){pose.mainTime=std::clamp(*pendingMainSeek,0.f,pose.controlledClip->duration);pose.mainScrubbing=pose.mainPaused;pendingMainSeek.reset();}if(pendingSeek&&pose.pins&&pose.clip){pose.clipTime=std::clamp(*pendingSeek,0.f,pose.clip->duration);pose.scrubbing=pose.clipPaused;pendingSeek.reset();}if(pose.pins&&pose.mode==Mode::Custom&&(pose.support.mode!=supportTarget&&!BindSupport(pose)||pose.mainSupport.mode!=controlledSupport&&!BindSupport(pose,0))){StopPose("support binding failed",true);return;}ApplyPose(dt);ApplyBoneLayer();if(!ApplySupport()||!ApplySupport(0)){auto status=panelStatus;StopPose("support solve failed",true);Event(status.starts_with("support_")?status.c_str():"support_update_failed");return;}if(pose.pins&&pose.support.mode!="none"&&!ApplyManagerMotion(0,1)){StopPose("supported contact update failed",true);return;}AuditScene();PlaybackAudit(dt,false);
}
void Tree(void* transform,int depth,int& budget,Pins& pins){
 if(!transform||depth>48||budget<=0)return;--budget;
 Log(std::string(size_t(depth)*2,' ')+ObjectName(transform));
 int count=0;if(!Value(ChildCount,transform,count)||count<0||count>2048)return;
 for(int i=0;i<count&&budget>0;++i){void* args[]{&i};Tree(pins.Keep(Call(Child,transform,args)),depth+1,budget,pins);}
}
void Actor(void* entity,const std::string& label){
 Pins pins;entity=pins.Keep(entity);if(!entity)return;
 auto model=pins.Keep(Call(Model,entity));auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;
 if(!go){Log(label+": no instantiated model");return;}
 Log("Character candidate "+label+" model="+ObjectName(go)+" (identity unverified)");
 auto scene=pins.Keep(Call(Scene,go));auto sceneData=scene?runtime->object_unbox(runtime->context,scene):nullptr;
 auto sceneName=sceneData?pins.Keep(Call(SceneName,sceneData)):nullptr;
 char sceneText[1024]{};if(sceneName)runtime->copy_managed_string(runtime->context,sceneName,sceneText,sizeof(sceneText));
 Log(std::string("Scene=")+(sceneName?sceneText:"unavailable"));
 bool inactive=true;void* args[]{animator.type_object,&inactive};auto array=pins.Keep(Call(Components,go,args));
 int dim=0,count=0;void* lengthArgs[]{&dim};
 if(!array||!Value(Length,array,count,lengthArgs)||count<0||count>128){Log("Animator array unavailable");return;}
 Log("Animator count="+std::to_string(count));
 for(int i=0;i<count;++i){void* itemArgs[]{&i};auto a=pins.Keep(Call(Item,array,itemArgs));if(!a)continue;Log("Animator="+ObjectName(a));}
 int budget=1200;Tree(pins.Keep(Call(GoTransform,go)),0,budget,pins);
 if(budget<=0)Log("Hierarchy truncated at 1200 nodes");
}
std::string JsonText(const std::string& value){
 std::string out="\"";for(unsigned char c:value){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+='?';}else out+=char(c);}return out+"\"";
}
void Targets(){
 Pins pins;auto leader=pins.Keep(Call(Main));int count=0;auto manager=Squad(pins,count);targetChoices.clear();
 auto model=leader?pins.Keep(Call(Model,leader)):nullptr;auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;
 DetectAdministrator();
 std::string json="{\"type\":\"targets\",\"controlled\":"+JsonText(go?ObjectName(go):"")+",\"selected_slot\":"+std::to_string(selectedSlot)+",\"targets\":[";
 bool first=true;
 for(int i=0;manager&&i<count;++i){void* args[]{&i};auto entity=pins.Keep(Call(Member,manager,args));if(!entity||entity==leader)continue;
  auto m=pins.Keep(Call(Model,entity));auto g=m?pins.Keep(Call(ModelGo,m)):nullptr;if(!Living(g))continue;
  auto name=ObjectName(g);if(name.size()>512)continue;targetChoices[i]=name;
  if(!first)json+=",";first=false;json+="{\"slot\":"+std::to_string(i)+",\"name\":"+JsonText(name)+"}";
 }
 json+="]}";panelStatus="隊伍角色清單已更新";if(host&&host->emit)host->emit(host->context,json.c_str());
}
Presets::Json CaptureSkeletonReference(const std::vector<Pose::FullBone>& bones){
 if(pose.pins)throw std::runtime_error("Stop playback before capturing reference");
 auto data=Presets::Json{{"format","endfield-skeleton-pose-reference"},{"version",1},{"pose_kind","current_gameplay_pose_not_bind_pose"},{"controlled",workflow.draft.controlled},{"partner",workflow.draft.target},{"bones",Presets::Json::array()}};
 std::map<void*,std::string> keys;for(auto& b:bones)keys[b.bone]=b.info.key;
 for(auto& b:bones){PoseMath::Vector3 local{},world{};PoseMath::Quaternion localQ{},worldQ{};
  if(!Living(b.bone)||!Value(LocalPosition,b.bone,local)||!Value(Position,b.bone,world)||!PoseMath::Finite(local)||!PoseMath::Finite(world)||!Rotation(b.bone,localQ)||!Value(WorldRotation,b.bone,worldQ)||!PoseMath::Normalize(worldQ))throw std::runtime_error("Incomplete skeleton reference");
  auto parent=Call(Parent,b.bone);auto parentKey=keys.find(parent);
  data["bones"].push_back({{"key",b.info.key},{"name",b.info.name},{"actor",b.info.actor},{"parent_key",parentKey==keys.end()?std::string{}:parentKey->second},{"local_position",{local.x,local.y,local.z}},{"world_position",{world.x,world.y,world.z}},{"local_rotation_xyzw",{localQ.x,localQ.y,localQ.z,localQ.w}},{"world_rotation_xyzw",{worldQ.x,worldQ.y,worldQ.z,worldQ.w}}});
 }
 return data;
}
#include "bind_reference.inc"
std::string SaveSkeletonReference(const std::vector<Pose::FullBone>& bones){
#ifndef INTERACTION_NO_OVERLAY
 try{auto data=CaptureSkeletonReference(bones);try{CaptureMeshBindposes(data,bones);}catch(const std::exception& e){data["bind_pose_error"]=e.what();Log(std::string("Bind reference unavailable: ")+e.what());}auto base=PanelSettingsPath();if(base.empty())throw std::runtime_error("Missing application data directory");auto directory=base.parent_path()/"SkeletonReferences";std::filesystem::create_directories(directory);auto path=directory/("skeleton-reference-"+std::to_string(GetTickCount64())+".json");auto temp=path;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<data.dump(2);out.flush();if(!out.good())throw std::runtime_error("Reference write failed");}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Reference commit failed");auto u=path.u8string();std::string result(u.begin(),u.end());lastSkeletonReferencePath=result;Log("Skeleton reference saved: "+result);return result;}catch(const std::exception& e){Log(std::string("Skeleton reference unavailable: ")+e.what());}
#endif
 return {};
}
void ScanBoneCatalog(){std::vector<std::string> errors;externalParts=ExternalModels::Load({moduleFolder/"../../external-models",PanelSettingsPath().parent_path()/"ExternalModels"},errors);for(auto& error:errors)Log("External model profile: "+error);Pins pins;std::vector<Pose::FullBone> scanned;auto leader=pins.Keep(Call(Main));auto model=leader?pins.Keep(Call(Model,leader)):nullptr;auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;if(!Living(go)||!Presets::Administrator(Presets::Identity(ObjectName(go)))){Event("control_endminf_first");return;}auto root=BodyRoot(go,pins);if(!root||!WalkBones(root,0,pins,scanned)){Event("bone_catalog_unavailable");return;}int count=0,matches=0;auto squadManager=Squad(pins,count);for(int i=0;squadManager&&i<count;++i){void* args[]{&i};auto entity=pins.Keep(Call(Member,squadManager,args));if(!entity||entity==leader)continue;auto m=pins.Keep(Call(Model,entity));auto g=m?pins.Keep(Call(ModelGo,m)):nullptr;if(!Living(g)||Presets::Identity(ObjectName(g))!=workflow.draft.target)continue;auto r=BodyRoot(g,pins);if(!r||!WalkBones(r,1,pins,scanned)){Event("bone_catalog_unavailable");return;}++matches;}if(matches!=1){CharacterMismatch("preset_target_missing_or_ambiguous",workflow.draft.controlled,workflow.draft.target,{},true);return;}boneCatalog.clear();for(auto& b:scanned)boneCatalog.push_back(b.info);
 externalCatalog=Presets::Json::array();for(auto& p:externalParts){int actor=p.character==Presets::Identity(ObjectName(go))?0:p.character==workflow.draft.target?1:-1;if(actor<0)continue;auto item=Presets::Json{{"id",p.id},{"name",p.name},{"actor",actor},{"backend",p.backend},{"parameters",Presets::Json::array()}};
 if(p.backend=="unity_transform"){std::vector<BoneInfo*> matches;for(auto& b:boneCatalog)if(b.actor==actor&&b.name==p.bone)matches.push_back(&b);bool bound=matches.size()==1;if(bound)matches[0]->group="模型骨架";item["status"]=bound?"已連接 Unity 骨架；在模型骨架調整 XYZ":"找不到唯一骨架；請確認模組已載入";item["available"]=bound;}
 else{std::error_code ec;bool source=std::filesystem::is_regular_file(std::filesystem::u8path(p.source),ec);item["status"]=source?"已找到 EFMI 設定檔；控制橋接尚未接通":"找不到 EFMI 設定檔；請修正描述檔路徑";item["available"]=false;for(auto& q:p.parameters)item["parameters"].push_back({{"name",q.name},{"min",q.low},{"max",q.high}});}
 externalCatalog.push_back(item);
 }++boneGeneration;auto referencePath=SaveSkeletonReference(scanned);auto data=Presets::Json{{"type","bones_ready"},{"count",boneCatalog.size()},{"generation",boneGeneration},{"reference_path",referencePath}}.dump();panelStatus="骨架已更新，共 "+std::to_string(boneCatalog.size())+" 個節點"+(referencePath.empty()?"；姿勢參考未匯出（先停止播放）":"；姿勢參考已匯出");if(host&&host->emit)host->emit(host->context,data.c_str());}
void Scan(){
 Log("=== Gameplay scan; thread="+std::to_string(GetCurrentThreadId())+" ===");
 Pins pins;auto leader=pins.Keep(Call(Main));
 if(!leader){Log("No controlled Gameplay character; enter Gameplay and retry");return;}
 Actor(leader,"controlled");
 if(!squad.field_info){Log("Squad field unavailable; controlled character only");return;}
 auto player=pins.Keep(Call(Player));auto manager=player?pins.Keep(runtime->field_get_value_object(runtime->context,squad.field_info,player)):nullptr;
 bool loading=true;int count=0;
 if(!manager||!Value(Loading,manager,loading)||loading||!Value(SlotCount,manager,count)||count<0||count>16){Log("Squad not ready");return;}
 for(int i=0;i<count;++i){void* args[]{&i};auto entity=pins.Keep(Call(Member,manager,args));if(entity&&entity!=leader)Actor(entity,"slot="+std::to_string(i));}
 Log("=== Scan complete; no pose changes ===");
}
void MovementReport(){
 Pins pins;auto entity=pins.Keep(Call(Main));auto controlled=ReadMovement(entity);
 std::string json="{\"type\":\"movement_diagnostics\",\"controlled\":"+MovementJson(controlled)+",\"partner\":null}";
 int count=0;auto manager=Squad(pins,count);
 for(int i=0;manager&&i<count;++i){void* args[]{&i};auto member=pins.Keep(Call(Member,manager,args));if(!member||member==entity)continue;
  auto model=pins.Keep(Call(Model,member));auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;
  if(go&&ObjectName(go).starts_with("chr_0004_pelica_postmodel(")){
   json="{\"type\":\"movement_diagnostics\",\"controlled\":"+MovementJson(controlled)+",\"partner\":"+MovementJson(ReadMovement(member))+"}";break;
  }
 }
 Log(json);if(host&&host->emit)host->emit(host->context,json.c_str());
}
void FollowReport(){
 // Runs only on the existing verified Gameplay thread. Stop the preview
 // first, so this research pass never races model-placement writes.
 StopPose("read-only follow research",true);
 FollowMetadata::Api metadata;
 if(!metadata.Load()){Log("[follow-research] required metadata exports missing; nothing inspected");Event("follow_metadata_unavailable");return;}
 Pins pins;int count=0;auto manager=Squad(pins,count);void* partner=nullptr;
 for(int i=0;manager&&i<count;++i){void* args[]{&i};auto entity=pins.Keep(Call(Member,manager,args));
  auto model=entity?pins.Keep(Call(Model,entity)):nullptr;auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;
  if(go&&ObjectName(go).starts_with("chr_0004_pelica_postmodel(")){partner=entity;break;}
 }
 if(!partner){Log("[follow-research] Perlica squad instance unavailable");Event("follow_partner_missing");return;}
 Log("=== FOLLOW RESEARCH BEGIN; metadata only, no discovered method invocation ===");
 int budget=1500;
 metadata.Dump(partner,"partner Entity",Log,budget);
 metadata.Dump(pins.Keep(Call(Movement,partner)),"partner MovementComponent",Log,budget);
 metadata.Dump(pins.Keep(Call(CharacterController,partner)),"partner CharacterController",Log,budget);
 metadata.Dump(manager,"SquadManager",Log,budget);
 Log("=== FOLLOW RESEARCH COMPLETE; no AI/movement changes ===");Event("follow_research_complete");
}
#include "fbx_import.inc"
void __fastcall Detour(void* instance,float dt,void* method){
 if(next)next(instance,dt,method);
 if(!active.load())return;
 std::lock_guard lock(gate);
 if(!active.load())return;
 if(!unityThread)unityThread=GetCurrentThreadId();
 if(unityThread!=GetCurrentThreadId()){Log("Unexpected callback thread; Unity work skipped");return;}
 DetectAdministrator();
 PumpCalibrationPreview();
 if(pendingStart>=0&&GameFocused()){int index=pendingStart;pendingStart=-1;TriggerPreset(index);}
 if(pendingPreset>=0){int index=pendingPreset;pendingPreset=-1;if(pose.pins)StopPose("apply preset",true);activePreset=-1;if(PreparePreset(index))PresetEvent(index,false);}
 PumpFreeCamera(dt);
 PumpPose(dt);
 static ULONGLONG lastTimeline=0;if(GetTickCount64()-lastTimeline>=150){lastTimeline=GetTickCount64();auto payload=Presets::Json{{"type","timeline"},{"free_camera",FreeCameraJson()},{"timeline",TimelineJson()},{"controlled_timeline",TimelineJson(true)}}.dump();if(host&&host->emit)host->emit(host->context,payload.c_str());}
 if(targetsPending.exchange(false))Targets();if(bonesPending){bonesPending=false;ScanBoneCatalog();}PumpFbxImport();
 if(movementPending.exchange(false))MovementReport();
 if(followPending.exchange(false))FollowReport();
 if(pending.exchange(false)){StopPose("diagnostic scan",true);Scan();Event("scan_complete");}
}
std::string ConnectionFailure(BE_Result result,const std::string& stage){
 if(result==BE_Result_Conflict&&stage=="CameraManager.TailLateTick")return "連接入口被獨占（可能是 BEM 相機模組）；停用相機模組並重啟遊戲後再連接";
 if(result==BE_Result_NotReady)return "遊戲入口尚未就緒（"+stage+"），進入遊戲後重試";
 if(result==BE_Result_NotFound||result==BE_Result_ContractMismatch)return "遊戲入口不相容或缺少方法（"+stage+"），請核對 BEM 與遊戲版本";
 return "連接失敗（"+stage+"，錯誤 "+std::to_string(int(result))+"），請查看 BEM 模組日誌";
}
BE_Result Connect(){
 if(hook)return BE_Result_Ok;
 auto failure=[](BE_Result result,const std::string& stage){panelStatus=ConnectionFailure(result,stage);Log("Connect failed: "+stage+" result="+std::to_string(int(result)));return result;};
 runtime=host->get_runtime?host->get_runtime(host->context):host->runtime;
 if(!runtime)return failure(BE_Result_NotReady,"runtime");
 if(runtime->abi_version!=1||!runtime->resolve_method||!runtime->resolve_class||!runtime->runtime_invoke||!runtime->object_unbox||!runtime->copy_managed_string||!runtime->gchandle_new||!runtime->gchandle_free||!runtime->field_get_value_object)return failure(BE_Result_ContractMismatch,"runtime ABI");
 for(int i=0;i<sizeof(methods)/sizeof(methods[0]);++i){
  auto result=runtime->resolve_method(runtime->context,&methods[i].spec,&methods[i].resolved);
  if(result!=BE_Result_Ok){Log(std::string("Missing contract: ")+methods[i].spec.class_name+"."+methods[i].spec.method_name);if(i<Player)return failure(result,std::string(methods[i].spec.class_name)+"."+methods[i].spec.method_name);}
 }
 poseContracts=true;for(int i=Alive;i<=SetLocalRotation;++i)poseContracts=poseContracts&&methods[i].resolved.method_info;
 auto getLease=reinterpret_cast<BE_GetPoseLeaseApiV1Fn>(GetProcAddress(GetModuleHandleW(L"BetterEndfield.Host.dll"),"BetterEndfield_GetPoseLeaseApiV1"));
 leases=getLease?getLease():nullptr;
 if(leases&&(leases->version!=1||!leases->acquire||!leases->owns||!leases->release))leases=nullptr;
 if(runtime->resolve_class(runtime->context,"UnityEngine.AnimationModule.dll","UnityEngine","Animator",&animator)!=BE_Result_Ok||!animator.type_object)return failure(BE_Result_NotFound,"UnityEngine.Animator");
 runtime->resolve_class(runtime->context,core,"UnityEngine","SkinnedMeshRenderer",&skinnedRenderer);runtime->resolve_class(runtime->context,"UnityEngine.UIModule.dll","UnityEngine","Canvas",&uiCanvas);
 if(runtime->resolve_field){BE_FieldDescriptorV1 field{game,"Beyond.Gameplay","GamePlayer","squadManager","Beyond.Gameplay.Core.SquadManager"};runtime->resolve_field(runtime->context,&field,&squad);}
 lookContracts=squad.field_info&&methods[Position].resolved.method_info&&methods[WorldRotation].resolved.method_info;
 for(int i=Player;i<=Loading;++i)lookContracts=lookContracts&&methods[i].resolved.method_info;
 standingContracts=lookContracts;for(int i=SetPosition;i<=SetLocalPosition;++i)standingContracts=standingContracts&&methods[i].resolved.method_info;
 movementContracts=true;for(int i=EntityValid;i<=MoveMode;++i)movementContracts=movementContracts&&methods[i].resolved.method_info;
 auto hooks=host->hooks;
 if(!hooks||hooks->version!=1||hooks->struct_size<sizeof(*hooks)||!hooks->create||!hooks->disable)return failure(BE_Result_NotReady,"hook chain API");
 auto result=hooks->create(hooks->context,id,methods[Tail].resolved.method_pointer,reinterpret_cast<void*>(&Detour),reinterpret_cast<void**>(&next),&hook);
 if(result!=BE_Result_Ok){hook=0;next=nullptr;return failure(result,"CameraManager.TailLateTick");}
 FreeCameraInput::toggleRequests=0;if(!FreeCameraInput::Install(host->hooks,id)||!InstallCameraInput()||!InstallCameraOutput())freeCamera.status="自由相機輸入攔截未接通，未啟用";Log("Diagnostic TailLateTick chain connected; scans are explicitly queued by UI");return BE_Result_Ok;
}
void ReadSavedGrip(const char* configuration){if(!configuration)return;try{auto config=Presets::Json::parse(configuration);if(!config.contains("grip_values"))return;Grip::Settings candidate=gripTarget;if(!candidate.Read(Presets::Json{{"grip_values",config.at("grip_values")}}.dump()))return;if(config.value("grip_schema",0)==1)for(int i=0;i<21;++i)std::swap(candidate.values[i],candidate.values[i+21]);gripTarget=candidate;}catch(...){}}
BE_Result BE_CALL Initialize(const BE_ThirdPartyHostV1* provided,const char* configuration){
 if(!provided||provided->version!=1||provided->struct_size<sizeof(*provided)||!provided->log||!provided->reply)return BE_Result_ContractMismatch;
 std::lock_guard lock(gate);host=provided;targetChoices.clear();selectedSlot=-1;selectedModel.clear();selectedIdentity.clear();boneTarget=Bones::Settings{};presetBank=Presets::Bank{};presetKeys.fill(true);activePreset=-1;pendingPreset=-1;pendingStart=-1;targetsPending=false;active=true;pending=false;restoreRequested=false;armed=false;keyDown=false;unityThread=0;
 wchar_t path[32768]{};HMODULE module=nullptr;
 if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&Initialize),&module)&&GetModuleFileNameW(module,path,32768)){
  // Log beside the local DLL; inability to write is reported, never redirected to game files.
  moduleFolder=std::filesystem::path(path).parent_path();
  output.open(moduleFolder/"Interaction.Diagnostics.log",std::ios::app);
 }
 currentUiSettings=false;panelBaseline="{}";if(configuration){try{auto c=Presets::Json::parse(configuration);panelBaseline=c.dump();currentUiSettings=c.value("ui_schema",0)>=16;if(c.contains("preset_bank"))presetBank.Read(c["preset_bank"]);}catch(...){}}
 lastControlled.clear();selectedControlledClip.clear();controlledPose="inherit";controlledSupport="none";controlledLoop=false;animationImportFolders={};blenderFolder.clear();supportTarget="none";armedMode=Mode::HeadTest;calibrationMode=false;calibrationLoop=false;gripTarget=Grip::Settings{};if(configuration){ReadSavedGrip(configuration);try{auto c=Presets::Json::parse(configuration);blenderFolder=c.value("blender_folder",std::string{});if(c.contains("bone_offsets"))boneTarget.Read(c["bone_offsets"]);if(c.contains("support_mode")&&Support::Valid(Support::Normalize(c["support_mode"])))supportTarget=Support::Normalize(c["support_mode"]);}catch(...){}}LoadPanelSettings();if(!currentUiSettings)presetBank.enabled=true;LoadAnimationLibrary();LoadAnimationOrder();auto loadedBones=boneTarget;workflow=Workflow::State{};workflow.draft.controlledPose=controlledPose;workflow.draft.controlledAnimation=selectedControlledClip;workflow.draft.controlledSupport=controlledSupport;fbxManagerAction=controlledPose;selectedControlledClip.clear();mainTimelineDraft={};pendingMainSeek.reset();pendingMainStop=pendingPartnerStop=false;workflow.draft.support=supportTarget;workflow.draft.bones=loadedBones;workflow.draft.grip=gripTarget;boneTarget=workflow.draft.bones;boneCatalog.clear();bonesPending=false;pendingPreview=false;pendingPreviewPaused=false;pendingSelectionStop=false;timelineDraft={};gripCurrent=gripTarget;
 movementPending=false;followPending=false;
 bool loadedA=handshakeA.Load(moduleFolder/"motions/18.asf",moduleFolder/"motions/18_01.amc");
 bool loadedB=handshakeB.Load(moduleFolder/"motions/19.asf",moduleFolder/"motions/19_01.amc");
 Log(std::string("Handshake source assets: ")+(loadedA&&loadedB?"loaded":"unavailable"));
 Log("EndfieldAnimationImporter 1.3.5-Alpha initialized; follow research is read-only; verified preview modes retained");
 FreeCameraInput::Start();
 if(!InteractionPanel::Start(PanelRead,PanelExecute))Log("Interaction overlay unavailable; use module web UI");
 if(!output)Log("Persistent log unavailable; Host retains only its recent diagnostic messages");
 return BE_Result_Ok;
}
BE_Result BE_CALL Configure(const char* configuration){std::lock_guard lock(gate);if(!configuration)return BE_Result_InvalidArgument;try{auto c=Presets::Json::parse(configuration);auto support=Support::Normalize(c.value("support_mode",supportTarget));if(!Support::Valid(support))return BE_Result_InvalidArgument;auto nextBank=presetBank;if(c.contains("preset_bank")&&!nextBank.Read(c["preset_bank"]))return BE_Result_InvalidArgument;auto previousGrip=gripTarget.Encode();ReadSavedGrip(configuration);if(c.contains("bone_offsets")){Bones::Settings candidate;if(!candidate.Read(c["bone_offsets"]))return BE_Result_InvalidArgument;if(candidate.Encode()!=boneTarget.Encode())workflow.Invalidate();boneTarget=candidate;workflow.draft.bones=boneTarget;}if(previousGrip!=gripTarget.Encode()){workflow.Invalidate();workflow.draft.grip=gripTarget;}if(supportTarget!=support)workflow.Invalidate();supportTarget=workflow.draft.support=support;blenderFolder=c.value("blender_folder",blenderFolder);presetBank=nextBank;panelBaseline=c.dump();presetKeys.fill(true);SavePanelSettings();return BE_Result_Ok;}catch(...){return BE_Result_InvalidArgument;}}
Presets::Json EfmiCatalog(){auto mods=Presets::Json::array();int index=0,meshIndex=0;for(auto& m:efmiScan.mods){auto controls=Presets::Json::array(),meshes=Presets::Json::array();for(auto& c:m.controls){controls.push_back({{"index",index++},{"key",EfmiBridge::Utf8(m.file)+"|"+c.binding},{"name",c.name},{"binding",c.binding},{"min",c.low},{"max",c.high},{"value",c.value},{"integer",c.integer},{"dirty",c.dirty}});}for(auto& mesh:m.meshes)meshes.push_back({{"index",meshIndex++},{"key",EfmiBridge::Utf8(m.file)+"|"+mesh.name},{"name",EfmiBridge::MeshLabel(mesh.name)},{"condition",mesh.condition},{"resource",mesh.resource},{"supported",mesh.supported},{"reason",mesh.reason},{"values",mesh.values},{"dirty",mesh.dirty}});mods.push_back({{"name",m.name},{"file",EfmiBridge::Utf8(m.file)},{"namespace",m.space},{"controls",controls},{"meshes",meshes}});}return {{"folder",externalFolder},{"reload_key",efmiScan.reloadKey},{"bridge_available",!efmiScan.efmiRoot.empty()},{"mods",mods},{"errors",efmiScan.errors}};}
void ScanExternalFolder(const std::string& path){auto scanned=EfmiBridge::Discover(std::filesystem::path(std::u8string(path.begin(),path.end())));efmiScan=std::move(scanned);for(auto& mod:efmiScan.mods)for(auto& mesh:mod.meshes)ExternalMesh::Probe(mod,mesh);externalFolder=EfmiBridge::Utf8(efmiScan.folder);std::vector<std::string> profileErrors;auto labels=ExternalModels::Load({moduleFolder/"../../external-models",PanelSettingsPath().parent_path()/"ExternalModels"},profileErrors);for(auto& m:efmiScan.mods)for(auto& c:m.controls){std::set<std::string> affected;for(auto& mesh:m.meshes)if(EfmiBridge::Uses(mesh.condition,c.binding))affected.insert(EfmiBridge::MeshLabel(mesh.name));c.name=affected.empty()?c.binding:*affected.begin()+(affected.size()>1?" 等 "+std::to_string(affected.size())+" 項":"");for(auto& p:labels)if(std::filesystem::path(std::u8string(p.source.begin(),p.source.end())).lexically_normal()==m.file.lexically_normal())for(auto& q:p.parameters)if(EfmiBridge::Lower(q.binding)==EfmiBridge::Lower(c.binding))c.name=q.name;}
 // Restore the bridge's overrides as editable pending values; source defaults stay untouched.
 auto pathOut=efmiScan.efmiRoot.empty()?std::filesystem::path{}:EfmiBridge::OutputPath(efmiScan);if(!pathOut.empty()&&std::filesystem::exists(pathOut)){EfmiBridge::Owned(pathOut);std::istringstream in(EfmiBridge::ReadFile(pathOut));std::string line;while(std::getline(in,line))for(auto& m:efmiScan.mods)for(auto& c:m.controls){auto prefix="post $\\"+m.space+"\\"+c.binding.substr(1)+" = ";if(line.starts_with(prefix)){try{auto value=std::stof(line.substr(prefix.size()));if(std::isfinite(value)&&value>=c.low&&value<=c.high){c.value=value;c.dirty=true;}}catch(...){}}}}
 for(auto& part:externalCatalog)if(part.value("backend",std::string{})=="efmi_shapekey")part["status"]="請在外部模型調整掃描來源資料夾，使用 INI 重載橋接";
 auto settingsPath=efmiScan.folder/"BEM.GeneratedMeshes"/"settings.json";if(std::filesystem::exists(settingsPath)){try{auto saved=Presets::Json::parse(EfmiBridge::ReadFile(settingsPath));if(saved.value("format",std::string{})=="bem-external-mesh-settings")for(auto& m:efmiScan.mods)for(auto& mesh:m.meshes){auto key=EfmiBridge::Utf8(m.file)+"|"+mesh.name;if(mesh.supported&&saved.at("values").contains(key)){auto values=saved["values"][key].get<std::array<float,12>>();if(ExternalMesh::Valid(values)){mesh.values=values;mesh.dirty=true;}}}}catch(...){Log("External mesh settings invalid; retain scan defaults");}}
 panelStatus="外部模型已掃描："+std::to_string(efmiScan.mods.size())+" 個 INI；可見狀態尚未驗證";SavePanelSettings();}
bool SetExternalValue(int index,const std::string& key,float value){for(auto& m:efmiScan.mods)for(auto& c:m.controls){if(index--!=0)continue;if(key!=EfmiBridge::Utf8(m.file)+"|"+c.binding||!std::isfinite(value)||value<c.low||value>c.high||(c.integer&&std::floor(value)!=value))return false;c.value=value;c.dirty=true;panelStatus="參數待寫入；寫入橋接後回遊戲重載 EFMI";return true;}return false;}
bool SetExternalMeshValue(int index,const std::string& key,int field,float value,bool linked){if(field<0||field>=12||!std::isfinite(value))return false;for(auto& mod:efmiScan.mods)for(auto& mesh:mod.meshes){if(index--!=0)continue;if(key!=EfmiBridge::Utf8(mod.file)+"|"+mesh.name||!mesh.supported)return false;auto next=mesh.values;next[field]=value;if(!ExternalMesh::Valid(next))return false;auto label=EfmiBridge::MeshLabel(mesh.name);for(auto& other:mod.meshes)if(&other==&mesh||(linked&&EfmiBridge::MeshLabel(other.name)==label)){if(!other.supported)return false;}for(auto& other:mod.meshes)if(&other==&mesh||(linked&&EfmiBridge::MeshLabel(other.name)==label)){other.values[field]=value;other.dirty=true;}panelStatus="網格調整待寫入；回遊戲重載 EFMI 套用";return true;}return false;}
bool ResetExternalMesh(int index,const std::string& key){for(auto& mod:efmiScan.mods)for(auto& mesh:mod.meshes){if(index--!=0)continue;if(key!=EfmiBridge::Utf8(mod.file)+"|"+mesh.name||!mesh.supported)return false;auto label=EfmiBridge::MeshLabel(mesh.name);for(auto& other:mod.meshes)if(EfmiBridge::MeshLabel(other.name)==label){other.values={0,0,0,0,0,0,1,1,1,0,0,0};ExternalMesh::Probe(mod,other);other.dirty=true;}panelStatus="網格已還原預設，寫入橋接並重載後套用";return true;}return false;}
bool ResetExternalMod(const std::string& file){
 auto found=std::find_if(efmiScan.mods.begin(),efmiScan.mods.end(),[&](auto& m){return EfmiBridge::Utf8(m.file)==file;});if(found==efmiScan.mods.end())return false;
 auto fresh=EfmiBridge::Parse(found->file,EfmiBridge::ReadFile(found->file),efmiScan.efmiRoot);auto restored=*found;
 for(auto& c:restored.controls){auto original=std::find_if(fresh.controls.begin(),fresh.controls.end(),[&](auto& f){return f.binding==c.binding;});if(original==fresh.controls.end())throw std::runtime_error("Mod parameters changed; rescan first");c.value=original->value;c.dirty=true;}
 for(auto& mesh:restored.meshes)if(mesh.supported){mesh.values={0,0,0,0,0,0,1,1,1,0,0,0};ExternalMesh::Probe(restored,mesh);if(!mesh.supported)throw std::runtime_error("Mesh changed; rescan first");mesh.dirty=true;}
 *found=std::move(restored);panelStatus="此模組已全部還原預設；請寫入橋接並重載 EFMI";return true;
}
void WriteExternalMeshes(){auto buffers=ExternalMesh::Build(efmiScan);if(buffers.empty())return;auto bridge=EfmiBridge::OutputPath(efmiScan);bridge.replace_filename(L"BEM.ExternalMesh.ini");if(std::filesystem::exists(bridge)&&!EfmiBridge::ReadFile(bridge).starts_with(ExternalMesh::Marker))throw std::runtime_error("Mesh bridge filename is occupied");auto revision=std::to_string(GetTickCount64());auto ini=ExternalMesh::Ini(buffers,revision);auto directory=efmiScan.folder/"BEM.GeneratedMeshes"/revision;if(std::filesystem::exists(directory))throw std::runtime_error("Mesh output revision collision; try again");std::filesystem::create_directories(directory);for(size_t i=0;i<buffers.size();++i){std::ofstream out(directory/("buffer"+std::to_string(i)+".buf"),std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(buffers[i].bytes.data()),buffers[i].bytes.size());out.flush();if(!out.good())throw std::runtime_error("Cannot write generated mesh buffer");}auto temp=bridge;temp+=L".bem-tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<ini;out.flush();if(!out.good())throw std::runtime_error("Cannot write mesh bridge");}if(!MoveFileExW(temp.c_str(),bridge.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit mesh bridge");auto values=Presets::Json::object();for(auto& m:efmiScan.mods)for(auto& mesh:m.meshes)if(mesh.dirty)values[EfmiBridge::Utf8(m.file)+"|"+mesh.name]=mesh.values;auto settings=efmiScan.folder/"BEM.GeneratedMeshes"/"settings.json";auto tempSettings=settings;tempSettings+=L".tmp";{std::ofstream out(tempSettings,std::ios::binary|std::ios::trunc);out<<Presets::Json{{"format","bem-external-mesh-settings"},{"values",values}}.dump(2);out.flush();if(!out.good())throw std::runtime_error("Mesh applied but settings save failed");}if(!MoveFileExW(tempSettings.c_str(),settings.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Mesh applied but settings commit failed");}
void WriteExternalBridge(){auto path=EfmiBridge::OutputPath(efmiScan);EfmiBridge::Owned(path);bool dirtyControls=false;for(auto& mod:efmiScan.mods)for(auto& control:mod.controls)dirtyControls|=control.dirty;if(!dirtyControls){bool dirtyMeshes=false;for(auto& mod:efmiScan.mods)for(auto& mesh:mod.meshes)dirtyMeshes|=mesh.dirty;if(!dirtyMeshes)throw std::runtime_error("Adjust a parameter or mesh before writing bridge");WriteExternalMeshes();panelStatus="網格橋接已寫入；回遊戲重載 EFMI（"+efmiScan.reloadKey+"）";return;}auto data=EfmiBridge::Generate(efmiScan);WriteExternalMeshes();auto temp=path;temp+=L".bem-tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<data;out.flush();if(!out.good())throw std::runtime_error("Cannot write bridge file");}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit bridge file");panelStatus="橋接已寫入；回遊戲重載 EFMI（"+efmiScan.reloadKey+"）";}
void RemoveExternalBridge(){auto meshPath=EfmiBridge::OutputPath(efmiScan);meshPath.replace_filename(L"BEM.ExternalMesh.ini");if(std::filesystem::exists(meshPath)){if(!EfmiBridge::ReadFile(meshPath).starts_with(ExternalMesh::Marker))throw std::runtime_error("Mesh bridge filename is occupied");auto disabled=meshPath;disabled.replace_extension(L"bem-disabled");if(!MoveFileExW(meshPath.c_str(),disabled.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot disable mesh bridge");}auto path=EfmiBridge::OutputPath(efmiScan);EfmiBridge::Owned(path);if(std::filesystem::exists(path)){auto disabled=path;disabled.replace_extension(L"bem-disabled");if(!MoveFileExW(path.c_str(),disabled.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot disable bridge");}panelStatus="橋接已停用；回遊戲重載 EFMI。已套用值可能被 EFMI 保存，需手動還原參數";}
bool DraftSourcesValid(){
 if(workflow.draft.animation.starts_with("clip:")){auto found=animationLibrary.find(workflow.draft.animation.substr(5));if(found==animationLibrary.end()||found->second->targetActor!=1)return false;}
 if(workflow.draft.controlledPose=="imported"){if(!workflow.draft.controlledAnimation.starts_with("clip:"))return false;auto found=animationLibrary.find(workflow.draft.controlledAnimation.substr(5));if(found==animationLibrary.end()||found->second->targetActor!=0||found->second->targetCharacter!=workflow.draft.controlled)return false;}
 return true;
}
BE_Result BE_CALL Message(const char* request,const char* body){
 std::lock_guard lock(gate);if(!host||!request||!body)return BE_Result_NotReady;
 bool mainActor=false;std::string message(body);BE_Result result=BE_Result_InvalidArgument;std::string response="{\"status\":\"unknown_command\"}";
 try{auto payload=Presets::Json::parse(message);auto command=payload.value("command",std::string{});mainActor=payload.value("actor",std::string("partner"))=="controlled";if(command!="animation_import"&&message.size()>1048576)return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"preset_payload_too_large\"}");
  if(command=="animation_delete"||command=="animations_reorder"){bool ok=command=="animation_delete"?DeleteAnimation(payload.at("id").get<std::string>()):ReorderAnimations(payload.at("order"));return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,ok?"{\"status\":\"animation_library_updated\"}":"{\"status\":\"animation_library_update_failed\"}");}
  if(command=="free_camera_hide_ui"||command=="free_camera_get"||command=="free_camera_set"||command=="free_camera_reset"||command=="free_camera_hotkey"){if(command=="free_camera_hide_ui"){freeCamera.hideUi=payload.value("hide_ui",false);SavePanelSettings();}if(command=="free_camera_set"){if(!hook){auto text=Presets::Json{{"detail","請先手動連接遊戲入口"}}.dump();return host->reply(host->context,request,BE_Result_NotReady,text.c_str());}freeCamera.request=payload.value("enabled",false)?1:0;if(freeCamera.request==1)freeCamera.status="等待遊戲更新啟用自由相機";}if(command=="free_camera_reset")freeCamera.reset=true;if(command=="free_camera_hotkey"){int key=payload.at("key");if(!FreeCamera::ValidHotkey(key))return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"detail\":\"Reserved or invalid camera shortcut\"}");FreeCameraInput::hotkey=key;FreeCameraInput::toggleRequests=0;SavePanelSettings();}auto text=Presets::Json{{"free_camera",FreeCameraJson()}}.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}
  if(command=="external_picker"){bool ok=InteractionPanel::RequestExternalPicker();return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_NotReady,ok?"{\"status\":\"external_picker_opening\"}":"{\"status\":\"external_picker_unavailable\"}");}
  if(command=="external_scan"||command=="external_get"||command=="external_set"||command=="external_apply"||command=="external_remove"||command=="external_mesh_set"||command=="external_mesh_reset"||command=="external_mod_reset"){try{if(command=="external_mod_reset"&&!ResetExternalMod(payload.at("file")))throw std::runtime_error("Mod missing; rescan first");if(command=="external_scan")ScanExternalFolder(payload.value("path",externalFolder));if(command=="external_set"&&!SetExternalValue(payload.at("index"),payload.at("key"),payload.at("value")))throw std::runtime_error("Invalid external parameter; rescan first");if(command=="external_mesh_reset"&&!ResetExternalMesh(payload.at("index"),payload.at("key")))throw std::runtime_error("Invalid mesh; rescan first");if(command=="external_mesh_set"&&!SetExternalMeshValue(payload.at("index"),payload.at("key"),payload.at("field"),payload.at("value"),payload.value("linked",true)))throw std::runtime_error("Invalid mesh value; rescan first");if(command=="external_apply")WriteExternalBridge();if(command=="external_remove")RemoveExternalBridge();auto data=EfmiCatalog();data["detail"]=panelStatus;auto text=data.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}catch(const std::exception& e){auto text=Presets::Json{{"detail",e.what()}}.dump();return host->reply(host->context,request,BE_Result_InvalidArgument,text.c_str());}}
  if(command=="bones_scan"){if(!hook)return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"connect_first\"}");bonesPending=true;return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"bones_scan_queued_return_to_game\"}");}
  if(command=="fbx_picker"){if(!hook)return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"connect_first\"}");auto manager=payload.value("manager_action",std::string("none"));if(manager!="imported"&&manager!="none"&&manager!="standing"&&manager!="waist_hold"&&manager!="front_waist"&&manager!="back_waist"&&manager!="head_pat")return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_manager_action\"}");if(!mainActor)fbxManagerAction=manager;bool ok=InteractionPanel::RequestFbxPicker(mainActor);return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_NotReady,ok?"{\"status\":\"fbx_picker_opening\"}":"{\"status\":\"fbx_picker_unavailable\"}");}
  if(command=="import_fbx"){bool ok=QueueFbxImport(payload.at("path").get<std::string>(),mainActor);return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_NotReady,ok?"{\"status\":\"fbx_queued\"}":"{\"status\":\"fbx_request_rejected\"}");}
  if(command=="fbx_status"){auto text=Presets::Json{{"status",fbxJob.process?"fbx_converting":fbxJob.waiting?"fbx_queued":"fbx_idle"},{"detail",panelStatus}}.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}
  if(command=="external_models_get"){auto data=Presets::Json{{"parts",externalCatalog}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}
  if(command=="bones_get"){int offset=payload.value("offset",0);if(offset<0||offset>int(boneCatalog.size()))return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_offset\"}");auto items=Presets::Json::array();for(int i=offset;i<std::min(offset+32,int(boneCatalog.size()));++i){auto& bone=boneCatalog[i];items.push_back({{"key",bone.key},{"name",bone.name},{"group",bone.group},{"actor",bone.actor},{"depth",bone.depth},{"values",boneTarget.Get(bone.key)}});}auto data=Presets::Json{{"type","bones_page"},{"generation",boneGeneration},{"total",boneCatalog.size()},{"offset",offset},{"scale_available",bool(methods[LocalScale].resolved.method_info&&methods[SetLocalScale].resolved.method_info)},{"bones",items}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}
  if(command=="bone_set"){auto key=payload.at("key").get<std::string>();int field=payload.at("field").get<int>();float value=payload.at("value").get<float>();auto found=std::find_if(boneCatalog.begin(),boneCatalog.end(),[&](auto& b){return b.key==key;});auto values=boneTarget.Get(key);if(payload.value("generation",uint64_t(0))!=boneGeneration||found==boneCatalog.end()||field<0||field>=9||field>=6&&(!methods[LocalScale].resolved.method_info||!methods[SetLocalScale].resolved.method_info))return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"refresh_bones_first\"}");values[field]=value;bool ok=boneTarget.Set(key,values);if(ok){if((key=="0|@root"||key=="1|@root")&&field<6){sceneAuditPending=true;++sceneRevision;Log("Root input revision="+std::to_string(sceneRevision)+" key="+key+" field="+std::to_string(field)+" value="+std::to_string(value));}workflow.draft.bones=boneTarget;workflow.Invalidate();}return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,ok?"{\"status\":\"bone_updated\"}":"{\"status\":\"invalid_bone_value\"}");}
  if(command=="bones_reset"){int actor=payload.value("actor",-1);if(actor<-1||actor>1)return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_actor\"}");boneTarget.Reset(actor);Grip::Settings defaults;if(actor<0)gripTarget=defaults;else std::copy_n(defaults.values.begin()+actor*21,21,gripTarget.values.begin()+actor*21);workflow.draft.grip=gripTarget;workflow.draft.bones=boneTarget;workflow.draft.support=supportTarget;workflow.Invalidate();return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"bone_defaults_restored\"}");}
  if(command=="support_set"){auto mode=payload.at("mode").get<std::string>();if(!Support::Valid(mode))return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_support_mode\"}");if(mainActor)controlledSupport=workflow.draft.controlledSupport=mode;else supportTarget=workflow.draft.support=mode;workflow.Invalidate();SavePanelSettings();auto data=Presets::Json{{"workflow",Workflow::EntryJson(workflow.draft)}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}
  if(command=="workflow_get"){workflow.draft.grip=gripTarget;workflow.draft.bones=boneTarget;workflow.draft.support=supportTarget;workflow.draft.support=supportTarget;auto text=Presets::Json{{"workflow",Workflow::EntryJson(workflow.draft)},{"loop_enabled",calibrationLoop},{"controlled_loop_enabled",controlledLoop},{"sync_animations",syncAnimations}}.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}
  if(command=="draft_set"){Presets::Entry entry;if(!Workflow::ReadEntry(payload.at("setting"),entry))return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_setting\"}");if(Presets::Administrator(lastControlled))entry.controlled=lastControlled;bool changedMain=workflow.draft.controlledAnimation!=entry.controlledAnimation||workflow.draft.controlledPose!=entry.controlledPose;bool changedAnimation=workflow.draft.animation!=entry.animation;workflow.draft=entry;controlledSupport=entry.controlledSupport;fbxManagerAction=entry.controlledPose;supportTarget=entry.support;gripTarget=entry.grip;boneTarget=entry.bones;workflow.Invalidate();if(changedAnimation)SelectCalibrationAnimation();if(changedMain)SelectCalibrationAnimation(true);auto data=Presets::Json{{"status","draft_updated"},{"workflow",Workflow::EntryJson(workflow.draft)},{"timeline",TimelineJson()},{"controlled_timeline",TimelineJson(true)}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}
  if(command=="prepare_export"){workflow.draft.grip=gripTarget;workflow.draft.bones=boneTarget;workflow.draft.support=supportTarget;bool ok=DraftSourcesValid()&&workflow.Prepare();auto data=Presets::Json{{"status",ok?"prepared":"invalid_setting"}};if(ok)data["calibration"]=Workflow::Export(workflow.prepared);auto text=data.dump();return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,text.c_str());}
  if(command=="calibration_save"){if(!workflow.ready)return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"prepare_first\"}");try{auto base=PanelSettingsPath();if(base.empty())throw std::runtime_error("Local application data unavailable");auto directory=base.parent_path()/"Exports";std::filesystem::create_directories(directory);auto path=directory/("interaction-calibration-"+std::to_string(GetTickCount64())+".json");std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<Workflow::Export(workflow.prepared).dump(2);out.flush();if(!out.good())throw std::runtime_error("Write failed");auto utf8=path.u8string();lastExportPath=std::string(utf8.begin(),utf8.end());auto data=Presets::Json{{"status","calibration_saved"},{"path",std::string(utf8.begin(),utf8.end())},{"calibration",Workflow::Export(workflow.prepared)}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}catch(...){return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"calibration_save_failed\"}");}}
  if(command=="calibration_export"){auto data=Presets::Json{{"status",workflow.ready?"prepared":"prepare_first"}};if(workflow.ready)data["calibration"]=Workflow::Export(workflow.prepared);auto text=data.dump();return host->reply(host->context,request,workflow.ready?BE_Result_Ok:BE_Result_NotReady,text.c_str());}
  if(command=="calibration_sync"){syncAnimations=payload.value("enabled",false);auto text=Presets::Json{{"sync_animations",syncAnimations},{"saved",SavePanelSettings()}}.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}
  if(command=="calibration_loop"){(mainActor?controlledLoop:calibrationLoop)=payload.value("enabled",false);auto text=Presets::Json{{"loop_enabled",calibrationLoop},{"controlled_loop_enabled",controlledLoop},{"sync_animations",syncAnimations},{"saved",SavePanelSettings()}}.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}
  if(command=="animation_pause"){calibrationMode=true;if(!SelectedTimelineActive(mainActor))return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"play_fbx_first\"}");(mainActor?pose.mainPaused:pose.clipPaused)=payload.value("paused",false);(mainActor?pose.mainScrubbing:pose.scrubbing)=mainActor?pose.mainPaused:pose.clipPaused;Event((mainActor?pose.mainPaused:pose.clipPaused)?"custom_animation_paused":"custom_animation_resumed");auto text=Presets::Json{{"paused",mainActor?pose.mainPaused:pose.clipPaused},{"time",mainActor?pose.mainTime:pose.clipTime}}.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}
  if(command=="blender_picker"){bool ok=InteractionPanel::RequestBlenderPicker();return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_NotReady,"{\"status\":\"blender_picker_requested\"}");}
  if(command=="blender_get"){auto data=Presets::Json{{"blender_folder",blenderFolder},{"blender_path",BlenderDisplayFolder()}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}
  if(command=="timeline_get"){auto text=Presets::Json{{"timeline",TimelineJson()},{"controlled_timeline",TimelineJson(true)}}.dump();return host->reply(host->context,request,BE_Result_Ok,text.c_str());}
  if(command=="timeline_action"){const std::map<std::string,InteractionPanel::Action> actions{{"seek",InteractionPanel::Action::Seek},{"step",InteractionPanel::Action::Step},{"in",InteractionPanel::Action::MarkIn},{"out",InteractionPanel::Action::MarkOut},{"reset",InteractionPanel::Action::RangeReset},{"loop",InteractionPanel::Action::RangeLoop},{"toggle",InteractionPanel::Action::Pause}};auto found=actions.find(payload.at("action").get<std::string>());bool ok=found!=actions.end()&&TimelineAction(found->second,payload.value("index",0),payload.value("time",0.f),mainActor);auto text=Presets::Json{{"timeline",TimelineJson()},{"controlled_timeline",TimelineJson(true)},{"status",ok?"timeline_updated":"invalid_timeline_action"}}.dump();return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,text.c_str());}
  if(command=="animation_slice"){if(!SelectedTimelineActive(mainActor))return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"play_fbx_first\"}");auto name=payload.at("name").get<std::string>();bool ok=!name.empty()&&name.size()<=160&&ImportAnimation(ImportedAnimation::Slice(*SelectedTimelineClip(mainActor),DraftTimeline(mainActor).start,DraftTimeline(mainActor).end,"slice_"+std::to_string(GetTickCount64()),name));return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,ok?"{\"status\":\"animation_imported\"}":"{\"status\":\"invalid_slice\"}");}
  if(command=="animation_save_picker"){bool ok=SelectedTimelineActive(mainActor)&&InteractionPanel::RequestAnimationSavePicker(mainActor);return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_NotReady,"{\"status\":\"save_picker_requested\"}");}
  if(command=="preview_stop"){if(pendingPreviewMain==mainActor)pendingPreview=false;QueueCalibrationStop(mainActor);return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"stop_queued\"}");}
  if(command=="preview_play"){if(!hook)return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"connect_first\"}");QueueCalibrationPreview(false,mainActor);return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"preview_queued\"}");}
  if(command=="prepared_to_slot"||command=="calibration_to_slot"){int index=payload.at("index").get<int>();if(index<0||index>=9)return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_slot\"}");bool ok=false;if(command=="prepared_to_slot")ok=workflow.Apply(presetBank,index);else{Presets::Entry entry;if(Workflow::Import(payload.at("calibration"),entry)){auto name=presetBank.slots[index].name;presetBank.slots[index]=entry;presetBank.slots[index].name=name;presetBank.slots[index].enabled=true;ok=true;}}if(ok){presetKeys.fill(true);ok=SavePanelSettings();}auto text=Presets::Json{{"status",ok?"slot_saved":"prepare_or_valid_json_required"},{"preset_bank",presetBank.Encode()}}.dump();return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,text.c_str());}
  if(command=="animation_import"){bool ok=ImportAnimation(payload.at("animation"));return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,ok?"{\"status\":\"animation_imported\"}":"{\"status\":\"animation_invalid_or_save_failed\"}");}
  if(command=="animations_get"){auto clips=Presets::Json::array();for(auto& key:animationOrder){auto clip=animationLibrary.at(key);clips.push_back({{"id",key},{"name",clip->name},{"duration",clip->duration},{"actor",clip->targetActor==0?"controlled":"partner"},{"target_character",clip->targetCharacter}});}auto data=Presets::Json{{"animations",clips}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}
  if(command=="presets_set"){auto bank=presetBank;if(!bank.Read(payload.at("preset_bank")))return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_preset_bank\"}");presetBank=bank;presetKeys.fill(true);if(!SavePanelSettings())return host->reply(host->context,request,BE_Result_NotReady,"{\"status\":\"save_failed\"}");return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"presets_updated\"}");}
  if(command=="presets_get"){auto data=Presets::Json{{"preset_bank",presetBank.Encode()}}.dump();return host->reply(host->context,request,BE_Result_Ok,data.c_str());}
  if(command=="apply_preset"||command=="start_preset"){int index=payload.at("index").get<int>();if(!hook||index<0||index>=9||!presetBank.slots[index].enabled)return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"preset_unavailable_connect_and_enable_slot\"}");if(command=="start_preset")pendingStart=index;else pendingPreset=index;return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"preset_queued_return_to_game\"}");}
 }catch(...){return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"invalid_message\"}");}
 if(message.find("\"overlay_show\"")!=std::string::npos||message.find("\"overlay_hide\"")!=std::string::npos){InteractionPanel::Show(message.find("\"overlay_show\"")!=std::string::npos);return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"overlay_visibility_updated\"}");}
 if(message.find("\"targets\"")!=std::string::npos){if(hook)targetsPending=true;return host->reply(host->context,request,hook?BE_Result_Ok:BE_Result_NotReady,hook?"{\"status\":\"targets_queued_return_to_game\"}":"{\"status\":\"connect_first\"}");}
 if(message.find("\"select_target\"")!=std::string::npos){
  if(pose.pins)return host->reply(host->context,request,BE_Result_Conflict,"{\"status\":\"stop_current_pose_before_selecting_target\"}");
  int slot=-1;auto at=message.find("\"target_slot\"");if(at!=std::string::npos){auto colon=message.find(':',at);if(colon!=std::string::npos){std::istringstream input(message.substr(colon+1));input>>slot;}}
  auto choice=targetChoices.find(slot);if(choice==targetChoices.end())return host->reply(host->context,request,BE_Result_InvalidArgument,"{\"status\":\"refresh_targets_and_select_again\"}");
  selectedSlot=slot;selectedModel=choice->second;selectedIdentity.clear();workflow.draft.target=Presets::Identity(choice->second);boneTarget.Reset(1);workflow.draft.bones=boneTarget;boneCatalog.clear();workflow.Invalidate();armed=false;activePreset=-1;
  return host->reply(host->context,request,BE_Result_Ok,"{\"status\":\"target_selected_save_preset\"}");
 }
 if(message.find("\"tune_get\"")!=std::string::npos){std::string json="{\"grip_values\":\""+gripTarget.Encode()+"\",\"calibration_mode\":"+(calibrationMode?"true":"false")+"}";return host->reply(host->context,request,BE_Result_Ok,json.c_str());}
 if(message.find("\"tune_set\"")!=std::string::npos){bool ok=gripTarget.Read(message);if(ok){workflow.draft.grip=gripTarget;workflow.Invalidate();}return host->reply(host->context,request,ok?BE_Result_Ok:BE_Result_InvalidArgument,ok?"{\"status\":\"grip_updated_next_gameplay_tick\"}":"{\"status\":\"invalid_grip_settings\"}");}
 if(message.find("\"tune_on\"")!=std::string::npos||message.find("\"tune_off\"")!=std::string::npos){calibrationMode=message.find("\"tune_on\"")!=std::string::npos;return host->reply(host->context,request,BE_Result_Ok,calibrationMode?"{\"status\":\"calibration_on_focus_loss_preserves_pose_up_to_120s\"}":"{\"status\":\"calibration_off\"}");}
 if(message.find("\"connect\"")!=std::string::npos){result=Connect();if(result==BE_Result_Ok){targetsPending=true;panelStatus="已連接，等待 Gameplay 更新角色清單";}response=Presets::Json{{"status",result==BE_Result_Ok?"connected":"connection_failed_check_host_log"},{"detail",panelStatus},{"result",int(result)}}.dump();}
 else if(message.find("\"scan\"")!=std::string::npos){result=hook?BE_Result_Ok:BE_Result_NotReady;if(hook)pending=true;response=hook?"{\"status\":\"queued_waiting_for_gameplay_tick\"}":"{\"status\":\"connect_first\"}";}
 else if(message.find("\"movement_status\"")!=std::string::npos){result=hook?BE_Result_Ok:BE_Result_NotReady;if(hook)movementPending=true;response=hook?"{\"status\":\"movement_diagnostics_queued\"}":"{\"status\":\"connect_first\"}";}
 else if(message.find("\"research_follow\"")!=std::string::npos){result=hook?BE_Result_Ok:BE_Result_NotReady;if(hook)followPending=true;response=hook?"{\"status\":\"follow_research_queued_return_to_game\"}":"{\"status\":\"connect_first\"}";}
 else if(message.find("\"arm_handshake\"")!=std::string::npos||message.find("\"arm_standing\"")!=std::string::npos||message.find("\"arm_mutual\"")!=std::string::npos||message.find("\"arm_lookat\"")!=std::string::npos||message.find("\"arm\"")!=std::string::npos){result=BE_Result_NotReady;response="{\"status\":\"pose_unavailable_use_presets\"}";}
 else if(message.find("\"restore\"")!=std::string::npos){armed=false;pendingPreset=-1;pendingStart=-1;pendingPreview=false;pendingSelectionStop=false;pendingTimeline=false;pendingSeek.reset();restoreRequested=true;result=BE_Result_Ok;response="{\"status\":\"restore_queued_return_to_game\"}";}
 // The business result is in the reply. Returning it again would make Host
 // overwrite useful details with a generic "Module message was rejected".
 return host->reply(host->context,request,result,response.c_str());
}
std::filesystem::path PanelSettingsPath(){wchar_t folder[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",folder,32768);return n&&n<32768?std::filesystem::path(folder)/L"BetterEndfield"/L"Interaction"/L"overlay-settings.json":std::filesystem::path{};}
bool ImportAnimation(const Presets::Json& data){auto clip=std::make_shared<ImportedAnimation::Clip>();if(!clip->Read(data)||(!animationLibrary.contains(clip->id)&&animationLibrary.size()>=128))return false;
#ifndef INTERACTION_NO_OVERLAY
 try{auto directory=PanelSettingsPath().parent_path()/L"Animations";if(directory.empty()||PanelSettingsPath().empty())return false;std::filesystem::create_directories(directory);auto path=directory/(clip->id+".json");auto temp=path;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<data.dump();out.flush();if(!out.good())return false;}if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;}catch(...){return false;}
#endif
 if(!animationLibrary.contains(clip->id)){if(animationLibrary.size()>=128)return false;animationOrder.push_back(clip->id);}animationLibrary[clip->id]=clip;if(workflow.draft.animation=="clip:"+clip->id)workflow.Invalidate();SaveAnimationOrder();panelStatus="動畫已匯入；在預設動作中選取 clip:"+clip->id;return true;}
void LoadAnimationLibrary(){animationLibrary.clear();
#ifndef INTERACTION_NO_OVERLAY
 try{auto path=PanelSettingsPath();if(path.empty())return;auto directory=path.parent_path()/L"Animations";if(!std::filesystem::exists(directory))return;int count=0;for(auto& file:std::filesystem::directory_iterator(directory)){if(count++>=128)break;if(!file.is_regular_file()||file.path().extension()!=L".json")continue;std::ifstream input(file.path(),std::ios::binary);auto data=Presets::Json::parse(input,nullptr,false);auto clip=std::make_shared<ImportedAnimation::Clip>();if(clip->Read(data))animationLibrary[clip->id]=clip;}}
 catch(...){Log("Animation library reload incomplete");}
#endif
}
bool SaveAnimationOrder(){
#ifdef INTERACTION_NO_OVERLAY
 return true;
#else
 try{auto base=PanelSettingsPath();if(base.empty())return false;std::filesystem::create_directories(base.parent_path());auto file=base.parent_path()/"animation-order.json",temp=file;temp+=".tmp";{std::ofstream out(temp);out<<Presets::Json(animationOrder).dump();out.flush();if(!out.good())return false;}return MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}catch(...){return false;}
#endif
}
void LoadAnimationOrder(){animationOrder.clear();
#ifndef INTERACTION_NO_OVERLAY
 try{auto file=PanelSettingsPath().parent_path()/"animation-order.json";if(std::filesystem::exists(file)&&std::filesystem::file_size(file)<32768){std::ifstream in(file);auto data=Presets::Json::parse(in);if(data.is_array())for(auto& value:data){auto key=value.get<std::string>();if(animationLibrary.contains(key)&&std::find(animationOrder.begin(),animationOrder.end(),key)==animationOrder.end())animationOrder.push_back(key);}}}catch(...){}
#endif
 for(auto& [key,clip]:animationLibrary)if(std::find(animationOrder.begin(),animationOrder.end(),key)==animationOrder.end())animationOrder.push_back(key);
}
bool ReorderAnimations(const Presets::Json& order){try{auto nextOrder=order.get<std::vector<std::string>>();if(nextOrder.size()!=animationLibrary.size())return false;std::set<std::string> seen;for(auto& key:nextOrder)if(!animationLibrary.contains(key)||!seen.insert(key).second)return false;auto old=animationOrder;animationOrder=nextOrder;if(!SaveAnimationOrder()){animationOrder=old;return false;}return true;}catch(...){return false;}}
bool DeleteAnimation(const std::string& key){if(!animationLibrary.contains(key))return false;
#ifndef INTERACTION_NO_OVERLAY
 try{auto base=PanelSettingsPath();if(base.empty())return false;auto file=base.parent_path()/"Animations"/(key+".json");if(std::filesystem::exists(file)&&!std::filesystem::remove(file))return false;}catch(...){return false;}
#endif
 animationLibrary.erase(key);std::erase(animationOrder,key);if(workflow.draft.animation=="clip:"+key||workflow.draft.controlledAnimation=="clip:"+key)workflow.Invalidate();SaveAnimationOrder();panelStatus="動畫已刪除；引用它的預設保留並標示缺失，正在播放的動畫直到結束";return true;}
bool SavePanelSettings(){
#ifdef INTERACTION_NO_OVERLAY
 return true;
#else
 try{auto file=PanelSettingsPath();if(file.empty())return false;std::filesystem::create_directories(file.parent_path());auto temp=file;temp+=L".tmp";auto data=Presets::Json{{"ui_schema",16},{"free_camera_hotkey",FreeCameraInput::hotkey.load()},{"free_camera_hide_ui",freeCamera.hideUi},{"preset_bank",presetBank.Encode()},{"grip_values",gripTarget.Encode()},{"grip_schema",2},{"bone_offsets",boneTarget.Encode()},{"calibration_loop",calibrationLoop},{"controlled_loop",controlledLoop},{"sync_animations",syncAnimations},{"controlled_support",workflow.draft.controlledSupport},{"controlled_pose",workflow.draft.controlledPose},{"controlled_animation",workflow.draft.controlledAnimation},{"support_mode",supportTarget},{"blender_folder",blenderFolder},{"animation_import_folders",animationImportFolders},{"external_folder",externalFolder},{"skeleton_references",EncodeSkeletonReferences()},{"base_configuration",panelBaseline}}.dump();{std::ofstream stream(temp,std::ios::binary|std::ios::trunc);stream<<data;stream.flush();if(!stream.good())return false;}return MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}catch(...){return false;}
#endif
}
void LoadPanelSettings(){
#ifndef INTERACTION_NO_OVERLAY
 try{auto file=PanelSettingsPath();if(file.empty()||!std::filesystem::exists(file)||std::filesystem::file_size(file)>33554432)return;std::ifstream in(file,std::ios::binary);auto data=Presets::Json::parse(in);freeCamera.hideUi=data.value("free_camera_hide_ui",false);int cameraKey=data.value("free_camera_hotkey",VK_F9);if(FreeCamera::ValidHotkey(cameraKey))FreeCameraInput::hotkey=cameraKey;if(data.contains("animation_import_folders")){const auto& folders=data["animation_import_folders"];if(folders.is_array()&&folders.size()==2&&folders[0].is_string()&&folders[1].is_string())animationImportFolders=folders.get<std::array<std::string,2>>();}if(data.value("base_configuration",std::string{})!=panelBaseline){Log("BEM configuration changed offline; retain new BEM configuration");return;}auto bank=presetBank;if(!bank.Read(data.at("preset_bank")))return;Grip::Settings settings;if(!settings.Read(Presets::Json{{"grip_values",data.at("grip_values")}}.dump()))return;blenderFolder=data.value("blender_folder",blenderFolder);externalFolder=data.value("external_folder",externalFolder);if(data.contains("skeleton_references"))ReadSkeletonReferences(data["skeleton_references"]);calibrationLoop=data.value("calibration_loop",false);controlledLoop=data.value("controlled_loop",false);syncAnimations=data.value("sync_animations",false);controlledSupport=data.value("controlled_support",std::string("none"));controlledPose=data.value("controlled_pose",std::string("inherit"));selectedControlledClip=data.value("controlled_animation",std::string("none"));auto savedSupport=Support::Normalize(data.value("support_mode",std::string("none")));if(Support::Valid(savedSupport))supportTarget=savedSupport;currentUiSettings=data.value("ui_schema",0)>=16;presetBank=bank;gripTarget=settings;if(data.contains("bone_offsets"))boneTarget.Read(data["bone_offsets"]);Log("Overlay settings restored from local application data");}catch(...){Log("Overlay settings invalid; retain BEM configuration");}
#endif
}
InteractionPanel::Snapshot PanelRead(){std::lock_guard lock(gate);InteractionPanel::Snapshot state;for(auto& p:externalCatalog){std::string line=p.value("name",std::string{})+" · "+p.value("backend",std::string{})+" · "+p.value("status",std::string{});state.externalModels.push_back(line);}state.freeCameraHideUi=freeCamera.hideUi;state.freeCameraEnabled=freeCamera.enabled||freeCamera.request==1;state.freeCameraHotkey=FreeCameraInput::hotkey;state.freeCameraStatus=freeCamera.status;state.animationImportFolders=animationImportFolders;state.syncAnimations=syncAnimations;state.externalFolder=externalFolder;state.externalReloadKey=efmiScan.reloadKey;int externalIndex=0;for(auto& m:efmiScan.mods){InteractionPanel::ExternalMod mod;mod.name=m.name;mod.file=EfmiBridge::Utf8(m.file);for(auto& c:m.controls)mod.controls.push_back({externalIndex++,mod.file+"|"+c.binding,c.name,c.binding,c.low,c.high,c.value,c.integer,c.dirty});for(auto& mesh:m.meshes){mod.meshes.push_back(mesh.name+(mesh.condition.empty()?"":" · "+mesh.condition));state.externalMeshes.push_back({int(state.externalMeshes.size()),EfmiBridge::Utf8(m.file)+"|"+mesh.name,EfmiBridge::Utf8(m.file),EfmiBridge::MeshLabel(mesh.name),mesh.reason,mesh.supported,mesh.values});}state.externalMods.push_back(std::move(mod));}for(auto& e:efmiScan.errors)state.externalErrors.push_back(e);auto timeline=TimelineJson();state.timelineTime=timeline["time"];state.timelineDuration=timeline["duration"];state.rangeStart=timeline["start"];state.rangeEnd=timeline["end"];state.rangeLoop=timeline["loop"];state.timelineActive=timeline["active"];state.clipPaused=timeline["paused"];state.playbackTime=state.timelineDuration>0?std::to_string(state.timelineTime)+" / "+std::to_string(state.timelineDuration)+(state.clipPaused?" 秒 · 暫停":" 秒 · 播放中"):"未選擇匯入動畫";auto mainTimeline=TimelineJson(true);state.mainTime=mainTimeline["time"];state.mainDuration=mainTimeline["duration"];state.mainStart=mainTimeline["start"];state.mainEnd=mainTimeline["end"];state.mainRangeLoop=mainTimeline["loop"];state.mainActive=mainTimeline["active"];state.mainPaused=mainTimeline["paused"];state.mainLoop=controlledLoop;state.mainSupport=controlledSupport;state.mainAnimation=workflow.draft.controlledAnimation;state.loopEnabled=calibrationLoop;state.blenderFolder=BlenderDisplayFolder();state.supportMode=supportTarget;state.fbxAction=fbxManagerAction;state.scaleAvailable=methods[LocalScale].resolved.method_info&&methods[SetLocalScale].resolved.method_info;for(auto& b:boneCatalog)state.bones.push_back({b.key,b.name,b.group,b.actor,boneTarget.Get(b.key)});state.connected=hook!=0;state.calibration=calibrationMode;state.hotkeys=presetBank.enabled;state.active=pose.pins?activePreset:-1;state.grip=gripTarget.values;for(auto& key:animationOrder){state.animations.push_back("clip:"+key);state.animationNames.push_back(animationLibrary.at(key)->name);state.animationActors.push_back(animationLibrary.at(key)->targetActor);}state.controlled=Presets::Administrator(lastControlled)?lastControlled:std::string{};state.draftTarget=workflow.draft.target;state.draftAnimation=workflow.draft.animation;state.prepared=workflow.ready;state.exportPath=lastExportPath;if(workflow.ready)state.exportJson=Workflow::Export(workflow.prepared).dump(2);state.status=panelStatus;const std::map<std::string,std::string> labels{{"controlled_animation_missing","主控動畫缺失，請先導入或選擇有效的主控動畫"},{"support_bone_missing","互動角色缺少所選支點骨架，請更新全部骨架或選不固定"},{"support_update_failed","支撐更新失敗，動畫已停止，請查看日誌"},{"preview_start_failed_check_log","播放啟動失敗，請查看模組日誌"},{"configured_bone_missing","設定中的骨架不存在，請更新骨架或還原預設"},{"bone_catalog_too_large","骨架掃描超過上限，無法啟動"},{"off","互動已停止並還原"},{"off_restoration_unverified","已停止，還原未確認"},{"handshake_preview_on","握手正在執行"},{"standing_preview_on","站位正在執行"},{"mutual_lookat_on","雙向對視正在執行"},{"lookat_on","LookAt 正在執行"},{"contact_unreachable","手臂無法觸及，互動已停止"},{"preset_target_missing_or_ambiguous","找不到預設互動角色"},{"preset_controlled_character_mismatch","主控與預設不符，請切換對應管理員"}};auto status=labels.find(panelStatus);if(status!=labels.end())state.status=status->second;for(int i=0;i<9;++i){auto& p=presetBank.slots[i];state.slots[i]={p.enabled,p.name,p.target,p.animation,p.controlledAnimation,p.controlledPose=="imported"};}for(const auto& [slot,name]:targetChoices){auto identity=Presets::Identity(name);if(!identity.empty()&&std::find(state.targets.begin(),state.targets.end(),identity)==state.targets.end())state.targets.push_back(identity);}return state;}
void PanelExecute(InteractionPanel::Command command){std::lock_guard lock(gate);if(!active||!host)return;using A=InteractionPanel::Action;bool mainActor=InteractionPanel::MainAction(command.action);int index=command.index;bool bankChanged=false;try{
 switch(command.action){
 case A::FreeCameraToggle:if(!hook)freeCamera.status="請先手動連接遊戲入口";else freeCamera.request=freeCamera.enabled?0:1;if(freeCamera.request==1)freeCamera.status="等待遊戲更新啟用自由相機";break;
 case A::FreeCameraHideUi:freeCamera.hideUi=!freeCamera.hideUi;SavePanelSettings();break;
 case A::FreeCameraReset:if(freeCamera.enabled)freeCamera.reset=true;else freeCamera.status="請先啟用自由相機";break;
 case A::FreeCameraHotkey:if(FreeCamera::ValidHotkey(command.index)){FreeCameraInput::hotkey=command.index;FreeCameraInput::toggleRequests=0;SavePanelSettings();}break;
 case A::BlenderFolder:SetBlenderFolder(command.text);break;
 case A::ExternalFolder:case A::ExternalScan:try{ScanExternalFolder(command.text.empty()?externalFolder:command.text);}catch(const std::exception& e){panelStatus=e.what();}break;
 case A::ExternalValue:if(!SetExternalValue(index,command.text,command.value))panelStatus="參數無效，請重新掃描";break;
 case A::ExternalMeshReset:if(!ResetExternalMesh(index,command.text))panelStatus="請重新掃描網格";break;
 case A::ExternalModReset:try{if(!ResetExternalMod(command.text))panelStatus="請重新掃描模組";}catch(const std::exception& e){panelStatus=e.what();}break;
 case A::ExternalMeshValue:if(!SetExternalMeshValue(index,command.text,int(command.extraValue),command.value,true))panelStatus="網格參數無效";break;
 case A::ExternalApply:try{WriteExternalBridge();}catch(const std::exception& e){panelStatus=e.what();}break;
 case A::ExternalRemove:try{RemoveExternalBridge();}catch(const std::exception& e){panelStatus=e.what();}break;
 case A::BoneScan:if(hook){bonesPending=true;panelStatus="骨架掃描已排隊，回遊戲等待";}else panelStatus="請先連接入口";break;
 case A::BoneValue:{auto found=std::find_if(boneCatalog.begin(),boneCatalog.end(),[&](auto& b){return b.key==command.text;});if(found==boneCatalog.end()||index<0||index>=9){panelStatus="請更新骨架";break;}if(index>=6&&(!methods[LocalScale].resolved.method_info||!methods[SetLocalScale].resolved.method_info)){panelStatus="縮放 API 不可用";break;}auto values=boneTarget.Get(command.text);values[index]=command.value;if(boneTarget.Set(command.text,values)){if((command.text=="0|@root"||command.text=="1|@root")&&index<6){sceneAuditPending=true;++sceneRevision;Log("Root input revision="+std::to_string(sceneRevision)+" key="+command.text+" field="+std::to_string(index)+" value="+std::to_string(command.value));}workflow.draft.bones=boneTarget;workflow.Invalidate();panelStatus="骨架參數下一次 Gameplay 更新套用";}else panelStatus="骨架參數超出範圍";break;}
 case A::DraftTarget:{if(pose.pins){panelStatus="請先停止播放再換角色";break;}std::vector<std::string> targets;for(auto& [slot,name]:targetChoices){auto key=Presets::Identity(name);if(!key.empty())targets.push_back(key);}if(targets.empty()){panelStatus="請先更新隊伍";break;}auto it=std::find(targets.begin(),targets.end(),workflow.draft.target);workflow.draft.target=targets[it==targets.end()?0:(size_t(it-targets.begin())+1)%targets.size()];boneTarget.Reset(1);workflow.draft.bones=boneTarget;boneCatalog.clear();workflow.Invalidate();panelStatus="互動角色已選擇";break;}
 case A::DraftMotion:{std::vector<std::string> modes{"handshake","standing","mutual","lookat","head"};for(auto& key:animationOrder)if(animationLibrary.contains(key)&&animationLibrary.at(key)->targetActor==1)modes.push_back("clip:"+key);if(!command.text.empty()){if(std::find(modes.begin(),modes.end(),command.text)==modes.end()){panelStatus="動畫不存在";break;}workflow.draft.animation=command.text;}else{auto it=std::find(modes.begin(),modes.end(),workflow.draft.animation);workflow.draft.animation=modes[it==modes.end()?0:(size_t(it-modes.begin())+1)%modes.size()];}workflow.Invalidate();SelectCalibrationAnimation();break;}
 case A::ControlledMotion:{if(command.text=="none"){workflow.draft.controlledAnimation="none";workflow.draft.controlledPose=fbxManagerAction="none";workflow.Invalidate();SelectCalibrationAnimation(true);break;}auto key=command.text.starts_with("clip:")?command.text.substr(5):std::string{};auto found=animationLibrary.find(key);if(found==animationLibrary.end()||found->second->targetActor!=0){panelStatus="主控動畫不存在";break;}workflow.draft.controlledAnimation=command.text;workflow.draft.controlledPose=fbxManagerAction="imported";workflow.Invalidate();SelectCalibrationAnimation(true);break;}
 case A::SupportMode:{if(!Support::Valid(command.text)){panelStatus="支撐模式無效";break;}if(mainActor)workflow.draft.controlledSupport=controlledSupport=command.text;else workflow.draft.support=supportTarget=command.text;workflow.Invalidate();bankChanged=true;panelStatus=mainActor?"支撐模式已更新（主控）；需重新準備匯出":"支撐模式已更新（配角）；需重新準備匯出";break;}
 case A::SyncAnimations:syncAnimations=!syncAnimations;bankChanged=true;break;
 case A::Loop:(mainActor?controlledLoop:calibrationLoop)=!(mainActor?controlledLoop:calibrationLoop);bankChanged=true;panelStatus=(mainActor?controlledLoop:calibrationLoop)?"校準 FBX 循環已啟用":"校準循環已關閉，播放到本輪結束";break;
 case A::TimelinePlay:case A::Seek:case A::Step:case A::MarkIn:case A::MarkOut:case A::RangeReset:case A::RangeLoop:if(!TimelineAction(command.action,index,command.value,mainActor))panelStatus="請選擇有效的匯入動畫與區間";break;
 case A::SliceSave:if(SelectedTimelineActive(mainActor)){try{if(command.text.empty())throw std::runtime_error("請輸入片段名稱");auto data=ImportedAnimation::Slice(*SelectedTimelineClip(mainActor),DraftTimeline(mainActor).start,DraftTimeline(mainActor).end,"slice_"+std::to_string(GetTickCount64()),command.text);if(!ImportAnimation(data))throw std::runtime_error("保存動畫庫失敗");panelStatus="新片段已保存至動畫庫；需要檔案時選取該片段並匯出";}catch(const std::exception& e){panelStatus=e.what();}}else panelStatus="請先播放動畫";break;
 case A::ExportAnimation:{try{if(!SelectedTimelineActive(mainActor))throw std::runtime_error("請先預覽所選動畫");auto clip=SelectedTimelineClip(mainActor);auto data=ImportedAnimation::Slice(*clip,DraftTimeline(mainActor).start,DraftTimeline(mainActor).end,"slice_"+std::to_string(GetTickCount64()),clip->name+" · 片段");auto path=std::filesystem::path(std::u8string(command.text.begin(),command.text.end()));std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<data.dump(2);out.flush();panelStatus=out.good()?"動畫 JSON 已匯出":"動畫匯出失敗";}catch(const std::exception& e){panelStatus=e.what();}break;}
 case A::Pause:if(!TimelineAction(A::Pause,0,0,mainActor))panelStatus="請先選擇匯入動畫";break;
 case A::Preview:if(hook){QueueCalibrationPreview(false,mainActor);panelStatus="播放已排隊，等待 Gameplay 更新";}else panelStatus="請先連接入口";break;
 case A::Prepare:workflow.Invalidate();workflow.draft.grip=gripTarget;workflow.draft.bones=boneTarget;workflow.draft.support=supportTarget;if(!DraftSourcesValid()){panelStatus="動畫缺失或主控不符，請重新選擇對應動畫";break;}panelStatus=workflow.Prepare()?"已準備，可匯出 JSON 或套用槽位":"設定無效";break;
 case A::Reset:{Grip::Settings defaults;if(index<0)gripTarget=defaults;else if(index<2)std::copy_n(defaults.values.begin()+index*21,21,gripTarget.values.begin()+index*21);boneTarget.Reset(index);workflow.draft.bones=boneTarget;workflow.Invalidate();panelStatus="骨架參數已還原預設；需重新準備";break;}
 case A::DeleteAnimation:panelStatus=DeleteAnimation(command.text)?"動畫已刪除，引用它的槽位需重新指定":"刪除失敗";break;
 case A::ReorderAnimation:{auto order=animationOrder;auto it=std::find(order.begin(),order.end(),command.text);if(it==order.end()||index<0||index>=int(order.size())){panelStatus="排序無效";break;}auto key=*it;order.erase(it);order.insert(order.begin()+index,key);panelStatus=ReorderAnimations(order)?"動畫順序已儲存":"排序失敗";break;}
 case A::Export:{if(!workflow.ready){panelStatus="請先準備匯出";break;}try{auto path=std::filesystem::path(std::u8string(command.text.begin(),command.text.end()));std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<Workflow::Export(workflow.prepared).dump(2);out.flush();if(out.good())lastExportPath=command.text;panelStatus=out.good()?"校準 JSON 已匯出":"匯出失敗";}catch(...){panelStatus="匯出失敗";}break;}
 case A::ImportSetting:{if(index<0||index>=9)break;try{auto path=std::filesystem::path(std::u8string(command.text.begin(),command.text.end()));if(std::filesystem::file_size(path)>4194304){panelStatus="設定超過 4 MB";break;}std::ifstream in(path);Presets::Entry entry;if(!Workflow::Import(Presets::Json::parse(in),entry)){panelStatus="校準設定 JSON 無效";break;}entry.name=presetBank.slots[index].name;presetBank.slots[index]=entry;bankChanged=true;panelStatus=SavePanelSettings()?"設定 JSON 已讀入此槽":"已讀入，但保存失敗";}catch(...){panelStatus="無法讀取設定 JSON";}break;}
 case A::UiMode:presetKeys.fill(true);panelStatus=command.value>0?"UI 操作模式：Num0／Esc 返回遊戲":"已返回遊戲";break;
 case A::Connect:if(Connect()==BE_Result_Ok){targetsPending=true;panelStatus="已連接，等待隊伍掃描";}break;
 case A::Scan:if(hook){targetsPending=true;panelStatus="已排入隊伍掃描";}else panelStatus="請先連接入口";break;
 case A::Stop:if(pendingPreviewMain==mainActor)pendingPreview=false;if(pose.pins&&pose.mode==Mode::Custom&&(mainActor||pose.controlledClip)){QueueCalibrationStop(mainActor);panelStatus="停止已排隊，等待 Gameplay 還原";break;}pendingSelectionStop=false;pendingTimeline=false;pendingSeek.reset();pendingPreview=false;pendingStart=pendingPreset=-1;armed=false;restoreRequested=true;panelStatus="停止已排隊，等待 Gameplay 還原";break;
 case A::Calibration:calibrationMode=!calibrationMode;panelStatus=calibrationMode?"校準模式開啟":"校準模式關閉";break;
 case A::Hotkeys:presetBank.enabled=!presetBank.enabled;presetKeys.fill(true);bankChanged=true;panelStatus="快捷鍵已更新，請儲存設定";break;
 case A::Grip:if(index>=0&&index<42&&std::isfinite(command.value)){int field=index%21;float lo=field<3?-8.f:field<6?-60.f:-20.f,hi=field<3?8.f:field<6?60.f:100.f;if(command.value>=lo&&command.value<=hi){gripTarget.values[index]=command.value;workflow.Invalidate();panelStatus=pose.pins&&pose.mode==Mode::Custom?"校準參數已更新，需重新準備匯出":"骨架參數下一個 Gameplay 更新套用";}}break;
 case A::FbxAction:{const std::vector<std::string> choices{"none","standing","front_waist","back_waist","head_pat"};auto choice=command.text.empty()?choices[std::clamp(index,0,int(choices.size()-1))]:command.text;if(std::find(choices.begin(),choices.end(),choice)==choices.end())break;fbxManagerAction=workflow.draft.controlledPose=choice;workflow.Invalidate();SelectCalibrationAnimation(true);break;}
 case A::ImportControlledFbx:QueueFbxImport(command.text,true);break;
 case A::ImportFbx:QueueFbxImport(command.text);break;
 case A::Import:{try{auto path=std::filesystem::path(std::u8string(command.text.begin(),command.text.end()));std::ifstream in(path,std::ios::binary);auto json=Presets::Json::parse(in);if(!ImportAnimation(json))panelStatus="動畫格式不合法或保存失敗";}catch(...){panelStatus="無法讀取動畫檔";}break;}
 case A::Save:panelStatus=SavePanelSettings()?"全部設定已儲存":"儲存失敗，請查看資料夾權限";break;
 default:if(index<0||index>=9)return;auto& p=presetBank.slots[index];
  if(command.action==A::Start){if(!hook||!p.enabled){panelStatus="請先連接並啟用此預設";break;}pendingStart=index;panelStatus="開始／停止已排隊，等待 Gameplay 更新";}
  else if(command.action==A::Enabled){p.enabled=!p.enabled;bankChanged=true;panelStatus="預設啟用狀態已更新，請儲存";}
  else if(command.action==A::Motion){std::vector<std::string> modes{"handshake","standing","mutual","lookat","head"};for(auto& [id,clip]:animationLibrary)if(clip->targetActor==1)modes.push_back("clip:"+id);size_t found=0;for(size_t i=0;i<modes.size();++i)if(p.animation==modes[i])found=i;p.animation=modes[(found+1)%modes.size()];bankChanged=true;panelStatus="動作已更新，請儲存";}
  else if(command.action==A::Target){std::vector<std::string> targets;for(const auto& [slot,name]:targetChoices){auto identity=Presets::Identity(name);if(!identity.empty()&&identity!=p.controlled&&std::find(targets.begin(),targets.end(),identity)==targets.end())targets.push_back(identity);}if(targets.empty()){panelStatus="請先掃描隊伍角色";break;}auto it=std::find(targets.begin(),targets.end(),p.target);p.target=targets[it==targets.end()?0:(size_t(it-targets.begin())+1)%targets.size()];bankChanged=true;panelStatus="互動角色已更新，請儲存";}
  else if(command.action==A::Capture){if(!workflow.Apply(presetBank,index)){panelStatus="請先準備匯出，再套用此槽";break;}bankChanged=true;panelStatus=SavePanelSettings()?"已準備設定已匯入此槽":"已更新，但儲存失敗";}
 }
 if(bankChanged&&host->emit){auto body=Presets::Json{{"preset_bank",presetBank.Encode()}}.dump();host->emit(host->context,body.c_str());}
 }catch(...){panelStatus="面板操作失敗";}}
void BE_CALL Shutdown(){
 InteractionPanel::Stop();
 {
  std::unique_lock lock(gate);armed=false;CancelFbxImport();freeCamera.request=0;if(freeCamera.enabled){restored.wait_for(lock,std::chrono::milliseconds(300),[]{return !freeCamera.enabled;});if(freeCamera.enabled){freeCamera.enabled=false;freeCamera.pins.reset();freeCamera.status="自由相機已關閉";}}
  if(pose.pins){restoreRequested=true;restored.wait_for(lock,std::chrono::milliseconds(300),[]{return !pose.pins;});
   if(pose.pins)StopPose("shutdown without Gameplay tick",false);
  }
 }
 active=false;pending=false;movementPending=false;followPending=false;
 if(host&&host->hooks){for(size_t i=0;i<cameraInputHookCount;++i)host->hooks->disable(host->hooks->context,cameraInputHooks[i].handle);cameraInputHookCount=0;cameraInputReady=false;}
 if(host&&host->hooks&&cameraPushHook)host->hooks->disable(host->hooks->context,cameraPushHook);cameraPushHook=0;cameraPushReady=false;
 if(host&&hook&&host->hooks)host->hooks->disable(host->hooks->context,hook);
 FreeCameraInput::Stop();
 std::lock_guard lock(gate);hook=0;host=nullptr;runtime=nullptr;output.close();
}
const BE_ThirdPartyModuleV1 api{sizeof(BE_ThirdPartyModuleV1),1,id,Initialize,Configure,Message,Shutdown};
}
BE_EXPORT const BE_ThirdPartyModuleV1* BE_CALL BetterEndfield_GetThirdPartyModuleV1(){return &api;}

