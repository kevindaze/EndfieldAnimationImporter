#pragma once
#include "handshake_motion.h"
#include "bone_settings.h"
namespace SceneLayout {
struct Pair {PoseMath::Vector3 mainPosition,partnerPosition;PoseMath::Quaternion mainRotation,partnerRotation;};
// Stored root XYZ is centimeters in the manager's reference frame. Derive
// from immutable anchors every tick, never from yesterday's written pose.
inline Pair Build(PoseMath::Vector3 origin,PoseMath::Quaternion reference,
                  PoseMath::Vector3 partner,PoseMath::Quaternion partnerFacing,
                  const Bones::Values& main,const Bones::Values& passive){
 auto relative=PoseMath::Rotate(PoseMath::Inverse(reference),Handshake::Sub(partner,origin));
 auto relativeFacing=PoseMath::Multiply(PoseMath::Inverse(reference),partnerFacing);
 Pair result;result.mainPosition=Handshake::Add(origin,PoseMath::Rotate(reference,{main[0]*.01f,main[1]*.01f,main[2]*.01f}));
 result.mainRotation=PoseMath::Multiply(reference,Handshake::XYZ(main[3],main[4],main[5]));
 relative=Handshake::Add(relative,{passive[0]*.01f,passive[1]*.01f,passive[2]*.01f});
 result.partnerPosition=Handshake::Add(result.mainPosition,PoseMath::Rotate(result.mainRotation,relative));
 // Partner follows manager root Y while retaining its own vertical calibration.
 result.partnerRotation=PoseMath::Multiply(PoseMath::Multiply(result.mainRotation,relativeFacing),Handshake::XYZ(passive[3],passive[4],passive[5]));
 return result;
}
}
