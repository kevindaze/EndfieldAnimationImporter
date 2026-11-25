#pragma once
#include "native_animation_math.h"
#include "vmd_leg_ik.h"
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <map>
#include <span>
#include <string_view>

namespace AnimationImport {
struct MmdBone {
    const char* name;
    int parent;
    Vec rest;
    int effector=-1;
    std::array<int,2> links{-1,-1};
};
inline constexpr MmdBone mmdBones[]{
#include "standard_mmd_profile.inc"
};

class VmdReader {
    std::span<const char> bytes_;
public:
    explicit VmdReader(std::span<const char> bytes):bytes_(bytes) {}
    bool Empty() const { return bytes_.empty(); }
    std::span<const char> Take(size_t size) {
        if (size>bytes_.size()) throw std::runtime_error("Truncated VMD data");
        const auto result=bytes_.first(size);
        bytes_=bytes_.subspan(size);
        return result;
    }
    template<class T> T Read() {
        T value;
        const auto bytes=Take(sizeof(T));
        std::memcpy(&value,bytes.data(),sizeof(T));
        return value;
    }
    uint32_t Count(size_t stride) {
        const auto count=Read<uint32_t>();
        if (count>bytes_.size()/stride) throw std::runtime_error("Invalid VMD record count");
        return count;
    }
    void SkipRecords(size_t stride) {
        if (!Empty()) Take(size_t(Count(stride))*stride);
    }
};

inline std::string VmdName(std::span<const char> field) {
    const auto count=int(std::find(field.begin(),field.end(),'\0')-field.begin());
    if (!count) return {};
    wchar_t wide[32]{}, normalized[64]{};
    const int n=MultiByteToWideChar(932,MB_ERR_INVALID_CHARS,field.data(),count,wide,32);
    if (!n) return {};
    const int k=NormalizeString(NormalizationKC,wide,n,normalized,64);
    if (k<=0) return {};
    char utf[256]{};
    const int length=WideCharToMultiByte(CP_UTF8,0,normalized,k,utf,sizeof(utf),nullptr,nullptr);
    std::string name(utf,size_t(length));
    for (const auto& [oldName,newName] : {
            std::pair{"左肘","左ひじ"}, {"右肘","右ひじ"}, {"左膝","左ひざ"}, {"右膝","右ひざ"},
            {"左人差指","左人指"}, {"右人差指","右人指"}})
        if (name.starts_with(oldName)) return newName+name.substr(std::strlen(oldName));
    return name;
}

inline double VmdBezier(double t, const std::array<uint8_t,16>& curve, int channel) {
    const auto coordinate=[&](int index){ return std::min<int>(curve[size_t(channel+index)],127)/127.; };
    const double x1=coordinate(0),y1=coordinate(4),x2=coordinate(8),y2=coordinate(12);
    if ((x1==y1&&x2==y2)||t<=0||t>=1) return t;
    const auto cubic=[](double u,double a,double b) {
        const auto v=1-u;
        return 3*u*v*(v*a+u*b)+u*u*u;
    };
    double low=0,high=1;
    for (int i=0;i<18;++i) {
        const auto mid=(low+high)*.5;
        if (cubic(mid,x1,x2)<t) low=mid; else high=mid;
    }
    return cubic((low+high)*.5,y1,y2);
}

class VmdMotion {
    struct Key {
        Vec offset{};
        Quat rotation{0,0,0,1};
        std::array<uint8_t,16> curve{};
    };
    struct Joint {
        Vec position{};
        Quat rotation{0,0,0,1};
    };
    std::array<std::map<uint32_t,Key>,std::size(mmdBones)> keys_;
    std::array<std::map<uint32_t,bool>,std::size(mmdBones)> switches_;
    std::map<std::string,size_t> names_;
    std::map<std::string,size_t> mapped_;
    uint32_t first_=UINT32_MAX,last_=0;

