#pragma once
#include "pose_math.h"
namespace FreeCamera {
inline bool ValidHotkey(int key){return (key>=0x70&&key<=0x7B)||(key>='0'&&key<='9')||(key>='A'&&key<='Z'&&key!='W'&&key!='A'&&key!='S'&&key!='D'&&key!='E'&&key!='Q');}
struct View {
 PoseMath::Vector3 position{};float yaw=0,pitch=0;
 bool Capture(PoseMath::Vector3 p,PoseMath::Quaternion q){if(!PoseMath::Finite(p)||!PoseMath::Normalize(q))return false;auto forward=PoseMath::Rotate(q,{0,0,1});position=p;yaw=std::atan2(forward.x,forward.z)*57.29577951f;pitch=-std::asin(std::clamp(forward.y,-1.f,1.f))*57.29577951f;return true;}
 PoseMath::Quaternion Rotation()const{return PoseMath::YawPitch(yaw,pitch);}
 void Step(float dt,float right,float up,float forward,float mouseX,float mouseY,float speedMultiplier=1.f){
  yaw=std::remainder(yaw+mouseX*.15f,360.f);pitch=std::clamp(pitch+mouseY*.15f,-89.f,89.f);
  auto motion=PoseMath::Rotate(Rotation(),{right,0,forward});motion.y+=up;
  float length=std::sqrt(motion.x*motion.x+motion.y*motion.y+motion.z*motion.z);
  if(!std::isfinite(dt)||dt<=0||!std::isfinite(length)||length<1e-6f)return;
  float scale=3.f*speedMultiplier*std::min(dt,.05f)/std::max(1.f,length);position.x+=motion.x*scale;position.y+=motion.y*scale;position.z+=motion.z*scale;
 }
};
}
