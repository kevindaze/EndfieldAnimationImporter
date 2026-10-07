#include "handshake_motion.h"
#include "grip_settings.h"
#include <iostream>
int main(int argc,char** argv){
 if(argc!=2)return 1;std::filesystem::path p(argv[1]);Handshake::Clip a,b;
 if(!a.Load(p/"18.asf",p/"18_01.amc")||!b.Load(p/"19.asf",p/"19_01.amc")||a.frames.size()!=303||b.frames.size()!=303)return 2;
 auto q=Handshake::Aim({1,0,0},{0,1,0});auto v=PoseMath::Rotate(q,{1,0,0});if(std::abs(v.y-1)>1e-5f)return 3;
 q=Handshake::Aim({1,0,0},{-1,0,0});v=PoseMath::Rotate(q,{1,0,0});if(std::abs(v.x+1)>1e-5f)return 4;
 for(auto* clip:{&a,&b}){float travel=0;for(size_t i=0;i<clip->frames.size();++i){auto f=clip->Sample(float(i)/120);if(!PoseMath::Finite(f.upper)||!PoseMath::Finite(f.fore))return 5;auto& base=clip->frames[0];travel+=std::abs(f.upper.x-base.upper.x)+std::abs(f.fore.z-base.fore.z);}if(travel<1)return 6;}
 Handshake::Clip bad;if(bad.Load(p/"missing.asf",p/"18_01.amc"))return 7;
 PoseMath::Vector3 elbow{},contact{};
 if(!Handshake::Elbow({0,0,0},{0,0,0.5f},0.3f,0.3f,{0,-1,0},elbow))return 8;
 if(std::abs(Handshake::Length(elbow)-0.3f)>1e-5f||std::abs(Handshake::Length(Handshake::Sub(elbow,{0,0,0.5f}))-0.3f)>1e-5f||elbow.y>=0)return 9;
 if(Handshake::Elbow({0,0,0},{0,0,1},0.3f,0.3f,{0,-1,0},elbow))return 10;
 if(!Handshake::Contact({0,1.3f,0},{0.85f,1.3f,0},0.65f,0.65f,contact)||Handshake::Length(Handshake::Sub(contact,{0,1.3f,0}))>=0.65f||Handshake::Length(Handshake::Sub(contact,{0.85f,1.3f,0}))>=0.65f)return 11;
 if(Handshake::Contact({0,0,0},{2,0,0},0.6f,0.6f,contact))return 12;
 auto palm=Handshake::PalmRotation({}, {0,0,1},{1,0,0},{0,0,1},{-1,0,0});
 auto facing=PoseMath::Rotate(palm,{0,0,1}),normal=PoseMath::Rotate(palm,{1,0,0});
 if(std::abs(facing.z-1)>1e-5f||std::abs(normal.x+1)>1e-5f)return 13;
 auto curlAxis=Handshake::Cross({1,0,0},{0,0,1});auto bent=PoseMath::Rotate(Handshake::AngleAxis(curlAxis,35),{1,0,0});
 if(bent.z<=0||std::abs(Handshake::Length(bent)-1)>1e-5f)return 14;
 Grip::Settings settings,copy;
 settings.values[0]=2.5f;settings.values[25]=-30;settings.values[41]=75;
 if(!copy.Read("{\"grip_values\":\""+settings.Encode()+"\"}")||copy.values!=settings.values)return 15;
 auto before=copy.values;if(copy.Read("{\"grip_values\":\"1,2\"}")||copy.values!=before)return 16;
 settings.values[0]=9;if(copy.Read("{\"grip_values\":\""+settings.Encode()+"\"}")||copy.values!=before)return 17;
 Grip::Settings target;target.values[0]=4;Grip::Settings smooth;
 for(int i=0;i<300;++i)smooth.SmoothTo(target,1.0f/60);
 if(std::abs(smooth.values[0]-4)>0.001f)return 18;
 std::cout<<"Paired 303-frame clips, finite changing directions, aiming and missing asset rejection verified\n";
}
