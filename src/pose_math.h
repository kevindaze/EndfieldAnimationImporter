#pragma once
#include <cmath>
#include <algorithm>
namespace PoseMath {
struct Quaternion { float x=0,y=0,z=0,w=1; };
struct Vector3 {float x=0,y=0,z=0;};
inline bool Finite(Vector3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline bool SamePosition(Vector3 a,Vector3 b){return std::abs(a.x-b.x)<0.00001f&&std::abs(a.y-b.y)<0.00001f&&std::abs(a.z-b.z)<0.00001f;}
inline Vector3 Lerp(Vector3 a,Vector3 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
inline bool Normalize(Quaternion& q) {
 const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
 if(!std::isfinite(n)||n<0.5f||n>1.5f)return false;
 const float s=1/std::sqrt(n);q.x*=s;q.y*=s;q.z*=s;q.w*=s;return true;
}
inline Quaternion Multiply(const Quaternion& a,const Quaternion& b) {
 return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
 a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
 a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
 a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
inline bool SameRotation(const Quaternion& a,const Quaternion& b) {
 // q and -q encode the same orientation; compare components instead of a
 // broad angular tolerance so a genuine animation update is not discarded.
 const float minus=(a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)+(a.w-b.w)*(a.w-b.w);
 const float plus=(a.x+b.x)*(a.x+b.x)+(a.y+b.y)*(a.y+b.y)+(a.z+b.z)*(a.z+b.z)+(a.w+b.w)*(a.w+b.w);
 return minus<1e-10f||plus<1e-10f;
}
inline Quaternion Yaw20() {return {0,0.1736481777f,0,0.9848077530f};}
inline Quaternion Nlerp(Quaternion a,Quaternion b,float t){
 if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
 Quaternion q{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
 const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
 return n>0?Quaternion{q.x/n,q.y/n,q.z/n,q.w/n}:a;
}
inline bool StandingAnchors(Vector3 a,Vector3 b,Vector3& partner,Quaternion& facing,Quaternion& partnerFacing,Quaternion reference={}){
 if(!Finite(a)||!Finite(b)||!Normalize(reference))return false;
 // Placement depends on the controlled actor, never the partner's idle location.
 const float x=2*(reference.x*reference.z+reference.w*reference.y),z=1-2*(reference.x*reference.x+reference.y*reference.y),d=std::hypot(x,z);
 if(!std::isfinite(d)||d<1e-6f)return false;
 partner={a.x+x/d*1.2f,a.y,a.z+z/d*1.2f};
 const float yaw=std::atan2(x,z)*57.29577951f;
 const float angle=yaw*0.00872664626f,other=(yaw+180)*0.00872664626f;
 facing={0,std::sin(angle),0,std::cos(angle)};partnerFacing={0,std::sin(other),0,std::cos(other)};return true;
}
inline Quaternion Inverse(const Quaternion& q){return {-q.x,-q.y,-q.z,q.w};}
inline Quaternion LimitRotation(Quaternion rest,Quaternion desired,float degrees){
 auto delta=Multiply(Inverse(rest),desired);if(delta.w<0)delta={-delta.x,-delta.y,-delta.z,-delta.w};
 float angle=2*std::acos(std::clamp(delta.w,-1.f,1.f)),limit=degrees*.01745329252f;
 if(angle<=limit)return desired;float length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
 if(length<1e-6f)return rest;float factor=std::sin(limit*.5f)/length;
 return Multiply(rest,{delta.x*factor,delta.y*factor,delta.z*factor,std::cos(limit*.5f)});
}
inline Vector3 Rotate(const Quaternion& q,Vector3 v){auto r=Multiply(Multiply(q,{v.x,v.y,v.z,0}),Inverse(q));return {r.x,r.y,r.z};}
inline Quaternion YawPitch(float yaw,float pitch){
 const float y=yaw*0.00872664626f,p=pitch*0.00872664626f;
 return Multiply({0,std::sin(y),0,std::cos(y)},{std::sin(p),0,0,std::cos(p)});
}
inline bool LookAngles(Vector3 local,float& yaw,float& pitch){
 const float square=local.x*local.x+local.y*local.y+local.z*local.z;
 if(!std::isfinite(square)||square<0.04f)return false;
 const float horizontal=std::sqrt(local.x*local.x+local.z*local.z);
 if(horizontal<0.05f)return false;
 yaw=std::clamp(std::atan2(local.x,local.z)*57.29577951f,-55.0f,55.0f);
 pitch=std::clamp(-std::atan2(local.y,horizontal)*57.29577951f,-20.0f,20.0f);
 return true;
}
inline float Smooth(float current,float target,float dt){
 if(!std::isfinite(dt)||dt<=0)return current;
 return current+(target-current)*(1-std::exp(-8.0f*std::min(dt,0.05f)));
}
inline Quaternion AnimationBase(const Quaternion& observed,const Quaternion& lastWritten,const Quaternion& lastBase,bool wrote) {
 return wrote&&SameRotation(observed,lastWritten)?lastBase:observed;
}
}
