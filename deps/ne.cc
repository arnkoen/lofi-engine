#include "ne.h"
#include "tokamak/math/ne_math.h"
#include "tokamak/tokamak.h"
#include <stdio.h>


static inline neQ hmm_to_ne_q(const HMM_Quat& q) {
    return neQ{ q.X, q.Y, q.Z, q.W };
}

static inline neV3 hmm_to_ne_v3(const HMM_Vec3& v) {
    return neV3{ v.X, v.Y, v.Z };
}

static inline HMM_Vec3 ne_to_hmm_vec3(const neV3& v) {
    return HMM_Vec3{ v.X(), v.Y(), v.Z() };
}

static inline HMM_Quat ne_to_hmm_quat(const neQ& q) {
    return HMM_Q(q.X, q.Y, q.Z, q.W);
}

static inline neT3 hmm_to_ne_t3(const HMM_Mat4& m) {
    neT3 t;
    t.pos.Set(m.Elements[3][0], m.Elements[3][1], m.Elements[3][2]);
    t.rot[0].Set(m.Elements[0][0], m.Elements[0][1], m.Elements[0][2]);
    t.rot[1].Set(m.Elements[1][0], m.Elements[1][1], m.Elements[1][2]);
    t.rot[2].Set(m.Elements[2][0], m.Elements[2][1], m.Elements[2][2]);
    return t;
}

static inline HMM_Mat4 ne_to_hmm_mat4(const neT3& t) {
    HMM_Mat4 m = HMM_M4D(1.0f);
    m.Elements[0][0] = t.rot[0].X();
    m.Elements[0][1] = t.rot[0].Y();
    m.Elements[0][2] = t.rot[0].Z();
    m.Elements[1][0] = t.rot[1].X();
    m.Elements[1][1] = t.rot[1].Y();
    m.Elements[1][2] = t.rot[1].Z();
    m.Elements[2][0] = t.rot[2].X();
    m.Elements[2][1] = t.rot[2].Y();
    m.Elements[2][2] = t.rot[2].Z();
    m.Elements[3][0] = t.pos.X();
    m.Elements[3][1] = t.pos.Y();
    m.Elements[3][2] = t.pos.Z();
    return m;
}

static inline neM3 hmm_to_ne_m3(const HMM_Mat4& m) {
    neM3 mat;
    mat[0].Set(m.Elements[0][0], m.Elements[0][1], m.Elements[0][2]);
    mat[1].Set(m.Elements[1][0], m.Elements[1][1], m.Elements[1][2]);
    mat[2].Set(m.Elements[2][0], m.Elements[2][1], m.Elements[2][2]);
    return mat;
}


// C++ wrapper classes for C callbacks
class RigidBodyControllerCB : public neRigidBodyControllerCallback {
public:
    RigidBodyControllerCB(ne_rigid_body_controller_fn callback, void* userdata)
        : fn(callback), udata(userdata) {}

    void RigidBodyControllerCallback(neRigidBodyController* controller, float timeStep) override {
        neRigidBody* body = (neRigidBody*)controller->GetRigidBody();
        if (fn) {
            fn((ne_RigidBody)body, (ne_RigidBodyController)controller, timeStep, udata);
        }
    }

private:
    ne_rigid_body_controller_fn fn;
    void* udata;
};

class JointControllerCB : public neJointControllerCallback {
public:
    JointControllerCB(ne_joint_controller_fn callback, void* userdata)
        : fn(callback), udata(userdata) {}

    void ConstraintControllerCallback(neJointController* controller, float timeStep) override {
        if (fn) {
            fn((ne_JointController)controller, timeStep, udata);
        }
    }

private:
    ne_joint_controller_fn fn;
    void* udata;
};

class CustomAllocator : public neAllocatorAbstract {
public:
    CustomAllocator(ne_Allocator* allocator) : ne_alloc(allocator) {}

    neByte* Alloc(s32 size, s32 alignment = 0) override {
        if (ne_alloc && ne_alloc->alloc) {
            return (neByte*)ne_alloc->alloc((size_t)size, alignment, ne_alloc->udata);
        }
        return (neByte*)malloc(size);
    }

    void Free(neByte* ptr) override {
        if (ne_alloc && ne_alloc->free) {
            ne_alloc->free(ptr, ne_alloc->udata);
        } else {
            free(ptr);
        }
    }

private:
    ne_Allocator* ne_alloc;
};

// Store custom allocator pointers to clean them up
static CustomAllocator* g_custom_allocators[32] = {0};
static int g_allocator_count = 0;


