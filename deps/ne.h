#pragma once
#include "hmm.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//c++ gets confused if we declare these as c structs.
typedef void* ne_Simulator;
typedef void* ne_AnimBody;
typedef void* ne_RigidBody;
typedef void* ne_Geom;
typedef void* ne_Sensor;
typedef void* ne_RigidBodyController;
typedef void* ne_RigidBodyControllerCB;
typedef void* ne_JointControllerCB;
typedef void* ne_Joint;
typedef void* ne_JointController;
typedef void* ne_CollisionTable;
typedef void* ne_AllocatorAbstract;

//--ALLOCATOR-------------------------------------------------------------------------

typedef void* (*ne_AllocFn)(size_t size, size_t alignment, void* udata);
typedef void  (*ne_FreeFn)(void* ptr, void* udata);

typedef struct ne_Allocator {
	void* udata;
	ne_AllocFn alloc;
	ne_FreeFn free;
} ne_Allocator;

//--CALLBACKS-------------------------------------------------------------------------

// Callback function pointer for rigid body controllers
typedef void(*ne_rigid_body_controller_fn)(ne_RigidBody body, ne_RigidBodyController controller, float time_step, void* udata);

// Callback function pointer for joint controllers
typedef void(*ne_joint_controller_fn)(ne_JointController controller, float time_step, void* udata);

//--ENUMS AND TYPES-------------------------------

typedef enum {
    NE_BODY_TERRAIN = 0,
    NE_BODY_RIGID_BODY,
    NE_BODY_ANIMATED_BODY,
} ne_BodyType;

typedef enum {
    NE_BREAK_DISABLE = 0,
    NE_BREAK_NORMAL,
    NE_BREAK_ALL,
    NE_BREAK_NEIGHBOUR,
    NE_BREAK_NORMAL_PARTICLE,
    NE_BREAK_ALL_PARTICLE,
    NE_BREAK_NEIGHBOUR_PARTICLE,
} ne_BreakFlag;

typedef enum {
    NE_JOINT_BALLSOCKET = 0,
    NE_JOINT_BALLSOCKET2,
    NE_JOINT_HINGE,
    NE_JOINT_SLIDE,
} ne_JointType;

typedef enum {
    NE_MOTOR_SPEED = 0,
    NE_MOTOR_POSITION,
} ne_MotorType;

typedef enum {
    NE_COLLISION_RESPONSE_IGNORE = 0,
    NE_COLLISION_RESPONSE_IMPULSE = 1,
    NE_COLLISION_RESPONSE_CALLBACK = 2,
    NE_COLLISION_RESPONSE_IMPULSE_CALLBACK = 3,
} ne_CollisionResponse;

typedef enum {
    NE_LOG_LEVEL_NONE = 0,
    NE_LOG_LEVEL_ONE,
    NE_LOG_LEVEL_FULL,
} ne_LogLevel;

// Triangle mesh structures
typedef struct ne_Triangle {
    int32_t indices[3];
    int32_t material_id;
    uint32_t flag;
    uintptr_t user_data;
} ne_Triangle;

typedef struct ne_TriangleMesh {
    HMM_Vec3* vertices;
    int32_t vertex_count;
    ne_Triangle* triangles;
    int32_t triangle_count;
} ne_TriangleMesh;

// Collision info for callbacks
typedef struct ne_CollisionInfo {
    void* body_a;
    void* body_b;
    ne_BodyType type_a;
    ne_BodyType type_b;
    ne_Geom geom_a;
    ne_Geom geom_b;
    int32_t material_id_a;
    int32_t material_id_b;
    HMM_Vec3 body_contact_point_a;
    HMM_Vec3 body_contact_point_b;
    HMM_Vec3 world_contact_point_a;
    HMM_Vec3 world_contact_point_b;
    HMM_Vec3 relative_velocity;
    HMM_Vec3 collision_normal;
} ne_CollisionInfo;

