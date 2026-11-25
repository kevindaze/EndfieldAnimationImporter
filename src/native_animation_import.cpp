#include "imported_animation.h"
#include "native_fbx.h"
#include "native_vmd.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace AnimationImport {
using Json=Presets::Json;
namespace fs=std::filesystem;

Matrix ReadMatrix(const Json& rows) {
    if (!rows.is_array()||rows.size()!=4) throw std::runtime_error("Invalid bind matrix");
    Matrix result{};
    for (size_t row=0;row<4;++row) {
        if (!rows[row].is_array()||rows[row].size()!=4) throw std::runtime_error("Invalid bind matrix row");
        for (size_t col=0;col<4;++col) {
            const auto value=rows[row][col].get<double>();
            if (!std::isfinite(value)||std::abs(value)>1e9) throw std::runtime_error("Invalid bind component");
            if (row<3) result.cols[col].v[row]=value;
            else if (std::abs(value-(col==3?1:0))>1e-5) throw std::runtime_error("Non-affine bind matrix");
        }
    }
    if (std::abs(ufbx_matrix_determinant(&result))<1e-12) throw std::runtime_error("Singular bind matrix");
    return result;
}

Matrix Rigid(const Matrix& matrix) {
    const auto determinant=ufbx_matrix_determinant(&matrix);
    const auto t=ufbx_matrix_to_transform(&matrix);
    const auto [low,high]=std::minmax({t.scale.x,t.scale.y,t.scale.z});
    if (determinant<=1e-9||high<=0||high-low>.01*high)
        throw std::runtime_error("Target skeleton contains a singular, mirrored or nonuniform body bone");
    return Transform(t.translation,Unit(t.rotation),t.scale);
}

struct TargetBone {
    std::string name;
    Matrix bind{},local{};
    int parent=-1;
    bool weighted=false;
    const Json* capture=nullptr;
};
struct TargetSkeleton {
    std::vector<TargetBone> bones;
    Pose namedBind;
};

TargetSkeleton ReadTarget(const Json& reference,int actor,const Pose& source) {
    if (reference.at("format")!="endfield-skeleton-pose-reference"||reference.at("version")!=1)
        throw std::runtime_error("Unsupported skeleton reference format");
    std::vector<const Json*> palettes;
    for (const auto& palette : reference.at("mesh_bindposes"))
        if (palette.at("actor")==actor) palettes.push_back(&palette);
    if (palettes.empty()) throw std::runtime_error("The selected role has no mesh bind poses");
    const auto score=[](const Json* palette) {
        int result=0;
        for (const auto& bone : palette->at("bones")) result+=Required(bone.at("name").get<std::string>());
        return result;
    };
    std::stable_sort(palettes.begin(),palettes.end(),[&](auto a,auto b){return score(a)>score(b);});
    TargetSkeleton target;
    std::unordered_map<std::string,const Json*> nodes;
    std::map<std::string,std::string> mapped;
    for (const auto& bone : reference.at("bones")) {
        if (bone.at("actor")!=actor) continue;
        const auto key=bone.at("key").get<std::string>();
        const auto name=bone.at("name").get<std::string>();
        if (!nodes.emplace(key,&bone).second) throw std::runtime_error("Duplicate target bone key");
        if (source.contains(name)&&!mapped.emplace(name,key).second) throw std::runtime_error("Ambiguous target bone: "+name);
    }
    if (nodes.empty()||nodes.size()>3000) throw std::runtime_error("Invalid target skeleton size");
    std::unordered_map<std::string,int> indices;
    std::set<std::string> visiting;
    const auto visit=[&](auto&& self,const std::string& key)->int {
        if (key.empty()||!nodes.contains(key)) return -1;
        if (auto found=indices.find(key);found!=indices.end()) return found->second;
        if (visiting.size()>=128||!visiting.insert(key).second) throw std::runtime_error("Invalid target hierarchy");
        const auto* bone=nodes.at(key);
        const auto parentValue=bone->find("parent_key");
        const std::string parentKey=parentValue!=bone->end()&&parentValue->is_string()?parentValue->get<std::string>():"";
        const auto parent=self(self,parentKey);
        const int index=int(target.bones.size());
        target.bones.push_back({bone->at("name").get<std::string>(),{},{},parent,false,bone});
        indices[key]=index;
        visiting.erase(key);
        return index;
    };
    for (const auto& [_,key] : mapped) visit(visit,key);
    for (const auto* palette : palettes) {
        const auto toModel=Mul(ReadMatrix(palette->at("root_world_to_local")),
                               ReadMatrix(palette->at("renderer_local_to_world")));
        for (const auto& bone : palette->at("bones")) {
            const auto found=indices.find(bone.at("key").get<std::string>());
            if (found==indices.end()) continue;
            auto& b=target.bones[found->second];
            if (b.weighted) continue;
            b.bind=Rigid(Mul(toModel,Inverse(ReadMatrix(bone.at("inverse_bind_matrix")))));
            b.weighted=true;
        }
    }
    const auto inverseRoot=ReadMatrix(palettes.front()->at("root_world_to_local"));
    for (auto& b : target.bones) {
        if (!b.weighted) {
            const auto& bone=*b.capture;
            Matrix current{};
            if (bone.contains("world_matrix")) current=ReadMatrix(bone.at("world_matrix"));
            else {
                const auto p=bone.at("world_position").get<std::array<double,3>>();
                const auto q=bone.at("world_rotation_xyzw").get<std::array<double,4>>();
                current=Transform({p[0],p[1],p[2]},Unit(Quat{q[0],q[1],q[2],q[3]}));
            }
            b.bind=Rigid(Mul(inverseRoot,current));
        }
        b.local=b.parent<0?b.bind:Mul(Inverse(target.bones[b.parent].bind),b.bind);
        if (b.weighted&&source.contains(b.name)) target.namedBind[b.name]=b.bind;
    }
    RequireBody(target.namedBind);
    return target;
}

