#pragma once
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iomanip>
#include <locale>
#include <cctype>
#include <array>
#include <cstdint>
namespace EfmiBridge {
inline std::string Utf8(const std::filesystem::path& p){auto s=p.u8string();return {s.begin(),s.end()};}
inline std::string Trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");return a==s.npos?"":s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
inline std::string Lower(std::string s){for(auto& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
inline bool Variable(const std::string& s){return std::regex_match(s,std::regex(R"(\$[A-Za-z_][A-Za-z_0-9]*)"));}
inline bool Uses(const std::string& text,const std::string& variable){auto at=text.find(variable);while(at!=text.npos){auto end=at+variable.size();if(end==text.size()||!(std::isalnum(static_cast<unsigned char>(text[end]))||text[end]=='_'))return true;at=text.find(variable,at+1);}return false;}
inline std::string MeshLabel(std::string name){auto dot=name.find('.');if(dot!=name.npos&&name.substr(0,dot).find('-')!=name.npos)name=name.substr(dot+1);if(name.ends_with("_copy"))name.resize(name.size()-5);return name;}
struct Control {std::string name,binding;float low=0,high=1,value=0;bool integer=false,dirty=false;};
struct Resource {std::string name,file,format;int stride=0;};
struct Mesh {std::string name,condition,section,resource,indexResource;uint64_t count=0,start=0;int64_t base=0;std::array<float,12> values{0,0,0,0,0,0,1,1,1,0,0,0};bool supported=false,dirty=false;std::string reason;};
struct Mod {std::filesystem::path file;std::string name,space;std::vector<Control> controls;std::vector<Mesh> meshes;std::map<std::string,Resource> resources;};
struct Scan {std::filesystem::path folder,efmiRoot;std::string reloadKey;std::vector<Mod> mods;std::vector<std::string> errors;};
inline std::string ReadFile(const std::filesystem::path& file){if(std::filesystem::file_size(file)>8388608)throw std::runtime_error("INI exceeds 8 MB");std::ifstream f(file,std::ios::binary);if(!f)throw std::runtime_error("INI unreadable");return {std::istreambuf_iterator<char>(f),{}};}
inline Mod Parse(const std::filesystem::path& file,const std::string& content,const std::filesystem::path& root){Mod mod;mod.file=file;mod.name=Utf8(file.parent_path().filename());std::istringstream stream(content);std::string line,section,comment,resource,indexResource;int pendingMesh=-1;std::vector<std::string> conditions;std::map<std::string,std::set<float>> numbers;std::set<std::string> meshes,continuous;
 std::regex declaration(R"(^global\s+persist\s+(\$[A-Za-z_][A-Za-z_0-9]*)\s*=\s*([-+0-9.eE]+)\s*$)",std::regex::icase),meshName(R"(\[mesh:([^\]]+)\])"),drawName(R"(^;\s*Draw\s+Component\s+(\d+)(?:\s+(.+))?$)",std::regex::icase),numberUse(R"((\$[A-Za-z_][A-Za-z_0-9]*)\s*(?:==|>=|<=|>|<|=)\s*([-+0-9.eE]+))");
 while(std::getline(stream,line)){line=Trim(line);if(line.starts_with("\xef\xbb\xbf"))line.erase(0,3);std::smatch match;
 if(Lower(line).starts_with("namespace")){auto pos=line.find('=');if(pos!=line.npos)mod.space=Trim(line.substr(pos+1));}
 if(line.starts_with("[")){section=line;conditions.clear();resource.clear();indexResource.clear();pendingMesh=-1;comment.clear();if(section.starts_with("[Resource")){auto name=section.substr(1,section.size()-2);mod.resources[name].name=name;}continue;}
 if(line.starts_with(";")){std::string name;if(std::regex_search(line,match,meshName))name=match[1];else if(std::regex_match(line,match,drawName))name="Component "+match[1].str()+(match[2].matched?" / "+Trim(match[2].str()):"");if(!name.empty()){if(meshes.insert(name).second){std::string condition;for(auto& c:conditions){if(!condition.empty())condition+=" && ";condition+=c;}mod.meshes.push_back({name,condition,section,resource,indexResource});pendingMesh=int(mod.meshes.size())-1;}}else comment=Trim(line.substr(1));continue;}
 if(line.find("+")!=line.npos||line.find("*")!=line.npos||line.find("/")!=line.npos){std::smatch target;if(std::regex_search(line,target,std::regex(R"(^\s*(\$[A-Za-z_][A-Za-z_0-9]*)\s*=)")))continuous.insert(target[1]);}
 auto semi=line.find(';');if(semi!=line.npos)line=Trim(line.substr(0,semi));
 if(line.starts_with("if "))conditions.push_back(line.substr(3));else if(line.starts_with("elif ")){if(!conditions.empty())conditions.pop_back();conditions.push_back(line.substr(5));}else if(line=="else"){if(!conditions.empty())conditions.back()="else ("+conditions.back()+")";}else if(line=="endif"&&!conditions.empty())conditions.pop_back();
 if(section.starts_with("[Resource")){auto split=line.find('=');if(split!=line.npos){auto key=Lower(Trim(line.substr(0,split))),value=Trim(line.substr(split+1));auto& r=mod.resources[section.substr(1,section.size()-2)];if(key=="filename")r.file=value;if(key=="format")r.format=value;if(key=="stride")try{r.stride=std::stoi(value);}catch(...){}}}
 if(line.starts_with("ib ")||line.starts_with("ib=")){auto split=line.find('=');if(split!=line.npos)indexResource=Trim(line.substr(split+1));if(Lower(indexResource).starts_with("ref "))indexResource=Trim(indexResource.substr(4));}
 if(pendingMesh>=0&&(line.starts_with("drawindexedinstanced")||line.starts_with("drawindexed"))){auto split=line.find('=');if(split!=line.npos){std::vector<std::string> args;std::istringstream in(line.substr(split+1));std::string value;while(std::getline(in,value,','))args.push_back(Trim(value));bool instanced=line.starts_with("drawindexedinstanced");try{auto& m=mod.meshes[pendingMesh];m.count=std::stoull(args.at(0));m.start=std::stoull(args.at(instanced?2:1));m.base=std::stoll(args.at(instanced?3:2));}catch(...){}}pendingMesh=-1;}
 if(line.starts_with("vb0")){auto pos=line.find('=');if(pos!=line.npos)resource=Trim(line.substr(pos+1));if(Lower(resource).starts_with("ref "))resource=Trim(resource.substr(4));}
 if(std::regex_match(line,match,declaration)){try{float value=std::stof(match[2]);if(!std::isfinite(value))continue;std::string binding=match[1];auto lower=Lower(binding);if(lower.starts_with("$mfwide_")&&!lower.starts_with("$mfwide_wing_"))continue;if(lower.find("version")!=lower.npos||lower.find("first_run")!=lower.npos)continue;Control c;c.binding=binding;c.name=comment.empty()?binding:comment;c.value=value;mod.controls.push_back(c);numbers[binding].insert(value);}catch(...){}}
 for(auto it=std::sregex_iterator(line.begin(),line.end(),numberUse);it!=std::sregex_iterator();++it){try{float value=std::stof((*it)[2]);if(std::isfinite(value))numbers[(*it)[1]].insert(value);}catch(...){}}
 }
 for(auto& c:mod.controls){auto& n=numbers[c.binding];c.low=*n.begin();c.high=*n.rbegin();if(c.low==c.high){c.low=std::min(0.f,c.value);c.high=std::max(1.f,c.value);}c.integer=!continuous.contains(c.binding)&&std::all_of(n.begin(),n.end(),[](float v){return std::floor(v)==v;});}
 if(mod.space.empty()&&!root.empty()){std::error_code ec;auto relative=std::filesystem::relative(file,root,ec);if(!ec&&!relative.empty()&&!Utf8(relative).starts_with("..")){mod.space=Utf8(relative);std::replace(mod.space.begin(),mod.space.end(),'/','\\');}}
 // Missing exported mesh names: retain resource sections as unidentified components.
 if(mod.meshes.empty()){std::istringstream in(content);while(std::getline(in,line)){line=Trim(line);if(line.starts_with("[Resource")&&line.find("Position")!=line.npos)mod.meshes.push_back({line,"","",line});}}
 return mod;}
inline Scan Discover(const std::filesystem::path& requested){Scan result;result.folder=std::filesystem::weakly_canonical(requested);if(!std::filesystem::is_directory(result.folder))throw std::runtime_error("Selected folder does not exist");for(auto p=result.folder;!p.empty();){if(std::filesystem::is_regular_file(p/"d3dx.ini")){result.efmiRoot=p;auto text=ReadFile(p/"d3dx.ini");std::smatch m;if(std::regex_search(text,m,std::regex(R"((?:^|\n)\s*reload_config\s*=\s*([^\r\n;]+))")))result.reloadKey=Trim(m[1]);break;}auto parent=p.parent_path();if(parent==p)break;p=parent;}
 std::error_code ec;int files=0;std::vector<std::filesystem::path> paths;
 for(auto it=std::filesystem::recursive_directory_iterator(result.folder,std::filesystem::directory_options::skip_permission_denied,ec);it!=std::filesystem::recursive_directory_iterator();it.increment(ec)){if(ec){result.errors.push_back(ec.message());ec.clear();continue;}if(it.depth()>12||it->is_symlink()||(Lower(Utf8(it->path().filename())).starts_with("disabled")||Lower(Utf8(it->path().filename())).starts_with("bem.generated"))){if(it->is_directory())it.disable_recursion_pending();continue;}if(++files>12000)throw std::runtime_error("Too many files; select a smaller mod folder");if(Lower(Utf8(it->path().extension()))==".ini"&&!Lower(Utf8(it->path().filename())).starts_with("bem.external"))paths.push_back(it->path());}
 std::sort(paths.begin(),paths.end());for(auto& file:paths){try{auto mod=Parse(file,ReadFile(file),result.efmiRoot);if(!mod.controls.empty()||!mod.meshes.empty())result.mods.push_back(std::move(mod));}catch(const std::exception& e){result.errors.push_back(Utf8(file.filename())+": "+e.what());}}
 return result;}
inline std::filesystem::path OutputPath(const Scan& scan){if(scan.efmiRoot.empty())throw std::runtime_error("Cannot locate EFMI d3dx.ini");auto root=scan.efmiRoot/"Mods";auto path=scan.folder/"BEM.ExternalControls.ini";auto relative=path.lexically_relative(root);if(relative.empty()||Utf8(relative).starts_with(".."))throw std::runtime_error("Bridge must be inside EFMI Mods");return path;}
inline constexpr char Marker[]="; BEM EXTERNAL CONTROLS v1";
inline std::string Generate(const Scan& scan){std::ostringstream out;out.imbue(std::locale::classic());out<<Marker<<"\n; Generated by BEM. Original mod files are unchanged.\n; Reload EFMI configuration to apply.\nnamespace = BEMExternalControls\n\n[Constants]\n";int count=0;
 for(auto& mod:scan.mods){if(mod.space.empty()||mod.space.find_first_of("\r\n[];=$")!=mod.space.npos)throw std::runtime_error("Invalid EFMI namespace");auto fresh=Parse(mod.file,ReadFile(mod.file),scan.efmiRoot);if(fresh.space!=mod.space)throw std::runtime_error("Mod namespace changed; scan again");for(auto& c:mod.controls)if(c.dirty){if(!Variable(c.binding)||!std::isfinite(c.value)||c.value<c.low||c.value>c.high||(c.integer&&std::floor(c.value)!=c.value))throw std::runtime_error("Invalid external parameter");if(std::none_of(fresh.controls.begin(),fresh.controls.end(),[&](auto& f){return f.binding==c.binding;}))throw std::runtime_error("Mod parameter changed; scan again");out<<"post $\\"<<mod.space<<"\\"<<c.binding.substr(1)<<" = "<<std::setprecision(9)<<c.value<<"\n";++count;}}
 if(!count)throw std::runtime_error("Adjust a parameter before writing bridge");return out.str();}
inline void Owned(const std::filesystem::path& path){if(std::filesystem::exists(path)&&!ReadFile(path).starts_with(Marker))throw std::runtime_error("Bridge filename is occupied by another file");}
}