// Custom collision detection info
typedef struct ne_CustomCdInfo {
    HMM_Vec3 collision_normal;
    HMM_Vec3 world_contact_point_a;
    HMM_Vec3 world_contact_point_b;
    float penetration_depth;
    int32_t material_id_a;
    int32_t material_id_b;
} ne_CustomCdInfo;

// Callback function pointers
typedef void (*ne_breakage_callback)(void* original_body, ne_BodyType body_type, ne_Geom broken_geom, ne_RigidBody new_body);
typedef void (*ne_collision_callback)(ne_CollisionInfo* info);
typedef void (*ne_log_callback)(char* log_string);
typedef void (*ne_terrain_query_callback)(HMM_Vec3* min_bound, HMM_Vec3* max_bound, int32_t** candidate_triangles, ne_Triangle** triangles, HMM_Vec3** vertices, int32_t* candidate_count, int32_t* triangle_count, ne_RigidBody body);
typedef bool (*ne_custom_cd_rb2rb_callback)(ne_RigidBody body_a, ne_RigidBody body_b, ne_CustomCdInfo* cd_info);
typedef bool (*ne_custom_cd_rb2ab_callback)(ne_RigidBody body_a, ne_AnimBody body_b, ne_CustomCdInfo* cd_info);

//--SIMULATOR-------------------------------

typedef struct ne_SimSizeInfo {
    int32_t rigid_bodies_count;
    int32_t animated_bodies_count;
    int32_t rigid_particle_count;
    int32_t controllers_count;
    int32_t overlapped_pairs_count;
    int32_t geometries_count;
    int32_t constraints_count;
    int32_t constraint_sets_count;
    int32_t constraint_buffer_size;
    int32_t sensors_count;
    int32_t terrain_nodes_start_count;
    int32_t terrain_nodes_growby_count;
} ne_SimSizeInfo;

typedef struct ne_Desc {
    HMM_Vec3 gravity;
    ne_SimSizeInfo* size_info; // optional, NULL for defaults
    ne_Allocator* allocator; // optional, NULL for default allocator
} ne_Desc;



//--SIMULATOR------------------------------------------------------------------

ne_Simulator ne_create_sim(const ne_Desc* desc);
void ne_destroy_sim(ne_Simulator sim);

ne_RigidBody ne_sim_create_rigid_body(ne_Simulator sim);
ne_AnimBody ne_sim_create_anim_body(ne_Simulator sim);
void ne_sim_free_rigid_body(ne_Simulator sim, ne_RigidBody body);
void ne_sim_free_anim_body(ne_Simulator sim, ne_AnimBody body);

void ne_sim_advance(ne_Simulator sim, float sec, int32_t steps);
void ne_sim_set_gravity(ne_Simulator sim, HMM_Vec3 gravity);
HMM_Vec3 ne_sim_get_gravity(ne_Simulator sim);

//--RIGID BODY-----------------------------------------------------------------
// Physical properties
float ne_rigid_body_get_mass(ne_RigidBody b);
void ne_rigid_body_set_mass(ne_RigidBody b, float mass);
void ne_rigid_body_set_inertia_tensor(ne_RigidBody b, HMM_Vec3 tensor);

// Spatial state
HMM_Vec3 ne_rigid_body_get_pos(ne_RigidBody b);
void ne_rigid_body_set_pos(ne_RigidBody b, HMM_Vec3 pos);
HMM_Quat ne_rigid_body_get_rot(ne_RigidBody b);
void ne_rigid_body_set_rot(ne_RigidBody b, HMM_Quat rot);

// Dynamic state
HMM_Vec3 ne_rigid_body_get_velocity(ne_RigidBody b);
void ne_rigid_body_set_velocity(ne_RigidBody b, HMM_Vec3 vel);
HMM_Vec3 ne_rigid_body_get_angular_velocity(ne_RigidBody b);
HMM_Vec3 ne_rigid_body_get_velocity_at_point(ne_RigidBody b, HMM_Vec3 pt);

