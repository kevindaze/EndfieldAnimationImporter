#pragma once
#include <array>
#include <string>
namespace Support {
inline constexpr std::array<const char*,5> Modes{"none","left_foot","right_foot","left_knee","right_knee"};
inline std::string Normalize(std::string mode){return mode=="both_feet"?"none":mode;}
inline bool Valid(const std::string& mode){for(auto value:Modes)if(mode==value)return true;return false;}
inline const char* Label(const std::string& mode){return mode=="left_foot"?"左腳固定":mode=="right_foot"?"右腳固定":mode=="left_knee"?"左膝固定":mode=="right_knee"?"右膝固定":"不固定";}
}
