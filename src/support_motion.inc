bool BindSupport(Pose& candidate){
 auto& s=candidate.support;s=SupportState{};s.mode=supportTarget;
 if(candidate.mode!=Mode::Custom||s.mode=="none")return true;
 auto find=[&](const std::string& name)->void*{void* found=nullptr;for(auto& b:candidate.fullBones)if(b.info.actor==1&&b.info.name==name){if(found)return nullptr;found=b.bone;}return found;};
 bool knee=s.mode=="left_knee"||s.mode=="right_knee";
 s.count=1;
 PoseMath::Vector3 rootPosition{};PoseMath::Quaternion rootRotation{};
 if(!Value(Position,candidate.reverse.root,rootPosition)||!Value(WorldRotation,candidate.reverse.root,rootRotation)||!Value(LocalScale,candidate.reverse.root,s.captureScale))return false;
 for(int i=0;i<s.count;++i){auto& leg=s.legs[i];auto side=s.mode=="right_foot"||s.mode=="right_knee"||i==1?"R":"L";std::string prefix=std::string("Bip001_")+side+"_";
  leg.point=find(prefix+(knee?"Calf":"Foot"));if(!leg.point){Event("support_bone_missing");return false;}
  if(!knee){PoseMath::Vector3 position{};
   if(!Value(Position,leg.point,position))return false;
   leg.anchor=PoseMath::Rotate(PoseMath::Inverse(rootRotation),Handshake::Sub(position,rootPosition));
  }

 }
 s.ready=!knee;return true;
}
bool ApplySupport(){
 if(!pose.pins||pose.mode!=Mode::Custom)return true;
 auto& s=pose.support;if(s.mode=="none")return true;
 if(!leases->owns(pose.reverse.root,id,pose.reverse.lease))return false;
 auto scene=SceneLayout::Build(pose.placement.anchor,pose.placement.anchorRotation,pose.partnerPlacement.anchor,pose.partnerPlacement.anchorRotation,boneTarget.Get("0|@root"),boneTarget.Get("1|@root"));
 // Passive root rotations pivot about the support, while manager rotation
 // still carries the entire scene. Root XYZ moves the support deliberately.
 auto supportFacing=PoseMath::Multiply(scene.mainRotation,PoseMath::Multiply(PoseMath::Inverse(pose.placement.anchorRotation),pose.partnerPlacement.anchorRotation));
 PoseMath::Vector3 scale{};if(!Value(LocalScale,pose.reverse.root,scale)||s.captureScale.x<=0||s.captureScale.y<=0||s.captureScale.z<=0)return false;
 // Knee modes pivot around the first evaluated pose. Foot modes use the
 // standing contact captured before clip sampling, preserving ankle height.
 if(!s.ready){for(int i=0;i<s.count;++i){PoseMath::Vector3 point{};if(!Value(Position,s.legs[i].point,point))return false;s.legs[i].anchor=PoseMath::Rotate(PoseMath::Inverse(supportFacing),Handshake::Sub(point,scene.partnerPosition));}s.captureScale=scale;s.ready=true;}
 PoseMath::Vector3 targets[2]{},shift{};
 for(int i=0;i<s.count;++i){auto& leg=s.legs[i];PoseMath::Vector3 actual{};
  if(!Living(leg.point)||!Value(Position,leg.point,actual))return false;
  auto local=leg.anchor;local={local.x*scale.x/s.captureScale.x,local.y*scale.y/s.captureScale.y,local.z*scale.z/s.captureScale.z};
  targets[i]=Handshake::Add(scene.partnerPosition,PoseMath::Rotate(supportFacing,local));
  shift=Handshake::Add(shift,Handshake::Sub(targets[i],actual));
 }
 shift=Handshake::Scale(shift,1.f/s.count);PoseMath::Vector3 root{};
 if(!PoseMath::Finite(shift)||!Value(Position,pose.reverse.root,root)||!WriteValue(SetPosition,pose.reverse.root,Handshake::Add(root,shift)))return false;
 // Keep placement restoration and next-frame baseline tracking aware of the
 // compensated write; never treat our support offset as a new game baseline.
 if(!Value(LocalPosition,pose.reverse.root,pose.partnerPlacement.writtenLocal))return false;s.offset=shift;
 return true;
}