// Forces
void ne_rigid_body_set_force(ne_RigidBody b, HMM_Vec3 force);
void ne_rigid_body_set_torque(ne_RigidBody b, HMM_Vec3 torque);
void ne_rigid_body_set_force_with_pos(ne_RigidBody b, HMM_Vec3 force, HMM_Vec3 pos);
HMM_Vec3 ne_rigid_body_get_force(ne_RigidBody b);
HMM_Vec3 ne_rigid_body_get_torque(ne_RigidBody b);

// Impulses
void ne_rigid_body_apply_impulse(ne_RigidBody b, HMM_Vec3 impulse);
void ne_rigid_body_apply_impulse_with_pos(ne_RigidBody b, HMM_Vec3 impulse, HMM_Vec3 pos);
void ne_rigid_body_apply_twist(ne_RigidBody b, HMM_Vec3 twist);

// Properties
void ne_rigid_body_set_linear_damping(ne_RigidBody b, float damping);
float ne_rigid_body_get_linear_damping(ne_RigidBody b);
void ne_rigid_body_set_angular_damping(ne_RigidBody b, float damping);
float ne_rigid_body_get_angular_damping(ne_RigidBody b);
void ne_rigid_body_set_sleeping_param(ne_RigidBody b, float param);
float ne_rigid_body_get_sleeping_param(ne_RigidBody b);

// Collision
void ne_rigid_body_set_collision_id(ne_RigidBody b, int32_t id);
int32_t ne_rigid_body_get_collision_id(ne_RigidBody b);
void ne_rigid_body_set_user_data(ne_RigidBody b, uintptr_t data);
uintptr_t ne_rigid_body_get_user_data(ne_RigidBody b);

// Geometry
ne_Geom ne_rigid_body_add_geom(ne_RigidBody b);
bool ne_rigid_body_remove_geom(ne_RigidBody b, ne_Geom g);
int32_t ne_rigid_body_get_geom_count(ne_RigidBody b);

// Sensors
ne_Sensor ne_rigid_body_add_sensor(ne_RigidBody b);
bool ne_rigid_body_remove_sensor(ne_RigidBody b, ne_Sensor s);

// Controllers
ne_RigidBodyControllerCB ne_create_rigid_body_controller_cb(ne_rigid_body_controller_fn fn, void* userdata);
void ne_destroy_rigid_body_controller_cb(ne_RigidBodyControllerCB cb);

ne_RigidBodyController ne_rigid_body_add_controller(ne_RigidBody b, ne_RigidBodyControllerCB cb, int32_t period);
bool ne_rigid_body_remove_controller(ne_RigidBody b, ne_RigidBodyController ctrl);

// gfx_Transform
HMM_Mat4 ne_rigid_body_get_transform(ne_RigidBody b);

// Inertia
void ne_rigid_body_set_inertia_tensor_mat(ne_RigidBody b, HMM_Mat4 tensor);
void ne_rigid_body_update_inertia_tensor(ne_RigidBody b);

// Angular momentum
HMM_Vec3 ne_rigid_body_get_angular_momentum(ne_RigidBody b);
void ne_rigid_body_set_angular_momentum(ne_RigidBody b, HMM_Vec3 am);

// Geometry iteration
void ne_rigid_body_begin_iterate_geom(ne_RigidBody b);
ne_Geom ne_rigid_body_get_next_geom(ne_RigidBody b);
ne_RigidBody ne_rigid_body_break_geom(ne_RigidBody b, ne_Geom g);

// Sensor iteration
void ne_rigid_body_begin_iterate_sensor(ne_RigidBody b);
ne_Sensor ne_rigid_body_get_next_sensor(ne_RigidBody b);

// Controller iteration
void ne_rigid_body_begin_iterate_controller(ne_RigidBody b);
ne_RigidBodyController ne_rigid_body_get_next_controller(ne_RigidBody b);

// Custom collision detection
void ne_rigid_body_use_custom_cd(ne_RigidBody b, bool yes, HMM_Mat4* obb, float bounding_radius);
bool ne_rigid_body_is_using_custom_cd(ne_RigidBody b);