    static Vec Reflect(Vec v) { return {v.x,v.y,-v.z}; }
    static Key Interpolate(const std::map<uint32_t,Key>& keys,double frame) {
        if (keys.empty()) return {};
        auto right=keys.upper_bound(uint32_t(frame));
        if (right==keys.begin()) return right->second;
        if (right==keys.end()) return keys.rbegin()->second;
        const auto left=std::prev(right);
        const auto u=(frame-left->first)/(right->first-left->first);
        const auto& a=left->second;
        const auto& b=right->second;
        return {{std::lerp(a.offset.x,b.offset.x,VmdBezier(u,b.curve,0)),
                 std::lerp(a.offset.y,b.offset.y,VmdBezier(u,b.curve,1)),
                 std::lerp(a.offset.z,b.offset.z,VmdBezier(u,b.curve,2))},
                ufbx_quat_slerp(a.rotation,b.rotation,VmdBezier(u,b.curve,3))};
    }
    void MapBody() {
        const auto add=[&](const std::string& semantic,const std::string& name) {
            mapped_.emplace(semantic,names_.at(name));
        };
        add("Bip001_Pelvis","下半身"); add("Bip001_Spine","上半身");
        add("Bip001_Spine1","上半身2"); add("Bip001_Spine2","上半身3");
        add("Bip001_Neck","首"); add("Bip001_Head","頭");
        for (char side : {'L','R'}) {
            const std::string jp=side=='L'?"左":"右";
            for (const auto& [target,source] : {
                    std::pair{"Clavicle","肩"}, {"UpperArm","腕"}, {"Forearm","ひじ"}, {"Hand","手首"},
                    {"Thigh","足"}, {"Calf","ひざ"}, {"Foot","足首"}, {"Toe0","つま先"}})
                add(Side(side,target),jp+source);
            const char* fingers[]{"親指","人指","中指","薬指","小指"};
            for (int digit=0;digit<5;++digit)
                for (int joint=0;joint<3;++joint)
                    add(Side(side,Finger(digit,joint)),jp+fingers[digit]+std::to_string(joint+(digit?1:0)));
        }
    }

public:
    explicit VmdMotion(std::span<const char> bytes) {
        for (size_t i=0;i<std::size(mmdBones);++i) names_.emplace(mmdBones[i].name,i);
        MapBody();
        VmdReader input(bytes);
        const auto signature=input.Take(30);
        const std::string_view header(signature.data(),signature.size());
        if (header.starts_with("Vocaloid Motion Data 0002")) input.Take(20);
        else if (header.starts_with("Vocaloid Motion Data file")) input.Take(10);
        else throw std::runtime_error("Invalid VMD signature");

        const auto count=input.Count(111);
        if (count>2000000) throw std::runtime_error("Too many VMD bone keys");
        for (uint32_t i=0;i<count;++i) {
            const auto name=VmdName(input.Take(15));
            const auto frame=input.Read<uint32_t>();
            const Vec offset{input.Read<float>(),input.Read<float>(),input.Read<float>()};
            const Quat rotation{input.Read<float>(),input.Read<float>(),input.Read<float>(),input.Read<float>()};
            const auto curve=input.Take(64);
            auto found=names_.find(name);
            if (found==names_.end()) continue;
            if (!std::isfinite(Length(offset))||Length(offset)>100000)
                throw std::runtime_error("Invalid VMD bone translation");
            Key key{Reflect(offset),Unit(Quat{-rotation.x,-rotation.y,rotation.z,rotation.w})};
            std::memcpy(key.curve.data(),curve.data(),key.curve.size());
            auto& track=keys_[found->second];
            const auto moving=[](const Key& candidate) {
                return Length(candidate.offset)>1e-7||
                       std::abs(candidate.rotation.x)+std::abs(candidate.rotation.y)+std::abs(candidate.rotation.z)>1e-7;
            };
            auto [it,inserted]=track.try_emplace(frame,key);
            if (!inserted&&(moving(key)||!moving(it->second))) it->second=key;
            first_=std::min(first_,frame);
            last_=std::max(last_,frame);
        }
        if (first_==UINT32_MAX) throw std::runtime_error("VMD contains no supported body bone motion");
        input.SkipRecords(23); // morphs
        input.SkipRecords(61); // cameras
        input.SkipRecords(28); // lighting
        input.SkipRecords(9);  // self-shadow
        if (!input.Empty()) {
            const auto frames=input.Count(9);
            for (uint32_t i=0;i<frames;++i) {
                const auto frame=input.Read<uint32_t>();
                input.Take(1); // visibility
                const auto states=input.Count(21);
                for (uint32_t j=0;j<states;++j) {
                    const auto name=VmdName(input.Take(20));
                    const bool enabled=input.Read<uint8_t>()!=0;
                    auto item=names_.find(name);
                    if (item!=names_.end()&&mmdBones[item->second].effector>=0)
                        switches_[item->second][frame]=enabled;
                }
            }
        }
    }

