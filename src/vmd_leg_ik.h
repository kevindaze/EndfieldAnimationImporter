#pragma once
#include "native_animation_math.h"
#include <numbers>

namespace AnimationImport {
inline Quat RotationVector(Vec angular) {
    const auto magnitude=Length(angular);
    if (magnitude<1e-12) return {0,0,0,1};
    const auto q=Scale(angular,std::sin(magnitude/2)/magnitude);
    return {q.x,q.y,q.z,std::cos(magnitude/2)};
}

using LegJacobian=std::array<Vec,4>;

// There are only three position equations. Decomposing J*J^T yields the
// singular directions without introducing a general-purpose matrix library.
inline std::array<double,4> DampedLegStep(const LegJacobian& columns,Vec error) {
    double gram[3][3]{}, axes[3][3]{{1,0,0},{0,1,0},{0,0,1}};
    for (const auto col : columns)
        for (int r=0;r<3;++r)
            for (int c=0;c<3;++c) gram[r][c]+=col.v[r]*col.v[c];
    for (int sweep=0;sweep<9;++sweep)
        for (const auto [p,q] : {std::pair{0,1},{0,2},{1,2}}) {
            const auto off=gram[p][q];
            if (std::abs(off)<1e-15) continue;
            const auto slope=(gram[q][q]-gram[p][p])/(2*off);
            const auto tangent=std::copysign(1.,slope)/(std::abs(slope)+std::hypot(1.,slope));
            const auto cosine=1/std::hypot(1.,tangent);
            const auto sine=tangent*cosine;
            gram[p][p]-=tangent*off;
            gram[q][q]+=tangent*off;
            gram[p][q]=gram[q][p]=0;
            for (int r=0;r<3;++r) {
                const auto a=axes[r][p],b=axes[r][q];
                axes[r][p]=cosine*a-sine*b;
                axes[r][q]=sine*a+cosine*b;
                if (r==p||r==q) continue;
                const auto x=gram[r][p],y=gram[r][q];
                gram[r][p]=gram[p][r]=cosine*x-sine*y;
                gram[r][q]=gram[q][r]=sine*x+cosine*y;
            }
        }

    constexpr double maxStep=std::numbers::pi/4;
    std::array<double,4> update{};
    for (int axis=0;axis<3;++axis) {
        if (gram[axis][axis]<=1e-14) continue;
        const Vec direction{axes[0][axis],axes[1][axis],axes[2][axis]};
        const auto singular=std::sqrt(gram[axis][axis]);
        const double projected=Dot(direction,error)/singular;
        std::array<double,4> partial{};
        double largest=0,sensitivity=0;
        for (size_t i=0;i<columns.size();++i) {
            const auto v=Dot(direction,columns[i])/singular;
            sensitivity+=std::abs(v)*Length(columns[i])/singular;
            partial[i]=v*projected;
            largest=std::max(largest,std::abs(partial[i]));
        }
        const auto limit=maxStep/std::max(1.,sensitivity);
        const auto damping=largest>limit?limit/largest:1.;
        for (size_t i=0;i<columns.size();++i) update[i]+=.8*damping*partial[i];
    }
    double largest=0;
    for (const auto angle : update) largest=std::max(largest,std::abs(angle));
    if (largest>maxStep)
        for (auto& angle : update) angle*=maxStep/(maxStep+largest);
    return update;
}

inline double KneeFlexion(Quat q) {
    if (q.w<0) q={-q.x,-q.y,-q.z,-q.w};
    return std::clamp(2*std::atan2(q.x,q.w),0.,std::numbers::pi);
}

// MMD's standard legs use a free hip and a one-axis knee. Update both
// together so the hip's twist follows the incoming pose as it reaches the goal.
inline void SolveVmdLeg(Vec upper,Vec upperRest,Vec lower,Vec goal,Quat& hip,Quat& knee) {
    const auto chainLength=Length(upper)+Length(lower);
    if (!std::isfinite(chainLength)||chainLength<1e-6) return;
    const auto y=Unit(upperRest);
    const Vec x{1,0,0},z=Cross(x,y);
    upper=Scale(upper,1/chainLength);
    lower=Scale(lower,1/chainLength);
    goal=Scale(goal,1/chainLength);
    auto flexion=KneeFlexion(knee);
    knee=RotationVector({flexion,0,0});
    for (int iteration=0;iteration<40;++iteration) {
        const auto center=ufbx_quat_rotate_vec3(hip,upper);
        const auto tip=Add(center,ufbx_quat_rotate_vec3(ufbx_quat_mul(hip,knee),lower));
        const auto xAxis=ufbx_quat_rotate_vec3(hip,x);
        const LegJacobian jacobian{
            Cross(xAxis,tip),
            Cross(ufbx_quat_rotate_vec3(hip,y),tip),
            Cross(ufbx_quat_rotate_vec3(hip,z),tip),
            Cross(xAxis,Sub(tip,center))
        };
        auto error=Sub(goal,tip);
        const auto distance=Length(error);
        if (distance>.25) error=Scale(error,.25/distance);
        auto angles=DampedLegStep(jacobian,error);
        const auto nextFlexion=std::clamp(flexion+angles[3],0.,std::numbers::pi);
        if (nextFlexion!=flexion+angles[3]) {
            // Re-solve the hip against the residual after holding the knee at the bound.
            auto constrained=jacobian;
            constrained[3]={};
            angles=DampedLegStep(constrained,Sub(error,Scale(jacobian[3],nextFlexion-flexion)));
            angles[3]=nextFlexion-flexion;
        }
        const auto twist=Add(Scale(x,angles[0]),Add(Scale(y,angles[1]),Scale(z,angles[2])));
        hip=Unit(ufbx_quat_mul(hip,RotationVector(twist)));
        flexion+=angles[3];
        knee=RotationVector({flexion,0,0});
        if (iteration>10&&std::max(Length(twist),std::abs(angles[3]))<.001) break;
    }
}

inline void SolveVmdToe(Vec tail,Vec goal,Quat& ankle) {
    const auto length=Length(tail);
    if (!std::isfinite(length)||length<1e-6) return;
    const auto y=Scale(tail,1/length);
    const Vec x{1,0,0},z=Cross(x,y);
    goal=Scale(goal,1/length);
    for (int iteration=0;iteration<8;++iteration) {
        const auto tip=ufbx_quat_rotate_vec3(ankle,y);
        auto error=Sub(goal,tip);
        const auto distance=Length(error);
        if (distance>.5) error=Scale(error,.5/distance);
        const auto angles=DampedLegStep({
            Cross(ufbx_quat_rotate_vec3(ankle,x),tip),{},
            Cross(ufbx_quat_rotate_vec3(ankle,z),tip),{}},error);
        const auto twist=Add(Scale(x,angles[0]),Scale(z,angles[2]));
        ankle=Unit(ufbx_quat_mul(ankle,RotationVector(twist)));
    }
}
}