// State
void ne_rigid_body_set_active(ne_RigidBody b, bool active);
bool ne_rigid_body_is_active(ne_RigidBody b);
bool ne_rigid_body_is_idle(ne_RigidBody b);
void ne_rigid_body_gravity_enable(ne_RigidBody b, bool enable);
bool ne_rigid_body_gravity_enabled(ne_RigidBody b);
void ne_rigid_body_collide_connected(ne_RigidBody b, bool enable);
bool ne_rigid_body_is_collide_connected(ne_RigidBody b);
void ne_rigid_body_collide_directly_connected(ne_RigidBody b, bool enable);
bool ne_rigid_body_is_collide_directly_connected(ne_RigidBody b);

void ne_rigid_body_update_bounding_info(ne_RigidBody b);

//--ANIMATED BODY--------------------------------------------------------------
// Spatial state
HMM_Vec3 ne_anim_body_get_pos(ne_AnimBody b);
void ne_anim_body_set_pos(ne_AnimBody b, HMM_Vec3 pos);
HMM_Quat ne_anim_body_get_rot(ne_AnimBody b);
void ne_anim_body_set_rot(ne_AnimBody b, HMM_Quat rot);

// Collision
void ne_anim_body_set_collision_id(ne_AnimBody b, int32_t id);
int32_t ne_anim_body_get_collision_id(ne_AnimBody b);
void ne_anim_body_set_user_data(ne_AnimBody b, uintptr_t data);
uintptr_t ne_anim_body_get_user_data(ne_AnimBody b);

// Geometry
ne_Geom ne_anim_body_add_geom(ne_AnimBody b);
bool ne_anim_body_remove_geom(ne_AnimBody b, ne_Geom g);
int32_t ne_anim_body_get_geom_count(ne_AnimBody b);

// Sensors
ne_Sensor ne_anim_body_add_sensor(ne_AnimBody b);
bool ne_anim_body_remove_sensor(ne_AnimBody b, ne_Sensor s);

// gfx_Transform
HMM_Mat4 ne_anim_body_get_transform(ne_AnimBody b);

// Geometry iteration
void ne_anim_body_begin_iterate_geom(ne_AnimBody b);
ne_Geom ne_anim_body_get_next_geom(ne_AnimBody b);
ne_RigidBody ne_anim_body_break_geom(ne_AnimBody b, ne_Geom g);

// Sensor iteration
void ne_anim_body_begin_iterate_sensor(ne_AnimBody b);
ne_Sensor ne_anim_body_get_next_sensor(ne_AnimBody b);

// Custom collision detection
void ne_anim_body_use_custom_cd(ne_AnimBody b, bool yes, HMM_Mat4* obb, float bounding_radius);
bool ne_anim_body_is_using_custom_cd(ne_AnimBody b);

// State
void ne_anim_body_set_active(ne_AnimBody b, bool active);
bool ne_anim_body_is_active(ne_AnimBody b);
void ne_anim_body_collide_connected(ne_AnimBody b, bool enable);
bool ne_anim_body_is_collide_connected(ne_AnimBody b);
void ne_anim_body_collide_directly_connected(ne_AnimBody b, bool enable);
bool ne_anim_body_is_collide_directly_connected(ne_AnimBody b);

void ne_anim_body_update_bounding_info(ne_AnimBody b);

//--SENSOR---------------------------------------------------------------------
void ne_sensor_set_line(ne_Sensor s, HMM_Vec3 pos, HMM_Vec3 line_vec);
void ne_sensor_set_user_data(ne_Sensor s, uintptr_t data);
uintptr_t ne_sensor_get_user_data(ne_Sensor s);

HMM_Vec3 ne_sensor_get_line_vec(ne_Sensor s);
HMM_Vec3 ne_sensor_get_line_unit_vec(ne_Sensor s);
HMM_Vec3 ne_sensor_get_line_pos(ne_Sensor s);

float ne_sensor_get_detect_depth(ne_Sensor s);
HMM_Vec3 ne_sensor_get_detect_normal(ne_Sensor s);
HMM_Vec3 ne_sensor_get_detect_contact_point(ne_Sensor s);
ne_RigidBody ne_sensor_get_detect_rigid_body(ne_Sensor s);
ne_AnimBody ne_sensor_get_detect_anim_body(ne_Sensor s);
int32_t ne_sensor_get_detect_material(ne_Sensor s);