std::string SemanticChild(const std::string& name) {
    for (char side : {'L','R'})
        for (const auto& [parent,child] : {std::pair{"Clavicle","UpperArm"},{"UpperArm","Forearm"},
                 {"Forearm","Hand"},{"Hand","Finger2"},{"Thigh","Calf"},{"Calf","Foot"},{"Foot","Toe0"}})
            if (name==Side(side,parent)) return Side(side,child);
    return {};
}

struct Sample { float time; PoseMath::Quaternion rotation; PoseMath::Vector3 position; };

Json EncodeSamples(const std::vector<Sample>& samples,bool positional) {
    // Same tolerances as the Blender worker. Bound the simplifier's work even
    // for input that forces a split close to the start on every iteration.
    const auto count=samples.size();
    std::vector<bool> keep(count);
    keep.front()=keep.back()=true;
    for (size_t start=0;start+1<count;) {
        const size_t boundary=std::min(start+180,count-1);
        keep[boundary]=true;
        std::vector<std::pair<size_t,size_t>> pending{{start,boundary}};
        while (!pending.empty()) {
            const auto [first,last]=pending.back();
            pending.pop_back();
            const auto& a=samples[first];
            const auto& b=samples[last];
            size_t index=first;
            float worst=1;
            for (size_t i=first+1;i<last;++i) {
                const auto& sample=samples[i];
                const auto u=(sample.time-a.time)/(b.time-a.time);
                const auto q=PoseMath::Nlerp(a.rotation,b.rotation,u);
                const auto r=sample.rotation;
                const float dot=q.x*r.x+q.y*r.y+q.z*r.z+q.w*r.w;
                float error=2*std::acos(std::min(1.f,std::abs(dot)))/.008726646f;
                if (positional) {
                    const auto p=PoseMath::Lerp(a.position,b.position,u);
                    error=std::max(error,std::hypot(p.x-sample.position.x,p.y-sample.position.y,p.z-sample.position.z)/.001f);
                }
                if (error>worst) { worst=error; index=i; }
            }
            if (index!=first) {
                keep[index]=true;
                pending.emplace_back(first,index);
                pending.emplace_back(index,last);
            }
        }
        start=boundary;
    }
    auto keys=Json::array();
    for (size_t i=0;i<count;++i) {
        if (!keep[i]) continue;
        const auto& [time,q,p]=samples[i];
        Json key{{"time",time},{"rotation",{q.x,q.y,q.z,q.w}}};
        if (positional) key["position"]={p.x,p.y,p.z};
        keys.push_back(std::move(key));
    }
    return keys;
}

