// Optional skinning branches verified by name AND parent hierarchy. These
// joints are not a reason to swap left and right animation tracks.
bool BindArmTwists(Pose& candidate){
 if(!candidate.clip||!candidate.clip->absolute)return true;auto& pins=*candidate.pins;
 for(int actor=0;actor<2;++actor)for(const char* side:{"L","R"}){
  auto find=[&](const std::string& name)->void*{void* found=nullptr;for(auto& b:candidate.fullBones)if(b.info.actor==actor&&b.info.name==name){if(found)return nullptr;found=b.bone;}return found;};
  auto upper=find("Bip001_"+std::string(side)+"_UpperArm"),fore=find("Bip001_"+std::string(side)+"_Forearm");
  if(!upper||!fore)continue;PoseMath::Vector3 from{},to{};PoseMath::Quaternion upperWorld{},upperRest{};
  if(!Value(Position,upper,from)||!Value(Position,fore,to)||!Value(WorldRotation,upper,upperWorld)||!Rotation(upper,upperRest))continue;
  auto axis=PoseMath::Rotate(PoseMath::Inverse(upperWorld),Handshake::Sub(to,from));if(!Handshake::Unit(axis))continue;
  void* previous=upper;
  for(int joint=0;joint<2;++joint){auto name="Bip001_"+std::string(side)+"UpArmTwist"+(joint?"1":"");auto bone=find(name);
   if(!bone||Call(Parent,bone)!=previous)break;previous=bone;
   if(std::any_of(candidate.clip->tracks.begin(),candidate.clip->tracks.end(),[&](auto& track){return track.actor==actor&&track.bone==name;}))continue;
   PoseMath::Quaternion world{},local{};if(!Value(WorldRotation,bone,world)||!Rotation(bone,local))continue;
   FingerControl control;control.bone=bone;control.root=actor?candidate.reverse.root:candidate.root;control.rest=control.base=local;
   Pose::Twist driver;driver.upper=upper;driver.finger=candidate.fingers.size();driver.axis=axis;driver.upperRest=upperRest;driver.offset=PoseMath::Multiply(PoseMath::Inverse(upperWorld),world);driver.weight=1.f;
   candidate.fingers.push_back(control);candidate.twists.push_back(driver);
   std::erase_if(candidate.customBones,[&](auto& b){return b.bone==bone;});
   Log("Arm twist actor="+std::to_string(actor)+" bone="+name+" weight="+std::to_string(driver.weight));
  }
 }
 return true;
}
bool ApplyArmTwists(){
 for(auto& driver:pose.twists){auto& control=pose.fingers[driver.finger];PoseMath::Quaternion upperLocal{},upperWorld{},current{},parentWorld{};auto parent=Call(Parent,control.bone);
  if(!Living(control.bone)||!Living(driver.upper)||!leases->owns(control.root,id,control.lease)||!parent||!Rotation(driver.upper,upperLocal)||!Value(WorldRotation,driver.upper,upperWorld)||!Rotation(control.bone,current)||!Value(WorldRotation,parent,parentWorld))return false;
  auto delta=PoseMath::Multiply(PoseMath::Inverse(driver.upperRest),upperLocal),counter=Handshake::TwistFraction(delta,driver.axis,driver.weight-1.f);
  auto desiredWorld=PoseMath::Multiply(PoseMath::Multiply(upperWorld,counter),driver.offset);
  auto desired=PoseMath::Multiply(PoseMath::Inverse(parentWorld),desiredWorld);control.base=PoseMath::AnimationBase(current,control.written,control.base,control.wrote);
  if(!WriteRotation(control.bone,desired))return false;control.written=desired;control.wrote=true;
 }
 return true;
}