//--GEOMETRY-------------------------------------------------------------------
// gfx_Transform
void ne_geom_set_transform(ne_Geom g, HMM_Mat4 transform);
HMM_Mat4 ne_geom_get_transform(ne_Geom g);

// Shapes
void ne_geom_set_box_size(ne_Geom g, float width, float height, float depth);
void ne_geom_set_box_size_vec(ne_Geom g, HMM_Vec3 size);
bool ne_geom_get_box_size(ne_Geom g, HMM_Vec3* out_size);

void ne_geom_set_sphere_diameter(ne_Geom g, float diameter);
bool ne_geom_get_sphere_diameter(ne_Geom g, float* out_diameter);

void ne_geom_set_cylinder(ne_Geom g, float diameter, float height);
bool ne_geom_get_cylinder(ne_Geom g, float* out_diameter, float* out_height);

void ne_geom_set_convex_mesh(ne_Geom g, void* convex_data);
bool ne_geom_get_convex_mesh(ne_Geom g, void** out_convex_data);

// Material
void ne_geom_set_material_index(ne_Geom g, int32_t index);
int32_t ne_geom_get_material_index(ne_Geom g);

// User data
void ne_geom_set_user_data(ne_Geom g, uintptr_t data);
uintptr_t ne_geom_get_user_data(ne_Geom g);

// Breakage
void ne_geom_set_breakage_flag(ne_Geom g, ne_BreakFlag flag);
ne_BreakFlag ne_geom_get_breakage_flag(ne_Geom g);

void ne_geom_set_breakage_mass(ne_Geom g, float mass);
float ne_geom_get_breakage_mass(ne_Geom g);

void ne_geom_set_breakage_inertia_tensor(ne_Geom g, HMM_Vec3 tensor);
HMM_Vec3 ne_geom_get_breakage_inertia_tensor(ne_Geom g);

void ne_geom_set_breakage_magnitude(ne_Geom g, float mag);
float ne_geom_get_breakage_magnitude(ne_Geom g);

void ne_geom_set_breakage_absorption(ne_Geom g, float absorb);
float ne_geom_get_breakage_absorption(ne_Geom g);

void ne_geom_set_breakage_plane(ne_Geom g, HMM_Vec3 plane_normal);
HMM_Vec3 ne_geom_get_breakage_plane(ne_Geom g);

void ne_geom_set_breakage_neighbour_radius(ne_Geom g, float radius);
float ne_geom_get_breakage_neighbour_radius(ne_Geom g);

//--RIGID BODY CONTROLLER------------------------------------------------------
ne_RigidBody ne_rigid_body_controller_get_rigid_body(ne_RigidBodyController ctrl);
HMM_Vec3 ne_rigid_body_controller_get_force(ne_RigidBodyController ctrl);
HMM_Vec3 ne_rigid_body_controller_get_torque(ne_RigidBodyController ctrl);
void ne_rigid_body_controller_set_force(ne_RigidBodyController ctrl, HMM_Vec3 force);
void ne_rigid_body_controller_set_torque(ne_RigidBodyController ctrl, HMM_Vec3 torque);
void ne_rigid_body_controller_set_force_with_torque(ne_RigidBodyController ctrl, HMM_Vec3 force, HMM_Vec3 pos);

//--JOINT----------------------------------------------------------------------
void ne_joint_set_type(ne_Joint joint, ne_JointType type);
ne_JointType ne_joint_get_type(ne_Joint joint);

void ne_joint_set_frame_a(ne_Joint joint, HMM_Mat4 frame);
void ne_joint_set_frame_b(ne_Joint joint, HMM_Mat4 frame);
void ne_joint_set_frame_world(ne_Joint joint, HMM_Mat4 frame);
HMM_Mat4 ne_joint_get_frame_a(ne_Joint joint);
HMM_Mat4 ne_joint_get_frame_b(ne_Joint joint);

