#include "pose_math.h"
#include <cstdio>
#include <limits>
using namespace PoseMath;
int main(){
 int failures=0;
 auto check=[&](bool ok,const char* name){if(!ok){std::fprintf(stderr,"FAIL: %s\n",name);++failures;}};
 Quaternion identity{},offset=Yaw20();
 check(SameRotation(Multiply(identity,offset),offset),"identity -> expected 20 degree yaw");
 Quaternion negative{-offset.x,-offset.y,-offset.z,-offset.w};
 check(SameRotation(offset,negative),"opposite quaternion signs describe same pose");
 Quaternion observed=identity,base=identity,last{};bool wrote=false;
 for(int i=0;i<600;++i){base=AnimationBase(observed,last,base,wrote);last=Multiply(base,offset);Normalize(last);observed=last;wrote=true;}
 check(SameRotation(observed,offset),"600 non-animated frames cannot accumulate yaw");
 check(SameRotation(base,identity),"original base is retained for restore when animator is culled");
 Quaternion fresh{0.05f,0.02f,0.01f,0.9985f};Normalize(fresh);
 auto animation=AnimationBase(fresh,last,base,true);
 check(SameRotation(animation,fresh),"fresh animation wins over stale base");
 check(!SameRotation(fresh,last),"restore leaves a fresh animation pose untouched");
 Quaternion tinyUpdate=last;tinyUpdate.x+=0.0001f;Normalize(tinyUpdate);
 check(SameRotation(AnimationBase(tinyUpdate,last,base,true),tinyUpdate),"small real animation changes are preserved");
 Quaternion invalid{0,0,0,0};check(!Normalize(invalid),"zero quaternion rejected");
 invalid={std::numeric_limits<float>::quiet_NaN(),0,0,1};check(!Normalize(invalid),"NaN rejected");
 invalid={0,0,0,10};check(!Normalize(invalid),"invalid magnitude rejected");
 float yaw=0,pitch=0;
 check(LookAngles({0,0,2},yaw,pitch)&&yaw==0&&pitch==0,"front target is neutral");
 check(LookAngles({2,0,0},yaw,pitch)&&yaw==55,"right target clamps yaw");
 check(LookAngles({-2,0,0},yaw,pitch)&&yaw==-55,"left target clamps yaw");
 check(LookAngles({0,2,1},yaw,pitch)&&pitch==-20,"high target clamps pitch");
 check(!LookAngles({0,0,0},yaw,pitch),"coincident target rejected");
 check(!LookAngles({0,0,21},yaw,pitch),"distant target rejected");
 auto body=YawPitch(90,0);auto local=Rotate(Inverse(body),{2,0,0});
 check(LookAngles(local,yaw,pitch)&&std::abs(yaw)<0.001f,"world right is front of a rotated model");
 check(Smooth(0,55,1.0f/60)>0&&Smooth(0,55,1.0f/60)<55,"smooth turn has no instant jump");
 check(Smooth(10,-55,-1)==10,"invalid delta time cannot jump pose");
 check(std::abs(Smooth(Smooth(0,55,0.02f),55,0.02f)-Smooth(0,55,0.04f))<0.0001f,"smoothing independent of normal frame rate");
 Vector3 anchor{};Quaternion facing{},other{};
 check(StandingAnchors({0,0,0},{2,0,0},anchor,facing,other)&&SamePosition(anchor,{1.2f,0,0}),"standing anchor separation");
 check(SameRotation(facing,YawPitch(90,0))&&SameRotation(other,YawPitch(270,0)),"standing facing directions opposite");
 check(!StandingAnchors({0,0,0},{1,0.5f,0},anchor,facing,other),"different floor heights refused");
 check(!StandingAnchors({0,0,0},{0.1f,0,0},anchor,facing,other),"overlapping pair refused");
 check(!StandingAnchors({0,0,0},{4,0,0},anchor,facing,other),"large displacement refused");
 check(SameRotation(Nlerp({},YawPitch(180,0),1),YawPitch(180,0)),"rotation blend endpoint");
 if(!failures)std::puts("PASS: quaternion direction, 600-frame non-accumulation, live-animation preservation and invalid-input checks");
 return failures?1:0;
}
