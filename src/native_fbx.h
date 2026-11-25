#pragma once
#include "native_animation_math.h"
#include <map>
#include <memory>
#include <span>

namespace AnimationImport {
inline std::string FbxName(std::string_view raw) {
    if (const auto colon=raw.rfind(':');colon!=raw.npos) raw.remove_prefix(colon+1);
    std::string name;
    for (char c : raw) {
        if (c=='_'||c=='.'||c==' ') continue;
        name+=c>='A'&&c<='Z'?char(c-'A'+'a'):c;
    }
    if (name.starts_with("mixamorig")) name.erase(0,9);
    return name;
}
inline auto FbxAliases() {
    std::map<std::string,std::vector<std::string>> aliases{
        {"Bip001_Pelvis",{"hips","pelvis"}}, {"Bip001_Spine",{"spine"}},
        {"Bip001_Spine1",{"spine1"}}, {"Bip001_Spine2",{"spine2","chest"}},
        {"Bip001_Neck",{"neck"}}, {"Bip001_Head",{"head"}}
    };
    for (char side : {'L','R'}) {
        const std::string suffix=side=='L'?"l":"r", prefix=side=='L'?"left":"right";
        struct Alias {const char* target; const char* first; const char* second;};
        const Alias limbs[]{
            {"Clavicle","shoulder","shoulder"},{"UpperArm","upperarm","arm"},
            {"Forearm","lowerarm","forearm"},{"Hand","hand","hand"},{"Thigh","upperleg","upleg"},
            {"Calf","lowerleg","leg"},{"Foot","foot","foot"},{"Toe0","toe","toebase"}
        };
        for (auto& limb : limbs)
            aliases[Side(side,limb.target)]={limb.first+suffix,prefix+limb.second};
        const char* fingers[]{"thumb","index","middle","ring","little"};
        const char* joints[]{"proximal","intermediate","distal"};
        for (int digit=0;digit<5;++digit)
            for (int joint=0;joint<3;++joint)
                aliases[Side(side,Finger(digit,joint))]={
                    std::string(fingers[digit])+joints[joint]+suffix,
                    prefix+"hand"+(digit==4?"pinky":fingers[digit])+std::to_string(joint+1)};
    }
    for (auto& [target,names] : aliases) names.insert(names.begin(),FbxName(target));
    return aliases;
}

class FbxMotion {
    using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    using Nodes=std::map<std::string,ufbx_node*>;
    Scene scene_{nullptr,ufbx_free_scene};
    Nodes nodes_;
    std::vector<ufbx_anim_stack*> stacks_;
    size_t selected_=0;

    static std::string Error(const ufbx_error& error) {
        char text[384]{};
        ufbx_format_error(text,sizeof(text),&error);
        return text;
    }
    bool Animated(const Nodes& nodes,const ufbx_anim_stack& stack) const {
        std::vector<bool> influencers(scene_->elements.count);
        for (const auto& [_,bone] : nodes)
            for (auto node=bone;node;node=node->parent) influencers[node->element_id]=true;
        for (const auto* layer : stack.layers)
            for (const auto& prop : layer->anim_props)
                if (influencers[prop.element->element_id]&&prop.anim_value&&
                    (prop.anim_value->curves[0]||prop.anim_value->curves[1]||prop.anim_value->curves[2])) return true;
        return false;
    }

public:
    explicit FbxMotion(std::span<const char> bytes) {
        ufbx_load_opts options{};
        options.file_format=UFBX_FILE_FORMAT_FBX;
        options.ignore_geometry=true;
        options.ignore_embedded=true;
        options.ignore_missing_external_files=true;
        options.node_depth_limit=128;
        options.target_axes=ufbx_axes_right_handed_y_up;
        options.target_unit_meters=1;
        options.temp_allocator.memory_limit=size_t(1024)*1024*1024;
        options.result_allocator.memory_limit=size_t(2)*1024*1024*1024;
        ufbx_error error{};
        scene_.reset(ufbx_load_memory(bytes.data(),bytes.size(),&options,&error));
        if (!scene_) throw std::runtime_error("FBX parse: "+Error(error));
        if (scene_->nodes.count>10000) throw std::runtime_error("FBX hierarchy exceeds 10000 nodes");
        if (scene_->constraints.count) throw std::runtime_error("FBX contains constraints; bake the animation or use Blender");
        const auto aliases=FbxAliases();
        for (auto* pelvis : scene_->nodes) {
            const auto rootName=FbxName({pelvis->name.data,pelvis->name.length});
            const auto& roots=aliases.at("Bip001_Pelvis");
            if (std::find(roots.begin(),roots.end(),rootName)==roots.end()) continue;
            std::map<std::string,std::vector<ufbx_node*>> candidates;
            std::vector<ufbx_node*> pending{pelvis};
            while (!pending.empty()) {
                auto* node=pending.back();
                pending.pop_back();
                candidates[FbxName({node->name.data,node->name.length})].push_back(node);
                for (auto* child : node->children) pending.push_back(child);
            }
            // Both Mixamo's shoulders and the leg chain usually branch below the hips.
            Nodes mapped{{"Bip001_Pelvis",pelvis}};
            bool complete=true;
            for (const auto& [semantic,choices] : aliases) {
                if (semantic=="Bip001_Pelvis") continue;
                for (const auto& choice : choices) {
                    auto found=candidates.find(choice);
                    if (found==candidates.end()) continue;
                    if (found->second.size()==1) mapped.emplace(semantic,found->second.front());
                    break;
                }
                if (Required(semantic)&&!mapped.contains(semantic)) complete=false;
            }
            if (!complete) continue;
            std::vector<ufbx_anim_stack*> animated;
            for (auto* stack : scene_->anim_stacks) {
                const double duration=stack->time_end-stack->time_begin;
                if (std::isfinite(duration)&&duration>=.2&&Animated(mapped,*stack)) animated.push_back(stack);
            }
            if (animated.empty()) continue;
            if (!nodes_.empty()) throw std::runtime_error("Expected one animated FBX humanoid; export each actor separately");
            nodes_=std::move(mapped);
            stacks_=std::move(animated);
        }
        if (nodes_.empty()) throw std::runtime_error("FBX has no supported animated humanoid skeleton");
        if (stacks_.size()>128) throw std::runtime_error("FBX contains more than 128 animated takes");
    }
    size_t TakeCount() const { return stacks_.size(); }
    void Select(size_t index) { selected_=index; }
    std::string TakeName(size_t index) const {
        const auto name=stacks_.at(index)->name;
        return {name.data,name.length};
    }
    double Duration() const { return stacks_[selected_]->time_end-stacks_[selected_]->time_begin; }
    int SampleRate() const { return 10; }
    Pose Bind() const {
        Pose result;
        for (const auto& [name,node] : nodes_) {
            auto matrix=node->node_to_world;
            if (node->bind_pose&&node->bind_pose->is_bind_pose)
                for (const auto& bone : node->bind_pose->bone_poses)
                    if (bone.bone_node==node) { matrix=bone.bone_to_world; break; }
            result[name]=matrix;
        }
        return result;
    }
    Pose Sample(double seconds) const {
        ufbx_error error{};
        const auto& stack=*stacks_[selected_];
        Scene frame(ufbx_evaluate_scene(scene_.get(),stack.anim,stack.time_begin+seconds,nullptr,&error),ufbx_free_scene);
        if (!frame) throw std::runtime_error("FBX evaluation: "+Error(error));
        Pose result;
        for (const auto& [name,node] : nodes_) result[name]=frame->nodes.data[node->typed_id]->node_to_world;
        return result;
    }
};
}