void ne_joint_set_length(ne_Joint joint, float length);
float ne_joint_get_length(ne_Joint joint);

ne_RigidBody ne_joint_get_rigid_body_a(ne_Joint joint);
ne_RigidBody ne_joint_get_rigid_body_b(ne_Joint joint);
ne_AnimBody ne_joint_get_anim_body_b(ne_Joint joint);

void ne_joint_enable(ne_Joint joint, bool enable);
bool ne_joint_is_enabled(ne_Joint joint);

void ne_joint_set_damping_factor(ne_Joint joint, float damp);
float ne_joint_get_damping_factor(ne_Joint joint);

// Primary limits
bool ne_joint_is_limit_enabled(ne_Joint joint);
void ne_joint_enable_limit(ne_Joint joint, bool enable);
float ne_joint_get_upper_limit(ne_Joint joint);
void ne_joint_set_upper_limit(ne_Joint joint, float limit);
float ne_joint_get_lower_limit(ne_Joint joint);
void ne_joint_set_lower_limit(ne_Joint joint, float limit);

// Secondary limits
bool ne_joint_is_limit2_enabled(ne_Joint joint);
void ne_joint_enable_limit2(ne_Joint joint, bool enable);
float ne_joint_get_upper_limit2(ne_Joint joint);
void ne_joint_set_upper_limit2(ne_Joint joint, float limit);
float ne_joint_get_lower_limit2(ne_Joint joint);
void ne_joint_set_lower_limit2(ne_Joint joint, float limit);

// Solver params
void ne_joint_set_epsilon(ne_Joint joint, float epsilon);
float ne_joint_get_epsilon(ne_Joint joint);
void ne_joint_set_iteration(ne_Joint joint, int32_t iteration);
int32_t ne_joint_get_iteration(ne_Joint joint);

// Controllers
ne_JointControllerCB ne_create_joint_controller_cb(ne_joint_controller_fn fn, void* userdata);
void ne_destroy_joint_controller_cb(ne_JointControllerCB cb);

ne_JointController ne_joint_add_controller(ne_Joint joint, ne_JointControllerCB cb, int32_t period);
bool ne_joint_remove_controller(ne_Joint joint, ne_JointController ctrl);
void ne_joint_begin_iterate_controller(ne_Joint joint);
ne_JointController ne_joint_get_next_controller(ne_Joint joint);

// Motors
bool ne_joint_is_motor_enabled(ne_Joint joint);
void ne_joint_enable_motor(ne_Joint joint, bool enable);
void ne_joint_set_motor(ne_Joint joint, ne_MotorType type, float desire_value, float max_force);
void ne_joint_get_motor(ne_Joint joint, ne_MotorType* out_type, float* out_desire_value, float* out_max_force);

bool ne_joint_is_motor2_enabled(ne_Joint joint);
void ne_joint_enable_motor2(ne_Joint joint, bool enable);
void ne_joint_set_motor2(ne_Joint joint, ne_MotorType type, float desire_value, float max_force);
void ne_joint_get_motor2(ne_Joint joint, ne_MotorType* out_type, float* out_desire_value, float* out_max_force);

//--JOINT CONTROLLER-----------------------------------------------------------
ne_Joint ne_joint_controller_get_joint(ne_JointController ctrl);
HMM_Vec3 ne_joint_controller_get_force_body_a(ne_JointController ctrl);
HMM_Vec3 ne_joint_controller_get_force_body_b(ne_JointController ctrl);
HMM_Vec3 ne_joint_controller_get_torque_body_a(ne_JointController ctrl);
HMM_Vec3 ne_joint_controller_get_torque_body_b(ne_JointController ctrl);
void ne_joint_controller_set_force_body_a(ne_JointController ctrl, HMM_Vec3 force);
void ne_joint_controller_set_force_body_b(ne_JointController ctrl, HMM_Vec3 force);
void ne_joint_controller_set_force_with_torque_body_a(ne_JointController ctrl, HMM_Vec3 force, HMM_Vec3 pos);
void ne_joint_controller_set_force_with_torque_body_b(ne_JointController ctrl, HMM_Vec3 force, HMM_Vec3 pos);
void ne_joint_controller_set_torque_body_a(ne_JointController ctrl, HMM_Vec3 torque);
void ne_joint_controller_set_torque_body_b(ne_JointController ctrl, HMM_Vec3 torque);

