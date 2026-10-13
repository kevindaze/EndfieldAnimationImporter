struct BreastRuntime {
 bool enabled=false,automatic=true,automaticMain=false,reset=true;std::string manual,status="胸部物理未啟用",identity;
 std::unique_ptr<Pins> pins;void* root=nullptr;void* chest=nullptr;void* right=nullptr;void* left=nullptr;void* entity=nullptr;void* model=nullptr;void* head=nullptr;void* shoulderL=nullptr;void* shoulderR=nullptr;PoseMath::Vector3 lever{0,-.04f,.1f};PoseMath::Quaternion chestReference{};bool chestReferenceReady=false,leverReady=false;
 uint64_t lease=0;bool owned=false,wrote=false;PoseMath::Quaternion baseR{},baseL{},writtenR{},writtenL{};
 BreastPhysics::DirectionalResponse spring;float time=0;unsigned loops=0;bool wasPose=false;ULONGLONG scanned=0;
 bool cached=false;ULONGLONG cachedAt=0,loggedAt=0;unsigned long long replays=0;float outputDegrees=0;
} breast;
const Presets::Json& BreastDefaults(){static auto value=Presets::Json::parse(breastDefaultData);return value;}
void RemoveBreastLayer(){if(!breast.wrote)return;PoseMath::Quaternion q{};if(Living(breast.right)&&Rotation(breast.right,q)&&PoseMath::SameRotation(q,breast.writtenR))WriteRotation(breast.right,breast.baseR);if(Living(breast.left)&&Rotation(breast.left,q)&&PoseMath::SameRotation(q,breast.writtenL))WriteRotation(breast.left,breast.baseL);breast.wrote=false;}
void ClearBreast(){RemoveBreastLayer();breast.cached=false;breast.chestReferenceReady=breast.leverReady=false;breast.outputDegrees=0;if(breast.owned&&breast.root&&leases)leases->release(breast.root,id,breast.lease);breast.pins.reset();breast.root=breast.chest=breast.right=breast.left=breast.entity=breast.model=breast.head=breast.shoulderL=breast.shoulderR=nullptr;breast.owned=false;breast.lease=0;breast.spring.Reset();breast.identity.clear();breast.wasPose=false;}
void* BreastFind(void* node,const std::string& name,Pins& pins,int& budget,int depth=0){if(!node||depth>48||--budget<0)return nullptr;if(ObjectName(node)==name)return node;int count=0;if(!Value(ChildCount,node,count)||count<0||count>2048)return nullptr;for(int i=0;i<count&&budget>0;++i){void* args[]{&i};auto child=pins.Keep(Call(Child,node,args));if(auto found=BreastFind(child,name,pins,budget,depth+1))return found;}return nullptr;}
bool BindBreast(const std::string& target){
 ClearBreast();const auto& data=BreastDefaults();if(!data.contains(target)){breast.status="此角色沒有胸部物理預設";return false;}const auto& p=data.at(target);
 if(!p.value("enabled",true)){breast.status="原模組未啟用此角色的預設";return false;}
 auto pins=std::make_unique<Pins>();int count=0;auto manager=Squad(*pins,count);auto main=Call(Main);int matches=0;void* root=nullptr;void* entity=nullptr;void* model=nullptr;void* head=nullptr;void* shoulderL=nullptr;void* shoulderR=nullptr;PoseMath::Vector3 lever{0,-.04f,.1f};PoseMath::Quaternion chestReference{};bool chestReferenceReady=false;
 auto consider=[&](void* member){if(!member)return;auto m=pins->Keep(Call(Model,member));auto go=m?pins->Keep(Call(ModelGo,m)):nullptr;if(!Living(go)||Presets::Identity(ObjectName(go))!=target)return;root=pins->Keep(Call(GoTransform,go));entity=member;model=go;++matches;};
 if(breast.automaticMain)consider(pins->Keep(main));else for(int i=0;manager&&i<count;++i){void* args[]{&i};auto member=pins->Keep(Call(Member,manager,args));if(member!=main)consider(member);}

 if(matches!=1||!Living(root)){breast.status="找不到唯一的指定角色";return false;}
 auto find=[&](const std::string& name){int budget=1400;return BreastFind(root,name,*pins,budget);};
 void* right=find(p.value("right",std::string{})),*left=find(p.value("left",std::string{}));
 if((!right||!left)&&p.value("fallback",true)){const char* pairs[][2]={{"breast_R_01_jnt","breast_L_01_jnt"},{"R_breast_01_jnt","L_breast_01_jnt"},{"xiong_R_0_skin_jnt","xiong_L_0_skin_jnt"},{"breast_R_01","breast_L_01"},{"R_breast_01","L_breast_01"},{"xiong_R_0_skin","xiong_L_0_skin"}};for(auto& pair:pairs){right=find(pair[0]);left=find(pair[1]);if(right&&left)break;}}
 auto chest=right?pins->Keep(Call(Parent,right)):nullptr;if(!right||!left||!Living(chest)){breast.status="角色缺少胸部骨架，無法套用物理";return false;}
 breast.head=find("Bip001_Head");breast.shoulderL=find("Bip001_L_Clavicle");breast.shoulderR=find("Bip001_R_Clavicle");if(!breast.shoulderL||!breast.shoulderR){breast.shoulderL=find("Bip001_L_UpperArm");breast.shoulderR=find("Bip001_R_UpperArm");}if(!breast.head||!breast.shoulderL||!breast.shoulderR){breast.status="缺少胸腔方向骨架，無法套用物理";breast.head=breast.shoulderL=breast.shoulderR=nullptr;return false;}
 breast.root=root;breast.entity=entity;breast.model=model;breast.right=right;breast.left=left;breast.chest=chest;breast.identity=target;breast.pins=std::move(pins);Log("Breast bones bound: "+target+" right="+ObjectName(right)+" left="+ObjectName(left)+" driver="+ObjectName(chest));return true;
}
struct BreastPoseContext {bool borrowed=false,animated=false,paused=false,stopped=false;bool main=false;uint64_t lease=0;float time=0;unsigned loops=0;};
BreastPoseContext BreastContext(){BreastPoseContext c;if(!pose.pins)return c;c.main=pose.root==breast.root;c.borrowed=c.main||pose.reverse.root==breast.root;if(!c.borrowed)return c;c.lease=c.main?pose.lease:pose.reverse.lease;c.stopped=c.main?pose.mainStopped:pose.partnerStopped;bool controlled=c.main&&bool(pose.controlledClip);c.animated=pose.mode==Mode::Custom&&!c.stopped&&(controlled||pose.clip&&(c.main?pose.clip->targetActor==0:pose.clip->targetActor==1));c.paused=c.animated&&(controlled?pose.mainPaused:pose.clipPaused);c.time=controlled?pose.mainTime:pose.clipTime;c.loops=c.main?0:pose.loopCount;return c;}
std::string BreastTarget(){if(!breast.automaticMain)return breast.automatic?workflow.draft.target:breast.manual;Pins pins;auto main=pins.Keep(Call(Main));auto model=main?pins.Keep(Call(Model,main)):nullptr;auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;return go?Presets::Identity(ObjectName(go)):std::string{};}
#include "breast_physics_hooks.inc"
std::string BreastDiagnostics(){return "動畫回呼："+std::to_string(breastHooks.calls[0])+" / "+std::to_string(breastHooks.calls[1])+" / "+std::to_string(breastHooks.calls[2])+" · 配角回呼："+std::to_string(breastHooks.matched)+" · 重播："+std::to_string(breast.replays)+" · 角度："+std::to_string(breast.outputDegrees);}
Presets::Json BreastJson(){auto characters=Presets::Json::array();for(auto& [key,p]:BreastDefaults().items())characters.push_back({{"id",key},{"name",p.value("name",key)}});return {{"enabled",breast.enabled},{"automatic",breast.automatic},{"automatic_main",breast.automaticMain},{"manual",breast.manual},{"status",breast.status},{"diagnostic",BreastDiagnostics()},{"output_degrees",breast.outputDegrees},{"hooks_ready",breastHooks.ready},{"replays",breast.replays},{"characters",characters}};}
void PumpBreast(float dt){
 if(!breast.enabled){ClearBreast();breast.status="胸部物理未啟用";return;}
 if(!breastHooks.ready){ClearBreast();breast.status=breastHooks.error.empty()?"胸部物理動畫入口尚未就緒":breastHooks.error;return;}
 auto target=BreastTarget();
 if(breast.reset){ClearBreast();breast.scanned=0;breast.reset=false;}
 if(target.empty()){ClearBreast();breast.status="請選擇套用角色";return;}
 if(breast.root){Pins pins;bool found=false;auto main=Call(Main);if(breast.automaticMain){auto model=main==breast.entity?Call(Model,main):nullptr;found=model&&Call(ModelGo,model)==breast.model;}else{int count=0;auto manager=Squad(pins,count);for(int i=0;manager&&i<count;++i){void* args[]{&i};auto member=Call(Member,manager,args);if(member!=breast.entity||member==main)continue;auto model=Call(Model,member);found=model&&Call(ModelGo,model)==breast.model;}}if(!found||!Living(breast.root)||target!=breast.identity)ClearBreast();}

 if(!breast.root){if(GetTickCount64()-breast.scanned<500)return;breast.scanned=GetTickCount64();if(!BindBreast(target))return;}
 auto context=BreastContext();
 if(breast.owned&&context.borrowed){ClearBreast();return;}
 if(!context.borrowed&&!breast.owned){breast.lease=leases->acquire(breast.root,id);breast.owned=breast.lease!=0;}
 if(!(context.borrowed?leases->owns(breast.root,id,context.lease):breast.owned&&leases->owns(breast.root,id,breast.lease))){breast.status="角色骨架控制權暫不可用";return;}
 bool discontinuity=context.animated!=breast.wasPose||(context.animated&&(context.loops!=breast.loops||context.time<breast.time||std::abs(context.time-breast.time)>std::max(.12f,dt*2)));breast.wasPose=context.animated;breast.time=context.time;breast.loops=context.loops;if(discontinuity)breast.spring.Reset();
 PoseMath::Vector3 p{},head{},left{},right{},positionR{},positionL{};PoseMath::Quaternion frame{},chestWorld{},worldR{},worldL{};
 if(!Value(Position,breast.chest,p)||!Value(WorldRotation,breast.chest,chestWorld)||!PoseMath::Normalize(chestWorld)||!Rotation(breast.right,breast.baseR)||!Rotation(breast.left,breast.baseL)||!Value(WorldRotation,breast.right,worldR)||!Value(WorldRotation,breast.left,worldL)){ClearBreast();breast.status="胸腔方向或骨架讀取失敗";return;}
 if(!breast.chestReferenceReady){if(!Value(Position,breast.head,head)||!Value(Position,breast.shoulderL,left)||!Value(Position,breast.shoulderR,right)||!BreastPhysics::ChestFrame(p,head,left,right,frame)){ClearBreast();breast.status="胸腔方向或骨架讀取失敗";return;}breast.chestReference=PoseMath::Multiply(PoseMath::Inverse(chestWorld),frame);breast.chestReferenceReady=true;}
 frame=PoseMath::Multiply(chestWorld,breast.chestReference);

 if(!breast.leverReady&&Value(Position,breast.right,positionR)&&Value(Position,breast.left,positionL)){auto arm=PoseMath::Rotate(PoseMath::Inverse(frame),{(positionR.x+positionL.x)*.5f-p.x,(positionR.y+positionL.y)*.5f-p.y,(positionR.z+positionL.z)*.5f-p.z});breast.lever={0,std::clamp(arm.y,-.15f,.15f),std::clamp(arm.z,.04f,.2f)};if(std::abs(breast.lever.y)<.01f)breast.lever.y=-.04f;breast.leverReady=true;}
 const auto& profile=BreastDefaults().at(target);float limit=std::max(profile.value("animation_up",0.f),profile.value("animation_down",0.f))*.01745329252f*2.f;
 auto angles=breast.spring.Step(p,frame,breast.lever,dt,profile.value("frequency",2.f),profile.value("damping",.52f),limit,context.paused,context.animated?.55f:0.f);
 breast.writtenR=BreastPhysics::BoneOutput(breast.baseR,worldR,frame,angles);breast.writtenL=BreastPhysics::BoneOutput(breast.baseL,worldL,frame,angles);float angle=BreastPhysics::Length(angles);
 bool wroteRight=WriteRotation(breast.right,breast.writtenR);breast.wrote=wroteRight;if(!wroteRight||!WriteRotation(breast.left,breast.writtenL)){RemoveBreastLayer();ClearBreast();breast.status="胸部物理寫入失敗";return;}
 breast.wrote=true;breast.cached=true;breast.cachedAt=GetTickCount64();breast.outputDegrees=angle*57.29577951f;breast.status="胸部物理已套用："+CharacterNames::Name(target);if(GetTickCount64()-breast.loggedAt>2000){breast.loggedAt=GetTickCount64();Log("Breast physics: "+target+" "+BreastDiagnostics());}
}
