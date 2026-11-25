#pragma once
#include "ufbx/ufbx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AnimationImport {
using Vec = ufbx_vec3;
using Quat = ufbx_quat;
using Matrix = ufbx_matrix;
using Pose = std::unordered_map<std::string, Matrix>;

inline Vec Add(Vec a, Vec b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec Scale(Vec a, double s) { return {a.x*s, a.y*s, a.z*s}; }
inline Vec Sub(Vec a, Vec b) { return Add(a, Scale(b, -1)); }
inline double Dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec Cross(Vec a, Vec b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
inline double Length(Vec a) { return std::sqrt(Dot(a,a)); }
inline Vec Unit(Vec a) {
    const auto length = Length(a);
    if (!std::isfinite(length) || length < 1e-8) throw std::runtime_error("Degenerate bone direction");
    return Scale(a, 1/length);
}
inline Quat Inverse(Quat q) { return {-q.x,-q.y,-q.z,q.w}; }
inline Quat Unit(Quat q) {
    const double length = std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if (!std::isfinite(length) || length < 1e-8) throw std::runtime_error("Invalid rotation quaternion");
    return {q.x/length,q.y/length,q.z/length,q.w/length};
}
inline Quat DirectionRotation(Vec from, Vec to) {
    from=Unit(from); to=Unit(to);
    const auto cosine=Dot(from,to);
    auto axis=Cross(from,to);
    if (cosine < -.9999999) {
        axis=Unit(Cross(from, std::abs(from.x)<.8 ? Vec{1,0,0} : Vec{0,1,0}));
        return {axis.x,axis.y,axis.z,0};
    }
    return Unit(Quat{axis.x,axis.y,axis.z,1+cosine});
}
inline Matrix Mul(const Matrix& a,const Matrix& b) { return ufbx_matrix_mul(&a,&b); }
inline Matrix Inverse(const Matrix& m) { return ufbx_matrix_invert(&m); }
inline Vec Position(const Matrix& m) { return m.cols[3]; }
inline Quat Rotation(const Matrix& m) { return Unit(ufbx_matrix_to_transform(&m).rotation); }
inline Matrix Transform(Vec position, Quat rotation, Vec scale={1,1,1}) {
    const ufbx_transform t{position,rotation,scale};
    return ufbx_transform_to_matrix(&t);
}
inline std::string Side(char side, const std::string& part) { return std::string("Bip001_")+side+"_"+part; }
inline std::string Finger(int digit, int joint) {
    return "Finger"+std::to_string(digit)+(joint?std::to_string(joint):"");
}
inline bool Required(const std::string& name) {
    if (name=="Bip001_Pelvis"||name=="Bip001_Spine"||name=="Bip001_Neck"||name=="Bip001_Head") return true;
    for (char side : {'L','R'})
        for (const char* part : {"UpperArm","Forearm","Hand","Thigh","Calf","Foot"})
            if (name==Side(side,part)) return true;
    return false;
}
inline void RequireBody(const Pose& pose) {
    for (const auto* name : {"Bip001_Pelvis","Bip001_Spine","Bip001_Neck","Bip001_Head"})
        if (!pose.contains(name)) throw std::runtime_error(std::string("Missing humanoid bind bone: ")+name);
    for (char side : {'L','R'})
        for (const char* part : {"UpperArm","Forearm","Hand","Thigh","Calf","Foot"})
            if (!pose.contains(Side(side,part))) throw std::runtime_error("Missing humanoid bind bone: "+Side(side,part));
}
inline Matrix BodyBasis(const Pose& pose) {
    const auto up=Unit(Sub(Position(pose.at("Bip001_Head")),Position(pose.at("Bip001_Pelvis"))));
    auto right=Sub(Position(pose.at(Side('R',"Thigh"))),Position(pose.at(Side('L',"Thigh"))));
    right=Unit(Sub(right,Scale(up,Dot(right,up))));
    Matrix basis{};
    basis.cols[0]=right;
    basis.cols[1]=up;
    basis.cols[2]=Cross(right,up);
    return basis;
}
inline double LegHeight(const Pose& pose) {
    const auto root=Position(pose.at("Bip001_Pelvis"));
    const auto length=(Length(Sub(root,Position(pose.at(Side('L',"Foot")))))+
                       Length(Sub(root,Position(pose.at(Side('R',"Foot"))))))/2;
    if (!std::isfinite(length)||length<1e-6) throw std::runtime_error("Invalid reference leg length");
    return length;
}
}