    double Duration() const { return double(last_-first_)/30; }
    int SampleRate() const { return 30; }
    Pose Bind() const {
        Pose result;
        for (const auto& [name,index] : mapped_) result[name]=Transform(Reflect(mmdBones[index].rest),{0,0,0,1});
        return result;
    }

    Pose Sample(double seconds) const {
        const double frame=std::clamp(first_+seconds*30.,double(first_),double(last_));
        std::array<Joint,std::size(mmdBones)> local{},world{};
        for (size_t i=0;i<std::size(mmdBones);++i) {
            const auto& bone=mmdBones[i];
            const auto rest=bone.parent<0?bone.rest:Sub(bone.rest,mmdBones[bone.parent].rest);
            const auto key=Interpolate(keys_[i],frame);
            local[i]={Add(Reflect(rest),key.offset),key.rotation};
        }
        const auto update=[&] {
            for (size_t i=0;i<std::size(mmdBones);++i) {
                const auto parent=mmdBones[i].parent;
                if (parent<0) world[i]=local[i];
                else world[i]={Add(world[parent].position,ufbx_quat_rotate_vec3(world[parent].rotation,local[i].position)),
                               Unit(ufbx_quat_mul(world[parent].rotation,local[i].rotation))};
            }
        };
        update();
        for (size_t i=0;i<std::size(mmdBones);++i) {
            const auto& bone=mmdBones[i];
            if (bone.effector<0||keys_[i].empty()) continue;
            const auto& states=switches_[i];
            const auto next=states.upper_bound(uint32_t(frame));
            if (next!=states.begin()&&!std::prev(next)->second) continue;
            const auto goal=world[i].position;
            const auto knee=bone.links[0], hip=bone.links[1], end=bone.effector;
            if (hip<0) {
                const auto parent=world[mmdBones[knee].parent].rotation;
                const auto direction=ufbx_quat_rotate_vec3(Inverse(parent),Sub(goal,world[knee].position));
                const auto tail=Reflect(Sub(mmdBones[end].rest,mmdBones[knee].rest));
                SolveVmdToe(tail,direction,local[knee].rotation);
                update();
                continue;
            }
            const auto parent=world[mmdBones[hip].parent].rotation;
            const auto direction=ufbx_quat_rotate_vec3(Inverse(parent),Sub(goal,world[hip].position));
            const auto upper=Reflect(Sub(mmdBones[knee].rest,mmdBones[hip].rest));
            const auto lower=Reflect(Sub(mmdBones[end].rest,mmdBones[knee].rest));
            SolveVmdLeg(local[knee].position,upper,lower,direction,local[hip].rotation,local[knee].rotation);
            update();
        }
        Pose result;
        for (const auto& [name,index] : mapped_) result[name]=Transform(world[index].position,world[index].rotation);
        return result;
    }
};
}