extern "C" {

    #define NE_DEF(val, def) (val == 0 ? def : val)

    //--SIMULATOR------------------------------------------------------------------

    ne_Simulator ne_create_sim(const ne_Desc* desc) {
        neSimulatorSizeInfo info;
        if (desc->size_info) {
            info.rigidBodiesCount = NE_DEF(desc->size_info->rigid_bodies_count, info.rigidBodiesCount);
            info.animatedBodiesCount = NE_DEF(desc->size_info->animated_bodies_count, info.rigidBodiesCount);
            info.rigidParticleCount = desc->size_info->rigid_particle_count;
            info.controllersCount = desc->size_info->controllers_count;
            info.overlappedPairsCount = desc->size_info->overlapped_pairs_count;
            info.geometriesCount = desc->size_info->geometries_count;
            info.constraintsCount = desc->size_info->constraints_count;
            info.constraintSetsCount = desc->size_info->constraint_sets_count;
            info.constraintBufferSize = desc->size_info->constraint_buffer_size;
            info.sensorsCount = desc->size_info->sensors_count;
            info.terrainNodesStartCount = desc->size_info->terrain_nodes_start_count;
            info.terrainNodesGrowByCount = desc->size_info->terrain_nodes_growby_count;
        }
        neV3 gravity = hmm_to_ne_v3(desc->gravity);

        neAllocatorAbstract* allocator = NULL;
        CustomAllocator* custom_alloc = NULL;
        if (desc->allocator) {
            custom_alloc = new CustomAllocator(desc->allocator);
            allocator = custom_alloc;
            // Track the allocator for cleanup
            if (g_allocator_count < 32) {
                g_custom_allocators[g_allocator_count++] = custom_alloc;
            }
        }

        return (ne_Simulator)neSimulator::CreateSimulator(info, allocator, &gravity);
    }

    void ne_destroy_sim(ne_Simulator sim) {
        neSimulator* s = (neSimulator*)sim;
        neSimulator::DestroySimulator(s);

        // Clean up custom allocators
        for (int i = 0; i < g_allocator_count; i++) {
            delete g_custom_allocators[i];
            g_custom_allocators[i] = NULL;
        }
        g_allocator_count = 0;
    }

    ne_RigidBody ne_sim_create_rigid_body(ne_Simulator sim) {
        neSimulator* s = (neSimulator*)sim;
        neRigidBody* body = s->CreateRigidBody();
        return (ne_RigidBody)body;
    }

    ne_AnimBody ne_sim_create_anim_body(ne_Simulator sim) {
        neSimulator* s = (neSimulator*)sim;
        neAnimatedBody* body = s->CreateAnimatedBody();
        return (ne_AnimBody)body;
    }

    void ne_sim_free_rigid_body(ne_Simulator sim, ne_RigidBody body) {
        neSimulator* s = (neSimulator*)sim;
        s->FreeRigidBody((neRigidBody*)body);
    }

    void ne_sim_free_anim_body(ne_Simulator sim, ne_AnimBody body) {
        neSimulator* s = (neSimulator*)sim;
        s->FreeAnimatedBody((neAnimatedBody*)body);
    }

    void ne_sim_advance(ne_Simulator sim, float sec, int32_t steps) {
        neSimulator* s = (neSimulator*)sim;
        s->Advance(sec, steps);
    }

    void ne_sim_set_gravity(ne_Simulator sim, HMM_Vec3 gravity) {
        neSimulator* s = (neSimulator*)sim;
        s->Gravity(hmm_to_ne_v3(gravity));
    }

    HMM_Vec3 ne_sim_get_gravity(ne_Simulator sim) {
        neSimulator* s = (neSimulator*)sim;
        return ne_to_hmm_vec3(s->Gravity());
    }

    //--RIGID BODY-----------------------------------------------------------------

    float ne_rigid_body_get_mass(ne_RigidBody b) {
        return ((neRigidBody*)b)->GetMass();
    }

    void ne_rigid_body_set_mass(ne_RigidBody b, float mass) {
        ((neRigidBody*)b)->SetMass(mass);
    }

    void ne_rigid_body_set_inertia_tensor(ne_RigidBody b, HMM_Vec3 tensor) {
        ((neRigidBody*)b)->SetInertiaTensor(hmm_to_ne_v3(tensor));
    }

    HMM_Vec3 ne_rigid_body_get_pos(ne_RigidBody b) {
        return ne_to_hmm_vec3(((neRigidBody*)b)->GetPos());
    }

    void ne_rigid_body_set_pos(ne_RigidBody b, HMM_Vec3 pos) {
        ((neRigidBody*)b)->SetPos(hmm_to_ne_v3(pos));
    }

    HMM_Quat ne_rigid_body_get_rot(ne_RigidBody b) {
        return ne_to_hmm_quat(((neRigidBody*)b)->GetRotationQ());
    }

    void ne_rigid_body_set_rot(ne_RigidBody b, HMM_Quat rot) {
        ((neRigidBody*)b)->SetRotation(hmm_to_ne_q(rot));
    }

    HMM_Vec3 ne_rigid_body_get_velocity(ne_RigidBody b) {
        return ne_to_hmm_vec3(((neRigidBody*)b)->GetVelocity());
    }

    void ne_rigid_body_set_velocity(ne_RigidBody b, HMM_Vec3 vel) {
        ((neRigidBody*)b)->SetVelocity(hmm_to_ne_v3(vel));
    }

    HMM_Vec3 ne_rigid_body_get_angular_velocity(ne_RigidBody b) {
        return ne_to_hmm_vec3(((neRigidBody*)b)->GetAngularVelocity());
    }

    HMM_Vec3 ne_rigid_body_get_velocity_at_point(ne_RigidBody b, HMM_Vec3 pt) {
        return ne_to_hmm_vec3(((neRigidBody*)b)->GetVelocityAtPoint(hmm_to_ne_v3(pt)));
    }

    void ne_rigid_body_set_force(ne_RigidBody b, HMM_Vec3 force) {
        ((neRigidBody*)b)->SetForce(hmm_to_ne_v3(force));
    }

    void ne_rigid_body_set_torque(ne_RigidBody b, HMM_Vec3 torque) {
        ((neRigidBody*)b)->SetTorque(hmm_to_ne_v3(torque));
    }

    void ne_rigid_body_set_force_with_pos(ne_RigidBody b, HMM_Vec3 force, HMM_Vec3 pos) {
        ((neRigidBody*)b)->SetForce(hmm_to_ne_v3(force), hmm_to_ne_v3(pos));
    }

    HMM_Vec3 ne_rigid_body_get_force(ne_RigidBody b) {
        return ne_to_hmm_vec3(((neRigidBody*)b)->GetForce());
    }

    HMM_Vec3 ne_rigid_body_get_torque(ne_RigidBody b) {
        return ne_to_hmm_vec3(((neRigidBody*)b)->GetTorque());
    }

    void ne_rigid_body_apply_impulse(ne_RigidBody b, HMM_Vec3 impulse) {
        ((neRigidBody*)b)->ApplyImpulse(hmm_to_ne_v3(impulse));
    }

    void ne_rigid_body_apply_impulse_with_pos(ne_RigidBody b, HMM_Vec3 impulse, HMM_Vec3 pos) {
        ((neRigidBody*)b)->ApplyImpulse(hmm_to_ne_v3(impulse), hmm_to_ne_v3(pos));
    }

    void ne_rigid_body_apply_twist(ne_RigidBody b, HMM_Vec3 twist) {
        ((neRigidBody*)b)->ApplyTwist(hmm_to_ne_v3(twist));
    }

    void ne_rigid_body_set_linear_damping(ne_RigidBody b, float damping) {
        ((neRigidBody*)b)->SetLinearDamping(damping);
    }

    float ne_rigid_body_get_linear_damping(ne_RigidBody b) {
        return ((neRigidBody*)b)->GetLinearDamping();
    }

    void ne_rigid_body_set_angular_damping(ne_RigidBody b, float damping) {
        ((neRigidBody*)b)->SetAngularDamping(damping);
    }

    float ne_rigid_body_get_angular_damping(ne_RigidBody b) {
        return ((neRigidBody*)b)->GetAngularDamping();
    }

    void ne_rigid_body_set_sleeping_param(ne_RigidBody b, float param) {
        ((neRigidBody*)b)->SetSleepingParameter(param);
    }

    float ne_rigid_body_get_sleeping_param(ne_RigidBody b) {
        return ((neRigidBody*)b)->GetSleepingParameter();
    }

    void ne_rigid_body_set_collision_id(ne_RigidBody b, int32_t id) {
        ((neRigidBody*)b)->SetCollisionID(id);
    }

    int32_t ne_rigid_body_get_collision_id(ne_RigidBody b) {
        return ((neRigidBody*)b)->GetCollisionID();
    }

    void ne_rigid_body_set_user_data(ne_RigidBody b, uintptr_t data) {
        ((neRigidBody*)b)->SetUserData(data);
    }

    uintptr_t ne_rigid_body_get_user_data(ne_RigidBody b) {
        return ((neRigidBody*)b)->GetUserData();
    }

    ne_Geom ne_rigid_body_add_geom(ne_RigidBody b) {
        return (ne_Geom)((neRigidBody*)b)->AddGeometry();
    }

    bool ne_rigid_body_remove_geom(ne_RigidBody b, ne_Geom g) {
        return ((neRigidBody*)b)->RemoveGeometry((neGeometry*)g) != 0;
    }

    int32_t ne_rigid_body_get_geom_count(ne_RigidBody b) {
        return ((neRigidBody*)b)->GetGeometryCount();
    }

    ne_Sensor ne_rigid_body_add_sensor(ne_RigidBody b) {
        return (ne_Sensor)((neRigidBody*)b)->AddSensor();
    }

    bool ne_rigid_body_remove_sensor(ne_RigidBody b, ne_Sensor s) {
        return ((neRigidBody*)b)->RemoveSensor((neSensor*)s) != 0;
    }

    ne_RigidBodyController ne_rigid_body_add_controller(ne_RigidBody b, ne_RigidBodyControllerCB cb, int32_t period) {
        return (ne_RigidBodyController)((neRigidBody*)b)->AddController((neRigidBodyControllerCallback*)cb, period);
    }

    bool ne_rigid_body_remove_controller(ne_RigidBody b, ne_RigidBodyController ctrl) {
        return ((neRigidBody*)b)->RemoveController((neRigidBodyController*)ctrl) != 0;
    }

    void ne_rigid_body_set_active(ne_RigidBody b, bool active) {
        ((neRigidBody*)b)->Active(active, (neRigidBody*)NULL);
    }

    bool ne_rigid_body_is_active(ne_RigidBody b) {
        return ((neRigidBody*)b)->Active() != 0;
    }

    bool ne_rigid_body_is_idle(ne_RigidBody b) {
        return ((neRigidBody*)b)->IsIdle() != 0;
    }

    void ne_rigid_body_gravity_enable(ne_RigidBody b, bool enable) {
        ((neRigidBody*)b)->GravityEnable(enable);
    }

    bool ne_rigid_body_gravity_enabled(ne_RigidBody b) {
        return ((neRigidBody*)b)->GravityEnable() != 0;
    }

    void ne_rigid_body_collide_connected(ne_RigidBody b, bool enable) {
        ((neRigidBody*)b)->CollideConnected(enable);
    }

    bool ne_rigid_body_is_collide_connected(ne_RigidBody b) {
        return ((neRigidBody*)b)->CollideConnected() != 0;
    }

    void ne_rigid_body_update_bounding_info(ne_RigidBody b) {
        ((neRigidBody*)b)->UpdateBoundingInfo();
    }

    //--ANIMATED BODY--------------------------------------------------------------

    HMM_Vec3 ne_anim_body_get_pos(ne_AnimBody b) {
        return ne_to_hmm_vec3(((neAnimatedBody*)b)->GetPos());
    }

    void ne_anim_body_set_pos(ne_AnimBody b, HMM_Vec3 pos) {
        ((neAnimatedBody*)b)->SetPos(hmm_to_ne_v3(pos));
    }

    HMM_Quat ne_anim_body_get_rot(ne_AnimBody b) {
        return ne_to_hmm_quat(((neAnimatedBody*)b)->GetRotationQ());
    }

    void ne_anim_body_set_rot(ne_AnimBody b, HMM_Quat rot) {
        ((neAnimatedBody*)b)->SetRotation(hmm_to_ne_q(rot));
    }

    void ne_anim_body_set_collision_id(ne_AnimBody b, int32_t id) {
        ((neAnimatedBody*)b)->SetCollisionID(id);
    }

    int32_t ne_anim_body_get_collision_id(ne_AnimBody b) {
        return ((neAnimatedBody*)b)->GetCollisionID();
    }

    void ne_anim_body_set_user_data(ne_AnimBody b, uintptr_t data) {
        ((neAnimatedBody*)b)->SetUserData(data);
    }

    uintptr_t ne_anim_body_get_user_data(ne_AnimBody b) {
        return ((neAnimatedBody*)b)->GetUserData();
    }

    ne_Geom ne_anim_body_add_geom(ne_AnimBody b) {
        return (ne_Geom)((neAnimatedBody*)b)->AddGeometry();
    }

    bool ne_anim_body_remove_geom(ne_AnimBody b, ne_Geom g) {
        return ((neAnimatedBody*)b)->RemoveGeometry((neGeometry*)g) != 0;
    }

    int32_t ne_anim_body_get_geom_count(ne_AnimBody b) {
        return ((neAnimatedBody*)b)->GetGeometryCount();
    }

    ne_Sensor ne_anim_body_add_sensor(ne_AnimBody b) {
        return (ne_Sensor)((neAnimatedBody*)b)->AddSensor();
    }

    bool ne_anim_body_remove_sensor(ne_AnimBody b, ne_Sensor s) {
        return ((neAnimatedBody*)b)->RemoveSensor((neSensor*)s) != 0;
    }

    void ne_anim_body_set_active(ne_AnimBody b, bool active) {
        ((neAnimatedBody*)b)->Active(active, (neAnimatedBody*)NULL);
    }

    bool ne_anim_body_is_active(ne_AnimBody b) {
        return ((neAnimatedBody*)b)->Active() != 0;
    }

    void ne_anim_body_collide_connected(ne_AnimBody b, bool enable) {
        ((neAnimatedBody*)b)->CollideConnected(enable);
    }

    bool ne_anim_body_is_collide_connected(ne_AnimBody b) {
        return ((neAnimatedBody*)b)->CollideConnected() != 0;
    }

    void ne_anim_body_update_bounding_info(ne_AnimBody b) {
        ((neAnimatedBody*)b)->UpdateBoundingInfo();
    }

    //--SENSOR---------------------------------------------------------------------

    void ne_sensor_set_line(ne_Sensor s, HMM_Vec3 pos, HMM_Vec3 line_vec) {
        ((neSensor*)s)->SetLineSensor(hmm_to_ne_v3(pos), hmm_to_ne_v3(line_vec));
    }

    void ne_sensor_set_user_data(ne_Sensor s, uintptr_t data) {
        ((neSensor*)s)->SetUserData(data);
    }

    uintptr_t ne_sensor_get_user_data(ne_Sensor s) {
        return ((neSensor*)s)->GetUserData();
    }

    HMM_Vec3 ne_sensor_get_line_vec(ne_Sensor s) {
        return ne_to_hmm_vec3(((neSensor*)s)->GetLineVector());
    }

    HMM_Vec3 ne_sensor_get_line_unit_vec(ne_Sensor s) {
        return ne_to_hmm_vec3(((neSensor*)s)->GetLineUnitVector());
    }

    HMM_Vec3 ne_sensor_get_line_pos(ne_Sensor s) {
        return ne_to_hmm_vec3(((neSensor*)s)->GetLinePos());
    }

    float ne_sensor_get_detect_depth(ne_Sensor s) {
        return ((neSensor*)s)->GetDetectDepth();
    }

    HMM_Vec3 ne_sensor_get_detect_normal(ne_Sensor s) {
        return ne_to_hmm_vec3(((neSensor*)s)->GetDetectNormal());
    }

    HMM_Vec3 ne_sensor_get_detect_contact_point(ne_Sensor s) {
        return ne_to_hmm_vec3(((neSensor*)s)->GetDetectContactPoint());
    }

    ne_RigidBody ne_sensor_get_detect_rigid_body(ne_Sensor s) {
        return (ne_RigidBody)((neSensor*)s)->GetDetectRigidBody();
    }

    ne_AnimBody ne_sensor_get_detect_anim_body(ne_Sensor s) {
        return (ne_AnimBody)((neSensor*)s)->GetDetectAnimatedBody();
    }

    int32_t ne_sensor_get_detect_material(ne_Sensor s) {
        return ((neSensor*)s)->GetDetectMaterial();
    }

    //--GEOMETRY-------------------------------------------------------------------

    void ne_geom_set_box_size(ne_Geom g, float width, float height, float depth) {
        ((neGeometry*)g)->SetBoxSize(width, height, depth);
    }

    void ne_geom_set_box_size_vec(ne_Geom g, HMM_Vec3 size) {
        ((neGeometry*)g)->SetBoxSize(hmm_to_ne_v3(size));
    }

    bool ne_geom_get_box_size(ne_Geom g, HMM_Vec3* out_size) {
        neV3 size;
        neBool result = ((neGeometry*)g)->GetBoxSize(size);
        if (result && out_size) {
            *out_size = ne_to_hmm_vec3(size);
        }
        return result != 0;
    }

    void ne_geom_set_sphere_diameter(ne_Geom g, float diameter) {
        ((neGeometry*)g)->SetSphereDiameter(diameter);
    }

    bool ne_geom_get_sphere_diameter(ne_Geom g, float* out_diameter) {
        float diam;
        neBool result = ((neGeometry*)g)->GetSphereDiameter(diam);
        if (result && out_diameter) {
            *out_diameter = diam;
        }
        return result != 0;
    }

    void ne_geom_set_cylinder(ne_Geom g, float diameter, float height) {
        ((neGeometry*)g)->SetCylinder(diameter, height);
    }

    bool ne_geom_get_cylinder(ne_Geom g, float* out_diameter, float* out_height) {
        float diam, h;
        neBool result = ((neGeometry*)g)->GetCylinder(diam, h);
        if (result) {
            if (out_diameter) *out_diameter = diam;
            if (out_height) *out_height = h;
        }
        return result != 0;
    }

    void ne_geom_set_material_index(ne_Geom g, int32_t index) {
        ((neGeometry*)g)->SetMaterialIndex(index);
    }

    int32_t ne_geom_get_material_index(ne_Geom g) {
        return ((neGeometry*)g)->GetMaterialIndex();
    }

    void ne_geom_set_user_data(ne_Geom g, uintptr_t data) {
        ((neGeometry*)g)->SetUserData(data);
    }

    uintptr_t ne_geom_get_user_data(ne_Geom g) {
        return ((neGeometry*)g)->GetUserData();
    }

    //--GEOMETRY (ADDITIONS)-------------------------------------------------------

    void ne_geom_set_transform(ne_Geom g, HMM_Mat4 transform) {
        neT3 trs = hmm_to_ne_t3(transform);
        ((neGeometry*)g)->SetTransform(trs);
    }

    HMM_Mat4 ne_geom_get_transform(ne_Geom g) {
        return ne_to_hmm_mat4(((neGeometry*)g)->GetTransform());
    }

    void ne_geom_set_convex_mesh(ne_Geom g, void* convex_data) {
        ((neGeometry*)g)->SetConvexMesh((neByte*)convex_data);
    }

    bool ne_geom_get_convex_mesh(ne_Geom g, void** out_convex_data) {
        neByte* data;
        neBool result = ((neGeometry*)g)->GetConvexMesh(data);
        if (result && out_convex_data) {
            *out_convex_data = data;
        }
        return result != 0;
    }

    void ne_geom_set_breakage_flag(ne_Geom g, ne_BreakFlag flag) {
        ((neGeometry*)g)->SetBreakageFlag((neGeometry::neBreakFlag)flag);
    }

    ne_BreakFlag ne_geom_get_breakage_flag(ne_Geom g) {
        return (ne_BreakFlag)((neGeometry*)g)->GetBreakageFlag();
    }

    void ne_geom_set_breakage_mass(ne_Geom g, float mass) {
        ((neGeometry*)g)->SetBreakageMass(mass);
    }

    float ne_geom_get_breakage_mass(ne_Geom g) {
        return ((neGeometry*)g)->GetBreakageMass();
    }

    void ne_geom_set_breakage_inertia_tensor(ne_Geom g, HMM_Vec3 tensor) {
        ((neGeometry*)g)->SetBreakageInertiaTensor(hmm_to_ne_v3(tensor));
    }

    HMM_Vec3 ne_geom_get_breakage_inertia_tensor(ne_Geom g) {
        return ne_to_hmm_vec3(((neGeometry*)g)->GetBreakageInertiaTensor());
    }

    void ne_geom_set_breakage_magnitude(ne_Geom g, float mag) {
        ((neGeometry*)g)->SetBreakageMagnitude(mag);
    }

    float ne_geom_get_breakage_magnitude(ne_Geom g) {
        return ((neGeometry*)g)->GetBreakageMagnitude();
    }

    void ne_geom_set_breakage_absorption(ne_Geom g, float absorb) {
        ((neGeometry*)g)->SetBreakageAbsorption(absorb);
    }

    float ne_geom_get_breakage_absorption(ne_Geom g) {
        return ((neGeometry*)g)->GetBreakageAbsorption();
    }

    void ne_geom_set_breakage_plane(ne_Geom g, HMM_Vec3 plane_normal) {
        ((neGeometry*)g)->SetBreakagePlane(hmm_to_ne_v3(plane_normal));
    }

    HMM_Vec3 ne_geom_get_breakage_plane(ne_Geom g) {
        return ne_to_hmm_vec3(((neGeometry*)g)->GetBreakagePlane());
    }

    void ne_geom_set_breakage_neighbour_radius(ne_Geom g, float radius) {
        ((neGeometry*)g)->SetBreakageNeighbourRadius(radius);
    }

    float ne_geom_get_breakage_neighbour_radius(ne_Geom g) {
        return ((neGeometry*)g)->GetBreakageNeighbourRadius();
    }

    //--CONTROLLER CALLBACK WRAPPERS-----------------------------------------------

    ne_RigidBodyControllerCB ne_create_rigid_body_controller_cb(ne_rigid_body_controller_fn fn, void* userdata) {
        return (ne_RigidBodyControllerCB)(new RigidBodyControllerCB(fn, userdata));
    }

    void ne_destroy_rigid_body_controller_cb(ne_RigidBodyControllerCB cb) {
        delete (RigidBodyControllerCB*)cb;
    }

    ne_JointControllerCB ne_create_joint_controller_cb(ne_joint_controller_fn fn, void* userdata) {
        return (ne_JointControllerCB)(new JointControllerCB(fn, userdata));
    }

    void ne_destroy_joint_controller_cb(ne_JointControllerCB cb) {
        delete (JointControllerCB*)cb;
    }

    //--RIGID BODY (ADDITIONS)-----------------------------------------------------

    HMM_Mat4 ne_rigid_body_get_transform(ne_RigidBody b) {
        return ne_to_hmm_mat4(((neRigidBody*)b)->GetTransform());
    }

    void ne_rigid_body_set_inertia_tensor_mat(ne_RigidBody b, HMM_Mat4 tensor) {
        ((neRigidBody*)b)->SetInertiaTensor(hmm_to_ne_m3(tensor));
    }

    void ne_rigid_body_update_inertia_tensor(ne_RigidBody b) {
        ((neRigidBody*)b)->UpdateInertiaTensor();
    }

    HMM_Vec3 ne_rigid_body_get_angular_momentum(ne_RigidBody b) {
        return ne_to_hmm_vec3(((neRigidBody*)b)->GetAngularMomentum());
    }

    void ne_rigid_body_set_angular_momentum(ne_RigidBody b, HMM_Vec3 am) {
        ((neRigidBody*)b)->SetAngularMomentum(hmm_to_ne_v3(am));
    }

    void ne_rigid_body_begin_iterate_geom(ne_RigidBody b) {
        ((neRigidBody*)b)->BeginIterateGeometry();
    }

    ne_Geom ne_rigid_body_get_next_geom(ne_RigidBody b) {
        return (ne_Geom)((neRigidBody*)b)->GetNextGeometry();
    }

    ne_RigidBody ne_rigid_body_break_geom(ne_RigidBody b, ne_Geom g) {
        return (ne_RigidBody)((neRigidBody*)b)->BreakGeometry((neGeometry*)g);
    }

    void ne_rigid_body_begin_iterate_sensor(ne_RigidBody b) {
        ((neRigidBody*)b)->BeginIterateSensor();
    }

    ne_Sensor ne_rigid_body_get_next_sensor(ne_RigidBody b) {
        return (ne_Sensor)((neRigidBody*)b)->GetNextSensor();
    }

    void ne_rigid_body_begin_iterate_controller(ne_RigidBody b) {
        ((neRigidBody*)b)->BeginIterateController();
    }

    ne_RigidBodyController ne_rigid_body_get_next_controller(ne_RigidBody b) {
        return (ne_RigidBodyController)((neRigidBody*)b)->GetNextController();
    }

    void ne_rigid_body_use_custom_cd(ne_RigidBody b, bool yes, HMM_Mat4* obb, float bounding_radius) {
        neT3* obb_ptr = NULL;
        neT3 obb_t3;
        if (obb) {
            obb_t3 = hmm_to_ne_t3(*obb);
            obb_ptr = &obb_t3;
        }
        ((neRigidBody*)b)->UseCustomCollisionDetection(yes, obb_ptr, bounding_radius);
    }

    bool ne_rigid_body_is_using_custom_cd(ne_RigidBody b) {
        return ((neRigidBody*)b)->UseCustomCollisionDetection() != 0;
    }

    void ne_rigid_body_collide_directly_connected(ne_RigidBody b, bool enable) {
        ((neRigidBody*)b)->CollideDirectlyConnected(enable);
    }

    bool ne_rigid_body_is_collide_directly_connected(ne_RigidBody b) {
        return ((neRigidBody*)b)->CollideDirectlyConnected() != 0;
    }

    //--ANIMATED BODY (ADDITIONS)--------------------------------------------------

    HMM_Mat4 ne_anim_body_get_transform(ne_AnimBody b) {
        return ne_to_hmm_mat4(((neAnimatedBody*)b)->GetTransform());
    }

    void ne_anim_body_begin_iterate_geom(ne_AnimBody b) {
        ((neAnimatedBody*)b)->BeginIterateGeometry();
    }

    ne_Geom ne_anim_body_get_next_geom(ne_AnimBody b) {
        return (ne_Geom)((neAnimatedBody*)b)->GetNextGeometry();
    }

    ne_RigidBody ne_anim_body_break_geom(ne_AnimBody b, ne_Geom g) {
        return (ne_RigidBody)((neAnimatedBody*)b)->BreakGeometry((neGeometry*)g);
    }

    void ne_anim_body_begin_iterate_sensor(ne_AnimBody b) {
        ((neAnimatedBody*)b)->BeginIterateSensor();
    }

    ne_Sensor ne_anim_body_get_next_sensor(ne_AnimBody b) {
        return (ne_Sensor)((neAnimatedBody*)b)->GetNextSensor();
    }

    void ne_anim_body_use_custom_cd(ne_AnimBody b, bool yes, HMM_Mat4* obb, float bounding_radius) {
        neT3* obb_ptr = NULL;
        neT3 obb_t3;
        if (obb) {
            obb_t3 = hmm_to_ne_t3(*obb);
            obb_ptr = &obb_t3;
        }
        ((neAnimatedBody*)b)->UseCustomCollisionDetection(yes, obb_ptr, bounding_radius);
    }

    bool ne_anim_body_is_using_custom_cd(ne_AnimBody b) {
        return ((neAnimatedBody*)b)->UseCustomCollisionDetection() != 0;
    }

    void ne_anim_body_collide_directly_connected(ne_AnimBody b, bool enable) {
        ((neAnimatedBody*)b)->CollideDirectlyConnected(enable);
    }

    bool ne_anim_body_is_collide_directly_connected(ne_AnimBody b) {
        return ((neAnimatedBody*)b)->CollideDirectlyConnected() != 0;
    }

    //--RIGID BODY CONTROLLER------------------------------------------------------

    ne_RigidBody ne_rigid_body_controller_get_rigid_body(ne_RigidBodyController ctrl) {
        return (ne_RigidBody)((neRigidBodyController*)ctrl)->GetRigidBody();
    }

    HMM_Vec3 ne_rigid_body_controller_get_force(ne_RigidBodyController ctrl) {
        return ne_to_hmm_vec3(((neRigidBodyController*)ctrl)->GetControllerForce());
    }

    HMM_Vec3 ne_rigid_body_controller_get_torque(ne_RigidBodyController ctrl) {
        return ne_to_hmm_vec3(((neRigidBodyController*)ctrl)->GetControllerTorque());
    }

    void ne_rigid_body_controller_set_force(ne_RigidBodyController ctrl, HMM_Vec3 force) {
        ((neRigidBodyController*)ctrl)->SetControllerForce(hmm_to_ne_v3(force));
    }

    void ne_rigid_body_controller_set_torque(ne_RigidBodyController ctrl, HMM_Vec3 torque) {
        ((neRigidBodyController*)ctrl)->SetControllerTorque(hmm_to_ne_v3(torque));
    }

    void ne_rigid_body_controller_set_force_with_torque(ne_RigidBodyController ctrl, HMM_Vec3 force, HMM_Vec3 pos) {
        ((neRigidBodyController*)ctrl)->SetControllerForceWithTorque(hmm_to_ne_v3(force), hmm_to_ne_v3(pos));
    }

    //--JOINT----------------------------------------------------------------------

    void ne_joint_set_type(ne_Joint joint, ne_JointType type) {
        ((neJoint*)joint)->SetType((neJoint::ConstraintType)type);
    }

    ne_JointType ne_joint_get_type(ne_Joint joint) {
        return (ne_JointType)((neJoint*)joint)->GetType();
    }

    void ne_joint_set_frame_a(ne_Joint joint, HMM_Mat4 frame) {
        ((neJoint*)joint)->SetJointFrameA(hmm_to_ne_t3(frame));
    }

    void ne_joint_set_frame_b(ne_Joint joint, HMM_Mat4 frame) {
        ((neJoint*)joint)->SetJointFrameB(hmm_to_ne_t3(frame));
    }

    void ne_joint_set_frame_world(ne_Joint joint, HMM_Mat4 frame) {
        ((neJoint*)joint)->SetJointFrameWorld(hmm_to_ne_t3(frame));
    }

    HMM_Mat4 ne_joint_get_frame_a(ne_Joint joint) {
        return ne_to_hmm_mat4(((neJoint*)joint)->GetJointFrameA());
    }

    HMM_Mat4 ne_joint_get_frame_b(ne_Joint joint) {
        return ne_to_hmm_mat4(((neJoint*)joint)->GetJointFrameB());
    }

    void ne_joint_set_length(ne_Joint joint, float length) {
        ((neJoint*)joint)->SetJointLength(length);
    }

    float ne_joint_get_length(ne_Joint joint) {
        return ((neJoint*)joint)->GetJointLength();
    }

    ne_RigidBody ne_joint_get_rigid_body_a(ne_Joint joint) {
        return (ne_RigidBody)((neJoint*)joint)->GetRigidBodyA();
    }

    ne_RigidBody ne_joint_get_rigid_body_b(ne_Joint joint) {
        return (ne_RigidBody)((neJoint*)joint)->GetRigidBodyB();
    }

    ne_AnimBody ne_joint_get_anim_body_b(ne_Joint joint) {
        return (ne_AnimBody)((neJoint*)joint)->GetAnimatedBodyB();
    }

    void ne_joint_enable(ne_Joint joint, bool enable) {
        ((neJoint*)joint)->Enable(enable);
    }

    bool ne_joint_is_enabled(ne_Joint joint) {
        return ((neJoint*)joint)->Enable() != 0;
    }

    void ne_joint_set_damping_factor(ne_Joint joint, float damp) {
        ((neJoint*)joint)->SetDampingFactor(damp);
    }

    float ne_joint_get_damping_factor(ne_Joint joint) {
        return ((neJoint*)joint)->GetDampingFactor();
    }

    bool ne_joint_is_limit_enabled(ne_Joint joint) {
        return ((neJoint*)joint)->EnableLimit() != 0;
    }

    void ne_joint_enable_limit(ne_Joint joint, bool enable) {
        ((neJoint*)joint)->EnableLimit(enable);
    }

    float ne_joint_get_upper_limit(ne_Joint joint) {
        return ((neJoint*)joint)->GetUpperLimit();
    }

    void ne_joint_set_upper_limit(ne_Joint joint, float limit) {
        ((neJoint*)joint)->SetUpperLimit(limit);
    }

    float ne_joint_get_lower_limit(ne_Joint joint) {
        return ((neJoint*)joint)->GetLowerLimit();
    }

    void ne_joint_set_lower_limit(ne_Joint joint, float limit) {
        ((neJoint*)joint)->SetLowerLimit(limit);
    }

    bool ne_joint_is_limit2_enabled(ne_Joint joint) {
        return ((neJoint*)joint)->EnableLimit2() != 0;
    }

    void ne_joint_enable_limit2(ne_Joint joint, bool enable) {
        ((neJoint*)joint)->EnableLimit2(enable);
    }

    float ne_joint_get_upper_limit2(ne_Joint joint) {
        return ((neJoint*)joint)->GetUpperLimit2();
    }

    void ne_joint_set_upper_limit2(ne_Joint joint, float limit) {
        ((neJoint*)joint)->SetUpperLimit2(limit);
    }

    float ne_joint_get_lower_limit2(ne_Joint joint) {
        return ((neJoint*)joint)->GetLowerLimit2();
    }

    void ne_joint_set_lower_limit2(ne_Joint joint, float limit) {
        ((neJoint*)joint)->SetLowerLimit2(limit);
    }

    void ne_joint_set_epsilon(ne_Joint joint, float epsilon) {
        ((neJoint*)joint)->SetEpsilon(epsilon);
    }

    float ne_joint_get_epsilon(ne_Joint joint) {
        return ((neJoint*)joint)->GetEpsilon();
    }

    void ne_joint_set_iteration(ne_Joint joint, int32_t iteration) {
        ((neJoint*)joint)->SetIteration(iteration);
    }

    int32_t ne_joint_get_iteration(ne_Joint joint) {
        return ((neJoint*)joint)->GetIteration();
    }

    ne_JointController ne_joint_add_controller(ne_Joint joint, ne_JointControllerCB cb, int32_t period) {
        return (ne_JointController)((neJoint*)joint)->AddController((neJointControllerCallback*)cb, period);
    }

    bool ne_joint_remove_controller(ne_Joint joint, ne_JointController ctrl) {
        return ((neJoint*)joint)->RemoveController((neJointController*)ctrl) != 0;
    }

    void ne_joint_begin_iterate_controller(ne_Joint joint) {
        ((neJoint*)joint)->BeginIterateController();
    }

    ne_JointController ne_joint_get_next_controller(ne_Joint joint) {
        return (ne_JointController)((neJoint*)joint)->GetNextController();
    }

    bool ne_joint_is_motor_enabled(ne_Joint joint) {
        return ((neJoint*)joint)->EnableMotor() != 0;
    }

    void ne_joint_enable_motor(ne_Joint joint, bool enable) {
        ((neJoint*)joint)->EnableMotor(enable);
    }

    void ne_joint_set_motor(ne_Joint joint, ne_MotorType type, float desire_value, float max_force) {
        ((neJoint*)joint)->SetMotor((neJoint::MotorType)type, desire_value, max_force);
    }

    void ne_joint_get_motor(ne_Joint joint, ne_MotorType* out_type, float* out_desire_value, float* out_max_force) {
        neJoint::MotorType type;
        float desire, max;
        ((neJoint*)joint)->GetMotor(type, desire, max);
        if (out_type) *out_type = (ne_MotorType)type;
        if (out_desire_value) *out_desire_value = desire;
        if (out_max_force) *out_max_force = max;
    }

    bool ne_joint_is_motor2_enabled(ne_Joint joint) {
        return ((neJoint*)joint)->EnableMotor2() != 0;
    }

    void ne_joint_enable_motor2(ne_Joint joint, bool enable) {
        ((neJoint*)joint)->EnableMotor2(enable);
    }

    void ne_joint_set_motor2(ne_Joint joint, ne_MotorType type, float desire_value, float max_force) {
        ((neJoint*)joint)->SetMotor2((neJoint::MotorType)type, desire_value, max_force);
    }

    void ne_joint_get_motor2(ne_Joint joint, ne_MotorType* out_type, float* out_desire_value, float* out_max_force) {
        neJoint::MotorType type;
        float desire, max;
        ((neJoint*)joint)->GetMotor2(type, desire, max);
        if (out_type) *out_type = (ne_MotorType)type;
        if (out_desire_value) *out_desire_value = desire;
        if (out_max_force) *out_max_force = max;
    }

    //--JOINT CONTROLLER-----------------------------------------------------------

    ne_Joint ne_joint_controller_get_joint(ne_JointController ctrl) {
        return (ne_Joint)((neJointController*)ctrl)->GetJoint();
    }

    HMM_Vec3 ne_joint_controller_get_force_body_a(ne_JointController ctrl) {
        return ne_to_hmm_vec3(((neJointController*)ctrl)->GetControllerForceBodyA());
    }

    HMM_Vec3 ne_joint_controller_get_force_body_b(ne_JointController ctrl) {
        return ne_to_hmm_vec3(((neJointController*)ctrl)->GetControllerForceBodyB());
    }

    HMM_Vec3 ne_joint_controller_get_torque_body_a(ne_JointController ctrl) {
        return ne_to_hmm_vec3(((neJointController*)ctrl)->GetControllerTorqueBodyA());
    }

    HMM_Vec3 ne_joint_controller_get_torque_body_b(ne_JointController ctrl) {
        return ne_to_hmm_vec3(((neJointController*)ctrl)->GetControllerTorqueBodyB());
    }

    void ne_joint_controller_set_force_body_a(ne_JointController ctrl, HMM_Vec3 force) {
        ((neJointController*)ctrl)->SetControllerForceBodyA(hmm_to_ne_v3(force));
    }

    void ne_joint_controller_set_force_body_b(ne_JointController ctrl, HMM_Vec3 force) {
        ((neJointController*)ctrl)->SetControllerForceBodyB(hmm_to_ne_v3(force));
    }

    void ne_joint_controller_set_force_with_torque_body_a(ne_JointController ctrl, HMM_Vec3 force, HMM_Vec3 pos) {
        ((neJointController*)ctrl)->SetControllerForceWithTorqueBodyA(hmm_to_ne_v3(force), hmm_to_ne_v3(pos));
    }

    void ne_joint_controller_set_force_with_torque_body_b(ne_JointController ctrl, HMM_Vec3 force, HMM_Vec3 pos) {
        ((neJointController*)ctrl)->SetControllerForceWithTorqueBodyB(hmm_to_ne_v3(force), hmm_to_ne_v3(pos));
    }

    void ne_joint_controller_set_torque_body_a(ne_JointController ctrl, HMM_Vec3 torque) {
        ((neJointController*)ctrl)->SetControllerTorqueBodyA(hmm_to_ne_v3(torque));
    }

    void ne_joint_controller_set_torque_body_b(ne_JointController ctrl, HMM_Vec3 torque) {
        ((neJointController*)ctrl)->SetControllerTorqueBodyB(hmm_to_ne_v3(torque));
    }

    //--COLLISION TABLE------------------------------------------------------------

    void ne_collision_table_set(ne_CollisionTable table, int32_t id1, int32_t id2, ne_CollisionResponse response) {
        ((neCollisionTable*)table)->Set(id1, id2, (neCollisionTable::neReponseBitFlag)response);
    }

    ne_CollisionResponse ne_collision_table_get(ne_CollisionTable table, int32_t id1, int32_t id2) {
        return (ne_CollisionResponse)((neCollisionTable*)table)->Get(id1, id2);
    }

    int32_t ne_collision_table_get_max_id(ne_CollisionTable table) {
        return ((neCollisionTable*)table)->GetMaxCollisionID();
    }

    //--SIMULATOR (ADDITIONS)------------------------------------------------------

    ne_RigidBody ne_sim_create_rigid_particle(ne_Simulator sim) {
        return (ne_RigidBody)((neSimulator*)sim)->CreateRigidParticle();
    }

    ne_CollisionTable ne_sim_get_collision_table(ne_Simulator sim) {
        return (ne_CollisionTable)((neSimulator*)sim)->GetCollisionTable();
    }

    bool ne_sim_set_material(ne_Simulator sim, int32_t index, float friction, float restitution) {
        return ((neSimulator*)sim)->SetMaterial(index, friction, restitution);
    }

    bool ne_sim_get_material(ne_Simulator sim, int32_t index, float* out_friction, float* out_restitution) {
        float friction, restitution;
        bool result = ((neSimulator*)sim)->GetMaterial(index, friction, restitution);
        if (result) {
            if (out_friction) *out_friction = friction;
            if (out_restitution) *out_restitution = restitution;
        }
        return result;
    }

    void ne_sim_set_terrain_mesh(ne_Simulator sim, ne_TriangleMesh* mesh) {
        neTriangleMesh ne_mesh;
        ne_mesh.vertices = (neV3*)mesh->vertices;
        ne_mesh.vertexCount = mesh->vertex_count;
        ne_mesh.triangles = (neTriangle*)mesh->triangles;
        ne_mesh.triangleCount = mesh->triangle_count;
        ((neSimulator*)sim)->SetTerrainMesh(&ne_mesh);
    }

    void ne_sim_free_terrain_mesh(ne_Simulator sim) {
        ((neSimulator*)sim)->FreeTerrainMesh();
    }

    ne_Joint ne_sim_create_joint_rb(ne_Simulator sim, ne_RigidBody body_a) {
        return (ne_Joint)((neSimulator*)sim)->CreateJoint((neRigidBody*)body_a);
    }

    ne_Joint ne_sim_create_joint_rb_rb(ne_Simulator sim, ne_RigidBody body_a, ne_RigidBody body_b) {
        return (ne_Joint)((neSimulator*)sim)->CreateJoint((neRigidBody*)body_a, (neRigidBody*)body_b);
    }

    ne_Joint ne_sim_create_joint_rb_ab(ne_Simulator sim, ne_RigidBody body_a, ne_AnimBody body_b) {
        return (ne_Joint)((neSimulator*)sim)->CreateJoint((neRigidBody*)body_a, (neAnimatedBody*)body_b);
    }

    void ne_sim_free_joint(ne_Simulator sim, ne_Joint joint) {
        ((neSimulator*)sim)->FreeJoint((neJoint*)joint);
    }

    void ne_sim_set_breakage_callback(ne_Simulator sim, ne_breakage_callback* cb) {
        ((neSimulator*)sim)->SetBreakageCallback((neBreakageCallback*)cb);
    }

    ne_breakage_callback* ne_sim_get_breakage_callback(ne_Simulator sim) {
        return (ne_breakage_callback*)((neSimulator*)sim)->GetBreakageCallback();
    }

    void ne_sim_set_collision_callback(ne_Simulator sim, ne_collision_callback* cb) {
        ((neSimulator*)sim)->SetCollisionCallback((neCollisionCallback*)cb);
    }

    ne_collision_callback* ne_sim_get_collision_callback(ne_Simulator sim) {
        return (ne_collision_callback*)((neSimulator*)sim)->GetCollisionCallback();
    }

    void ne_sim_set_terrain_query_callback(ne_Simulator sim, ne_terrain_query_callback* cb) {
        ((neSimulator*)sim)->SetTerrainTriangleQueryCallback((neTerrainTriangleQueryCallback*)cb);
    }

    ne_terrain_query_callback* ne_sim_get_terrain_query_callback(ne_Simulator sim) {
        return (ne_terrain_query_callback*)((neSimulator*)sim)->GetTerrainTriangleQueryCallback();
    }

    void ne_sim_set_custom_cd_rb2rb_callback(ne_Simulator sim, ne_custom_cd_rb2rb_callback* cb) {
        ((neSimulator*)sim)->SetCustomCDRB2RBCallback((neCustomCDRB2RBCallback*)cb);
    }

    ne_custom_cd_rb2rb_callback* ne_sim_get_custom_cd_rb2rb_callback(ne_Simulator sim) {
        return (ne_custom_cd_rb2rb_callback*)((neSimulator*)sim)->GetCustomCDRB2RBCallback();
    }

    void ne_sim_set_custom_cd_rb2ab_callback(ne_Simulator sim, ne_custom_cd_rb2ab_callback* cb) {
        ((neSimulator*)sim)->SetCustomCDRB2ABCallback((neCustomCDRB2ABCallback*)cb);
    }

    ne_custom_cd_rb2ab_callback* ne_sim_get_custom_cd_rb2ab_callback(ne_Simulator sim) {
        return (ne_custom_cd_rb2ab_callback*)((neSimulator*)sim)->GetCustomCDRB2ABCallback();
    }

    void ne_sim_set_log_callback(ne_Simulator sim, ne_log_callback* cb) {
        ((neSimulator*)sim)->SetLogOutputCallback((neLogOutputCallback*)cb);
    }

    ne_log_callback* ne_sim_get_log_callback(ne_Simulator sim) {
        return (ne_log_callback*)((neSimulator*)sim)->GetLogOutputCallback();
    }

    void ne_sim_set_log_level(ne_Simulator sim, ne_LogLevel level) {
        ((neSimulator*)sim)->SetLogOutputLevel((neSimulator::LOG_OUTPUT_LEVEL)level);
    }

    ne_SimSizeInfo ne_sim_get_current_size_info(ne_Simulator sim) {
        neSimulatorSizeInfo info = ((neSimulator*)sim)->GetCurrentSizeInfo();
        ne_SimSizeInfo result;
        result.rigid_bodies_count = info.rigidBodiesCount;
        result.animated_bodies_count = info.animatedBodiesCount;
        result.rigid_particle_count = info.rigidParticleCount;
        result.controllers_count = info.controllersCount;
        result.overlapped_pairs_count = info.overlappedPairsCount;
        result.geometries_count = info.geometriesCount;
        result.constraints_count = info.constraintsCount;
        result.constraint_sets_count = info.constraintSetsCount;
        result.constraint_buffer_size = info.constraintBufferSize;
        result.sensors_count = info.sensorsCount;
        result.terrain_nodes_start_count = info.terrainNodesStartCount;
        result.terrain_nodes_growby_count = info.terrainNodesGrowByCount;
        return result;
    }

    ne_SimSizeInfo ne_sim_get_start_size_info(ne_Simulator sim) {
        neSimulatorSizeInfo info = ((neSimulator*)sim)->GetStartSizeInfo();
        ne_SimSizeInfo result;
        result.rigid_bodies_count = info.rigidBodiesCount;
        result.animated_bodies_count = info.animatedBodiesCount;
        result.rigid_particle_count = info.rigidParticleCount;
        result.controllers_count = info.controllersCount;
        result.overlapped_pairs_count = info.overlappedPairsCount;
        result.geometries_count = info.geometriesCount;
        result.constraints_count = info.constraintsCount;
        result.constraint_sets_count = info.constraintSetsCount;
        result.constraint_buffer_size = info.constraintBufferSize;
        result.sensors_count = info.sensorsCount;
        result.terrain_nodes_start_count = info.terrainNodesStartCount;
        result.terrain_nodes_growby_count = info.terrainNodesGrowByCount;
        return result;
    }

    void ne_sim_get_memory_allocated(ne_Simulator sim, int32_t* out_memory) {
        int32_t mem;
        ((neSimulator*)sim)->GetMemoryAllocated(mem);
        if (out_memory) *out_memory = mem;
    }

    //--HELPER FUNCTIONS-----------------------------------------------------------

    HMM_Vec3 ne_box_inertia_tensor(float width, float height, float depth, float mass) {
        return ne_to_hmm_vec3(neBoxInertiaTensor(width, height, depth, mass));
    }

    HMM_Vec3 ne_box_inertia_tensor_vec(HMM_Vec3 box_size, float mass) {
        return ne_to_hmm_vec3(neBoxInertiaTensor(hmm_to_ne_v3(box_size), mass));
    }

    HMM_Vec3 ne_sphere_inertia_tensor(float diameter, float mass) {
        return ne_to_hmm_vec3(neSphereInertiaTensor(diameter, mass));
    }

    HMM_Vec3 ne_cylinder_inertia_tensor(float diameter, float height, float mass) {
        return ne_to_hmm_vec3(neCylinderInertiaTensor(diameter, height, mass));
    }
}

#include "tokamak/boxcylinder.cpp"
#include "tokamak/collision.cpp"
#include "tokamak/collisionbody.cpp"
#include "tokamak/constraint.cpp"
#include "tokamak/cylinder.cpp"
#include "tokamak/dcd.cpp"
#include "tokamak/lines.cpp"
#include "tokamak/ne_interface.cpp"
#if defined(__linux__)
#include "tokamak/perflinux.cpp"
#elif defined(_WIN32)
#include "tokamak/perfwin32.cpp"
#endif
#include "tokamak/region.cpp"
#include "tokamak/restcontact.cpp"
#include "tokamak/rigidbody.cpp"
#include "tokamak/rigidbodybase.cpp"
#include "tokamak/scenery.cpp"
#include "tokamak/simulator.cpp"
#include "tokamak/solver.cpp"
#include "tokamak/sphere.cpp"
#include "tokamak/stack.cpp"
#include "tokamak/tricollision.cpp"