//--COLLISION TABLE------------------------------------------------------------
void ne_collision_table_set(ne_CollisionTable table, int32_t id1, int32_t id2, ne_CollisionResponse response);
ne_CollisionResponse ne_collision_table_get(ne_CollisionTable table, int32_t id1, int32_t id2);
int32_t ne_collision_table_get_max_id(ne_CollisionTable table);

//--SIMULATOR (ADDITIONS)------------------------------------------------------
ne_RigidBody ne_sim_create_rigid_particle(ne_Simulator sim);
ne_CollisionTable ne_sim_get_collision_table(ne_Simulator sim);

bool ne_sim_set_material(ne_Simulator sim, int32_t index, float friction, float restitution);
bool ne_sim_get_material(ne_Simulator sim, int32_t index, float* out_friction, float* out_restitution);

void ne_sim_set_terrain_mesh(ne_Simulator sim, ne_TriangleMesh* mesh);
void ne_sim_free_terrain_mesh(ne_Simulator sim);

ne_Joint ne_sim_create_joint_rb(ne_Simulator sim, ne_RigidBody body_a);
ne_Joint ne_sim_create_joint_rb_rb(ne_Simulator sim, ne_RigidBody body_a, ne_RigidBody body_b);
ne_Joint ne_sim_create_joint_rb_ab(ne_Simulator sim, ne_RigidBody body_a, ne_AnimBody body_b);
void ne_sim_free_joint(ne_Simulator sim, ne_Joint joint);

void ne_sim_set_breakage_callback(ne_Simulator sim, ne_breakage_callback* cb);
ne_breakage_callback* ne_sim_get_breakage_callback(ne_Simulator sim);

void ne_sim_set_collision_callback(ne_Simulator sim, ne_collision_callback* cb);
ne_collision_callback* ne_sim_get_collision_callback(ne_Simulator sim);

void ne_sim_set_terrain_query_callback(ne_Simulator sim, ne_terrain_query_callback* cb);
ne_terrain_query_callback* ne_sim_get_terrain_query_callback(ne_Simulator sim);

void ne_sim_set_custom_cd_rb2rb_callback(ne_Simulator sim, ne_custom_cd_rb2rb_callback* cb);
ne_custom_cd_rb2rb_callback* ne_sim_get_custom_cd_rb2rb_callback(ne_Simulator sim);

void ne_sim_set_custom_cd_rb2ab_callback(ne_Simulator sim, ne_custom_cd_rb2ab_callback* cb);
ne_custom_cd_rb2ab_callback* ne_sim_get_custom_cd_rb2ab_callback(ne_Simulator sim);

void ne_sim_set_log_callback(ne_Simulator sim, ne_log_callback* cb);
ne_log_callback* ne_sim_get_log_callback(ne_Simulator sim);

void ne_sim_set_log_level(ne_Simulator sim, ne_LogLevel level);

ne_SimSizeInfo ne_sim_get_current_size_info(ne_Simulator sim);
ne_SimSizeInfo ne_sim_get_start_size_info(ne_Simulator sim);
void ne_sim_get_memory_allocated(ne_Simulator sim, int32_t* out_memory);

//--HELPER FUNCTIONS-----------------------------------------------------------
HMM_Vec3 ne_box_inertia_tensor(float width, float height, float depth, float mass);
HMM_Vec3 ne_box_inertia_tensor_vec(HMM_Vec3 box_size, float mass);
HMM_Vec3 ne_sphere_inertia_tensor(float diameter, float mass);
HMM_Vec3 ne_cylinder_inertia_tensor(float diameter, float height, float mass);

#ifdef __cplusplus
}
#endif