template<class Motion>
Json Retarget(const Motion& motion,const TargetSkeleton& target,const Pose& sourceBind,
              const std::string& character,int actor,const std::string& id,const std::string& label) {
    const double duration=motion.Duration();
    if (!std::isfinite(duration)||duration<.2||duration>3600)
        throw std::runtime_error("Source duration must be 0.2 seconds to 1 hour");
    const auto conversion=Mul(Mul(BodyBasis(target.namedBind),Transform({},{0,0,0,1},{1,1,-1})),
                              Inverse(BodyBasis(sourceBind)));
    const auto reverse=Inverse(conversion);
    const auto heightScale=LegHeight(target.namedBind)/LegHeight(sourceBind);
    const auto origin=Position(motion.Sample(0).at("Bip001_Pelvis"));
    const auto steps=size_t(std::ceil(duration*motion.SampleRate()));
    std::vector<std::vector<Sample>> samples(target.bones.size());
    std::vector<Matrix> posed(target.bones.size());
    std::vector<std::string> children;
    for (const auto& bone : target.bones) children.push_back(SemanticChild(bone.name));
    size_t trackCount=0;
    for (auto& b : target.bones) trackCount+=b.name.starts_with("Bip001");
    if (trackCount>64) throw std::runtime_error("Mapped hierarchy exceeds 64 tracks");
    for (size_t frame=0;frame<=steps;++frame) {
        const auto t=frame==steps?duration:double(frame)/motion.SampleRate();
        if (frame&&float(t)<=float(double(frame-1)/motion.SampleRate())) continue;
        const auto source=motion.Sample(t);
        const auto displacement=Scale(ufbx_transform_position(&conversion,Sub(Position(source.at("Bip001_Pelvis")),origin)),heightScale);
        for (size_t i=0;i<target.bones.size();++i) {
            const auto& bone=target.bones[i];
            const auto& parent=bone.parent<0?ufbx_identity_matrix:posed[bone.parent];
            auto world=Mul(parent,bone.local);
            if (target.namedBind.contains(bone.name)) {
                const auto& rest=sourceBind.at(bone.name);
                const auto& animated=source.at(bone.name);
                const auto delta=ufbx_quat_mul(Rotation(animated),Inverse(Rotation(rest)));
                const auto converted=Mul(Mul(conversion,Transform({},delta)),reverse);
                auto rotation=Unit(ufbx_quat_mul(Rotation(converted),Rotation(bone.bind)));
                const auto& child=children[i];
                if (!child.empty()&&target.namedBind.contains(child)) {
                    const auto sourceDirection=ufbx_transform_position(&conversion,Sub(Position(source.at(child)),Position(animated)));
                    const auto restDirection=Sub(Position(target.namedBind.at(child)),Position(bone.bind));
                    const auto localAxis=ufbx_quat_rotate_vec3(Inverse(Rotation(bone.bind)),restDirection);
                    const auto observed=ufbx_quat_rotate_vec3(rotation,localAxis);
                    if (Length(sourceDirection)>1e-6&&Length(observed)>1e-6)
                        rotation=Unit(ufbx_quat_mul(DirectionRotation(observed,sourceDirection),rotation));
                }
                const auto position=bone.name=="Bip001_Pelvis"?Add(Position(bone.bind),displacement):Position(world);
                world=Transform(position,rotation,ufbx_matrix_to_transform(&bone.bind).scale);
            }
            posed[i]=world;
            if (!bone.name.starts_with("Bip001")) continue;
            const auto local=Mul(Inverse(parent),world);
            const auto q=Rotation(local);
            const auto p=Position(local);
            samples[i].push_back({float(t),{float(q.x),float(q.y),float(q.z),float(q.w)},
                                         {float(p.x),float(p.y),float(p.z)}});
        }
    }
    auto tracks=Json::array();
    size_t keyCount=0;
    for (size_t i=0;i<target.bones.size();++i) {
        if (samples[i].empty()) continue;
        const auto& name=target.bones[i].name;
        auto keys=EncodeSamples(samples[i],name=="Bip001_Pelvis");
        keyCount+=keys.size();
        if (keyCount>1000000) throw std::runtime_error("Animation exceeds one million retained keys; split the source animation");
        tracks.push_back({{"bone",name},{"keys",std::move(keys)}});
    }
    Json clip{{"format","endfield-interaction-animation"},{"version",2},{"rotation_space","absolute_local"},
              {"target_character",character},{"id",id},{"name",label},{"duration",float(duration)},{"distance",1.2f},
              {"actors",Json::array({{{"role",actor?"partner":"controlled"},{"tracks",std::move(tracks)}}})}};
    ImportedAnimation::Clip check;
    if (!check.Read(clip)) throw std::runtime_error("Native converter produced an invalid animation");
    return clip;
}

