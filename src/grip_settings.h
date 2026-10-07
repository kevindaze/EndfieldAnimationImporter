#pragma once
#include <array>
#include <algorithm>
#include <sstream>
#include <locale>
#include <string>
#include <cmath>
namespace Grip {
struct Settings {
 std::array<float,42> values{};
 Settings(){const float curls[]{12,20,12,25,40,25,35,50,30,40,55,30,45,60,35};for(int actor=0;actor<2;++actor)for(int i=0;i<15;++i)values[actor*21+6+i]=curls[i];}
 float Get(int actor,int field)const{return values[actor*21+field];}
 std::string Encode()const{std::ostringstream out;out.imbue(std::locale::classic());for(size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}return out.str();}
 bool Read(const std::string& json){
  if(json.size()>16384)return false;auto key=json.find("\"grip_values\"");if(key==std::string::npos)return false;
  auto colon=json.find(':',key+13);if(colon==std::string::npos)return false;
  auto start=json.find_first_not_of(" \r\n\t",colon+1);if(start==std::string::npos||json[start]!='"')return false;
  auto end=json.find('"',start+1);if(end==std::string::npos)return false;std::istringstream in(json.substr(start+1,end-start-1));in.imbue(std::locale::classic());Settings candidate;
  for(size_t i=0;i<values.size();++i){float value=0;if(!(in>>value)||!std::isfinite(value))return false;int field=int(i%21);float low=field<3?-8.0f:field<6?-60.0f:-20.0f,high=field<3?8.0f:field<6?60.0f:100.0f;if(value<low||value>high)return false;candidate.values[i]=value;if(i+1<values.size()){char comma=0;if(!(in>>comma)||comma!=',')return false;}}
  in>>std::ws;if(!in.eof())return false;*this=candidate;return true;
 }
 void SmoothTo(const Settings& target,float dt){if(!std::isfinite(dt)||dt<=0)return;float t=1-std::exp(-8*std::min(dt,0.05f));for(size_t i=0;i<values.size();++i)values[i]+=(target.values[i]-values[i])*t;}
};
}
