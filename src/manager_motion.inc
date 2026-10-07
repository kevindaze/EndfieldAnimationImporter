// Partner FBX is evaluated first; this layer then follows its current waist.
bool BindManagerMotion(Pose& candidate){
 auto action=candidate.clip->managerAction;if(action=="none")return true;
 auto& pins=*candidate.pins;std::vector<Pose::FullBone> mainBones,targetBones;
 if(!WalkBones(candidate.root,0,pins,mainBones)||!WalkBones(candidate.reverse.root,1,pins,targetBones))return false;
 auto find=[&](const auto& bones,const std::string& name)->void*{void* result=nullptr;for(auto& b:bones)if(b.info.name==name){if(result)return nullptr;result=b.bone;}return result;};
 if(action=="waist_hold"||action=="front_waist"||action=="back_waist"||action=="head_pat"){
  candidate.waist=find(targetBones,"Bip001_Pelvis");candidate.waistSpine=find(targetBones,"Bip001_Spine");if(!candidate.waist){Event("waist_bone_missing");return false;}
  PoseMath::Quaternion bodyWorld{},targetWorld{};
  if(!Value(WorldRotation,candidate.waist,bodyWorld)||!Value(WorldRotation,candidate.reverse.root,targetWorld))return false;
  candidate.managerBodyRight=PoseMath::Rotate(PoseMath::Inverse(bodyWorld),PoseMath::Rotate(targetWorld,{1,0,0}));
  candidate.managerBodyForward=PoseMath::Rotate(PoseMath::Inverse(bodyWorld),PoseMath::Rotate(targetWorld,{0,0,1}));
  for(int sideIndex=0;sideIndex<(action=="waist_hold"?1:2);++sideIndex){
  std::string prefix=sideIndex?"Bip001_L_":"Bip001_R_";
  std::string names[]{prefix+"UpperArm",prefix+"Forearm",prefix+"Hand",prefix+"Finger2"};void* chain[4]{};
  for(int i=0;i<4;++i){chain[i]=find(mainBones,names[i]);if(!chain[i]){Event("manager_arm_missing");return false;}}
  for(int i=0;i<3;++i){if(Call(Parent,chain[i+1])!=chain[i])return false;ArmBone arm;arm.bone=chain[i];arm.child=chain[i+1];arm.root=candidate.root;PoseMath::Vector3 from{},to{};PoseMath::Quaternion world{},root{};
   if(!Rotation(arm.bone,arm.restLocal)||!Value(Position,arm.bone,from)||!Value(Position,arm.child,to)||!Value(WorldRotation,arm.bone,world)||!Value(WorldRotation,arm.root,root))return false;
   auto direction=Handshake::Sub(to,from);arm.length=Handshake::Length(direction);if(arm.length<.01f||arm.length>.8f||!Handshake::Unit(direction))return false;arm.axis=PoseMath::Rotate(PoseMath::Inverse(world),direction);arm.reference=PoseMath::Multiply(PoseMath::Inverse(root),world);auto plane=Handshake::Cross(direction,PoseMath::Rotate(root,{0,-1,-.35f}));if(!Handshake::Unit(plane))return false;arm.planeNormal=PoseMath::Rotate(PoseMath::Inverse(world),plane);candidate.arms.push_back(arm);
  }
  PoseMath::Vector3 wrist{},middle{},index{},little{};auto indexBone=find(mainBones,prefix+"Finger1"),littleBone=find(mainBones,prefix+"Finger4");PoseMath::Quaternion handWorld{};
  if(indexBone&&littleBone&&Value(Position,chain[2],wrist)&&Value(Position,chain[3],middle)&&Value(Position,indexBone,index)&&Value(Position,littleBone,little)&&Value(WorldRotation,chain[2],handWorld)){
   auto normal=Handshake::Cross(Handshake::Sub(middle,wrist),Handshake::Sub(index,little));if(sideIndex)normal=Handshake::Scale(normal,-1.f);if(Handshake::Unit(normal))candidate.palmNormal[sideIndex]=PoseMath::Rotate(PoseMath::Inverse(handWorld),normal);
  }
  }
  if(action=="head_pat"){candidate.waist=find(targetBones,"Bip001_Head");candidate.waistSpine=nullptr;if(!candidate.waist)return false;
   if(!Value(WorldRotation,candidate.waist,bodyWorld))return false;
   candidate.managerBodyRight=PoseMath::Rotate(PoseMath::Inverse(bodyWorld),PoseMath::Rotate(targetWorld,{1,0,0}));candidate.managerBodyForward=PoseMath::Rotate(PoseMath::Inverse(bodyWorld),PoseMath::Rotate(targetWorld,{0,0,1}));
  }
  auto& a=candidate.placement;auto& b=candidate.partnerPlacement;
  a.anchorRotation=a.startRotation;b.anchorRotation=a.startRotation;
  b.anchor=Handshake::Add(a.anchor,PoseMath::Rotate(a.startRotation,action=="waist_hold"?PoseMath::Vector3{.42f,0,.10f}:PoseMath::Vector3{0,0,.48f}));
  if(action=="front_waist"||action=="head_pat")b.anchorRotation=PoseMath::Multiply(a.startRotation,PoseMath::YawPitch(180,0));
 }
 // Capture the existing standing pose; retain partner animation untouched.
 for(auto& item:mainBones){if(!item.info.name.starts_with("Bip001"))continue;
  if(std::any_of(candidate.customBones.begin(),candidate.customBones.end(),[&](const auto& existing){return existing.bone==item.bone;}))continue;
  if(std::any_of(candidate.arms.begin(),candidate.arms.end(),[&](const auto& arm){return arm.bone==item.bone;}))continue;
  Pose::CustomBone bone;bone.bone=item.bone;bone.root=candidate.root;bone.actor=0;bone.track=candidate.clip->tracks.size();if(!Rotation(bone.bone,bone.rest))return false;bone.base=bone.rest;candidate.customBones.push_back(bone);
 }
 return true;
}
bool ApplyManagerMotion(float dt,float blend){
 if(!pose.clip||pose.arms.empty())return true;
 if((pose.arms.size()!=3&&pose.arms.size()!=6)||!Living(pose.waist))return false;
 for(size_t armIndex=0;armIndex<pose.arms.size()/3;++armIndex){auto arms=pose.arms.data()+armIndex*3;
 PoseMath::Vector3 waist{},shoulder{};PoseMath::Quaternion root{},partnerRoot{};
 if(!Value(Position,pose.waist,waist)||!Value(Position,arms[0].bone,shoulder)||!Value(WorldRotation,pose.root,root)||!Value(WorldRotation,pose.reverse.root,partnerRoot))return false;
 auto up=PoseMath::Rotate(partnerRoot,{0,1,0});PoseMath::Vector3 spine{};
 if(pose.waistSpine&&Living(pose.waistSpine)&&Value(Position,pose.waistSpine,spine)){auto direction=Handshake::Sub(spine,waist);if(Handshake::Unit(direction))up=direction;}
 PoseMath::Quaternion bodyRotation{};if(!Value(WorldRotation,pose.waist,bodyRotation))return false;
 auto rightHint=PoseMath::Rotate(bodyRotation,pose.managerBodyRight),forwardHint=PoseMath::Rotate(bodyRotation,pose.managerBodyForward);
 if(pose.clip->managerAction=="head_pat")up=Handshake::Cross(forwardHint,rightHint);
 Handshake::BodyFrame frame{};if(!Handshake::ContactFrame(up,rightHint,forwardHint,frame))return false;up=frame.up;
 auto side=Handshake::Sub(shoulder,waist);side=Handshake::Sub(side,Handshake::Scale(up,Handshake::Dot(side,up)));if(!Handshake::Unit(side))side=PoseMath::Rotate(partnerRoot,{-1,0,0});
 auto fresh=Handshake::Add(waist,Handshake::Add(Handshake::Scale(side,.12f),Handshake::Scale(up,.12f)));
 if(pose.clip->managerAction!="waist_hold"){
  auto right=frame.right,facing=frame.forward;
  float lateral=armIndex?-.13f:.13f;if(pose.clip->managerAction=="front_waist"||pose.clip->managerAction=="head_pat")lateral=-lateral;
  if(pose.clip->managerAction=="head_pat"){fresh=Handshake::Add(waist,Handshake::Add(Handshake::Scale(right,lateral*.65f),Handshake::Scale(up,.13f)));side=up;}
  else {float surface=pose.clip->managerAction=="front_waist"?.10f:-.10f;fresh=Handshake::Add(waist,Handshake::Add(Handshake::Scale(right,lateral),Handshake::Add(Handshake::Scale(up,.12f),Handshake::Scale(facing,surface))));side=Handshake::Scale(facing,surface>0?1.f:-1.f);}
 }
 if(!pose.managerContactReady[armIndex]){pose.managerContact[armIndex]=fresh;pose.managerContactReady[armIndex]=true;}else pose.managerContact[armIndex]=PoseMath::Lerp(pose.managerContact[armIndex],fresh,1-std::exp(-14*std::clamp(dt,0.f,.05f)));
 auto& hand=arms[2];auto reference=PoseMath::Multiply(root,hand.reference);auto forward=pose.clip->managerAction=="head_pat"?frame.forward:Handshake::Cross(up,side),normal=Handshake::Scale(side,-1.f);if(!Handshake::Unit(forward))return false;
 if(pose.managerTangent[armIndex]==0)pose.managerTangent[armIndex]=Handshake::Dot(forward,PoseMath::Rotate(reference,hand.axis))<0?-1.f:1.f;forward=Handshake::Scale(forward,pose.managerTangent[armIndex]);
 auto desiredHand=Handshake::PalmRotation(reference,hand.axis,pose.palmNormal[armIndex],forward,normal);
 auto palmOffset=PoseMath::Rotate(desiredHand,Handshake::Scale(hand.axis,hand.length*.5f));
 auto wrist=Handshake::Sub(pose.managerContact[armIndex],palmOffset),direction=Handshake::Sub(wrist,shoulder);float distance=Handshake::Length(direction);
 if(!Handshake::Unit(direction))return false;
 float reach=arms[0].length+arms[1].length;
 // Large partner motion may leave reach: keep the arm stable, never stretch it.
 distance=std::clamp(distance,std::abs(arms[0].length-arms[1].length)+.015f,reach-.015f);wrist=Handshake::Add(shoulder,Handshake::Scale(direction,distance));
 auto armReference=PoseMath::Multiply(root,arms[0].reference);auto preferred=PoseMath::Rotate(armReference,arms[0].planeNormal),previous=PoseMath::Rotate(root,pose.managerBend[armIndex]);PoseMath::Vector3 bendPlane{};
 if(!Handshake::StablePlane(direction,preferred,previous,bendPlane))return false;pose.managerBend[armIndex]=PoseMath::Rotate(PoseMath::Inverse(root),bendPlane);auto pole=Handshake::Cross(bendPlane,direction);
 PoseMath::Vector3 elbow{};if(!Handshake::Elbow(shoulder,wrist,arms[0].length,arms[1].length,pole,elbow))return false;
 PoseMath::Quaternion desired[3]{};desired[2]=desiredHand;PoseMath::Vector3 directions[]{Handshake::Sub(elbow,shoulder),Handshake::Sub(wrist,elbow)};
 for(int i=0;i<2;++i){auto& arm=arms[i];auto ref=PoseMath::Multiply(root,arm.reference);desired[i]=Handshake::PalmRotation(ref,arm.axis,arm.planeNormal,directions[i],bendPlane);}
 for(int i=0;i<3;++i){auto& arm=arms[i];PoseMath::Quaternion current{},parent{};auto p=Call(Parent,arm.bone);if(!Living(arm.bone)||!leases->owns(arm.root,id,arm.lease)||!Rotation(arm.bone,current)||!p||!Value(WorldRotation,p,parent))return false;
  arm.base=PoseMath::AnimationBase(current,arm.written,arm.base,arm.wrote);auto local=PoseMath::Multiply(PoseMath::Inverse(parent),desired[i]);if(i==2)local=Handshake::WristLimit(arm.restLocal,local,arm.axis);auto q=PoseMath::Nlerp(arm.restLocal,local,blend);if(arm.wrote)q=PoseMath::LimitRotation(arm.written,q,240.f*std::clamp(dt,0.f,.05f));if(!WriteRotation(arm.bone,q))return false;arm.written=q;arm.wrote=true;
 }
 }
 return true;
}