std::vector<char> ReadFile(const fs::path& path,uintmax_t limit) {
    const auto size=fs::file_size(path);
    if (!size||size>limit) throw std::runtime_error("Input file size is outside supported limits");
    std::vector<char> data(static_cast<size_t>(size));
    std::ifstream file(path,std::ios::binary);
    if (!file.read(data.data(),std::streamsize(data.size()))) throw std::runtime_error("Cannot read input file");
    return data;
}

std::string Utf8(const fs::path& path) {
    const auto value=path.u8string();
    return {value.begin(),value.end()};
}

std::string ShortTitle(std::string title,size_t limit) {
    if (title.size()<=limit) return title;
    while (limit&&(static_cast<unsigned char>(title[limit])&0xC0)==0x80) --limit;
    title.resize(limit);
    return title;
}

void WriteJson(const fs::path& path,const Json& value) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out<<value.dump(-1,' ',false,Json::error_handler_t::replace);
    out.close();
    if (!out) throw std::runtime_error("Unable to write conversion output");
}

Json Convert(const fs::path& source,const fs::path& output,const fs::path& referencePath,int actor,bool vmd) {
    const auto bytes=ReadFile(source,512ULL*1024*1024);
    const auto ref=ReadFile(referencePath,64ULL*1024*1024);
    const auto reference=Json::parse(ref);
    const auto character=reference.at(actor?"partner":"controlled").get<std::string>();
    if (!character.starts_with("chr_")||character.size()>44||
        character.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=std::string::npos)
        throw std::runtime_error("Invalid target character identifier");
    uint64_t hash=14695981039346656037ull;
    for (const auto byte : bytes) { hash^=static_cast<unsigned char>(byte); hash*=1099511628211ull; }
    std::ostringstream id;
    id<<(vmd?"eai_vmd_":"eai_fbx_")<<std::hex<<hash<<"_"<<character<<(actor?"":"_main");
    const auto title=ShortTitle(Utf8(source.stem()),70);
    auto files=Json::array();
    const auto write=[&](const auto& motion,const std::string& suffix,const std::string& name) {
        const auto sourceBind=motion.Bind();
        RequireBody(sourceBind);
        const auto target=ReadTarget(reference,actor,sourceBind);
        const auto clipId=id.str()+suffix;
        const auto filename=clipId+".interaction-animation.json";
        WriteJson(output/filename,Retarget(motion,target,sourceBind,character,actor,clipId,name));
        files.push_back(filename);
    };
    if (vmd) {
        const VmdMotion motion(bytes);
        write(motion,"",title);
    } else {
        FbxMotion motion(bytes);
        for (size_t take=0;take<motion.TakeCount();++take) {
            motion.Select(take);
            const bool multiple=motion.TakeCount()>1;
            write(motion,multiple?"_"+std::to_string(take+1):"",
                  multiple?ShortTitle(title+" · "+motion.TakeName(take),100):title);
        }
    }
    return files;
}
}

int wmain(int argc,wchar_t** argv) {
    using namespace AnimationImport;
    if (argc!=7||std::wstring_view(argv[3])!=L"--reference"||std::wstring_view(argv[5])!=L"--actor")
        return 2;
    const fs::path source(argv[1]), output(argv[2]), reference(argv[4]);
    try {
        const std::wstring_view role(argv[6]);
        if (role!=L"partner"&&role!=L"controlled") throw std::runtime_error("Invalid target role");
        auto ext=source.extension().wstring();
        std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return wchar_t(towlower(c));});
        if (ext!=L".fbx"&&ext!=L".vmd") throw std::runtime_error("Select an FBX or VMD file");
        const auto files=Convert(source,output,reference,role==L"partner",ext==L".vmd");
        WriteJson(output/"result.json",{{"ok",true},{"files",files}});
        return 0;
    } catch (const std::exception& error) {
        try { WriteJson(output/"result.json",{{"ok",false},{"error",error.what()}}); } catch (...) {}
        return 1;
    }
}
