#pragma once

/*
 * TODO:
 * -more image formats?
 */

#include "deps/hmm.h"
#include "deps/sokol_gfx.h"
#include "deps/sokol_gl.h"
#include "shaders/display.glsl.h"
#include "shaders/shaders.glsl.h"
#include "deps/tmixer.h"
#include "deps/handle_pool.h"
#include "deps/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sapp_event sapp_event;

typedef struct Scene Scene;
typedef struct Entity { hp_Handle id; } Entity;


//--LOG---------------------------------------------------------------------------------------

#ifdef _WIN32
#define ANSI(code) ""
#else
#define ANSI(code) code
#endif

#define LOG_INFO(...)  do { printf("%s[INFO  %.8s]%s %d: ", ANSI("\27[32m"), ANSI("\27[0m"), __FILE__, __LINE__); printf(__VA_ARGS__); } while(0)
#define LOG_WARN(...)  do { printf("%s[WARN  %.8s]%s %d: ", ANSI("\27[33m"), ANSI("\27[0m"), __FILE__, __LINE__); printf(__VA_ARGS__); } while(0)
#define LOG_ERROR(...) do { printf("%s[ERROR %.8s]%s %d: ", ANSI("\27[31m"), ANSI("\27[0m"), __FILE__, __LINE__); printf(__VA_ARGS__); } while(0)


//--ALLOCATOR-------------------------------------------------------------------------

typedef void* (*AllocFn)(size_t size, size_t align, void* udata);
typedef void  (*FreeFn)(void* ptr, void* udata);

typedef struct Allocator {
	void* udata;
	AllocFn alloc;
	FreeFn free;
} Allocator;

void* core_alloc(Allocator* alloc, size_t size, size_t align);
void core_free(Allocator* alloc, void* ptr);

Allocator default_allocator(void);


//--IO------------------------------------------------------------------------------

typedef struct {
    uint8_t* ptr;
    size_t size;
} IoMemory;

#ifndef __cplusplus
#define KIT_IoMemory(x) ((kit_IoMemory){&x, sizeof(x)})
#else
#define KIT_IoMemory(x) (kit_IoMemory{&x, sizeof(x)})
#endif

typedef uint16_t Result;

enum {
    RESULT_SUCCESS = 0,
    RESULT_NOMEM,
    RESULT_INVALID_PARAMS,
    RESULT_FILE_NOT_FOUND,
    RESULT_UNKNOWN_ERROR,
};

Result load_file(ArenaAlloc* alloc, IoMemory* out, const char* path, bool null_terminate);

//--GFX-------------------------------------------------------------------------------

//CAMERA

typedef struct Camera {
    float fov;
    float nearz;
    float farz;
    HMM_Vec3 target;
    HMM_Vec3 position;
} Camera;

HMM_Mat4 camera_view_mtx(Camera* cam);
HMM_Mat4 camera_proj_mtx(Camera* cam, int width, int height);

//IMAGES

typedef struct Texture {
    sg_image image;
    sg_view view;
} Texture;

//MODELS

typedef struct VertexPNT {
    HMM_Vec3 pos;
    HMM_Vec3 nrm;
    HMM_Vec2 uv;
} VertexPNT;

typedef struct VertexSkin {
    uint8_t indices[4];
    uint8_t weights[4];
} VertexSkin;

#define MESH_MAX_VBUFS 4

typedef struct Mesh {
    sg_buffer vbufs[MESH_MAX_VBUFS];
    sg_buffer ibuf;
    int first_element;
    int element_count;
} Mesh;


typedef struct Bounds {
    float min[3], max[3];
    float radius_xy, radius;
} Bounds;

typedef struct Model{
    Bounds bounds;
    Mesh meshes[4];
    int meshes_count;
} Model;

//ANIMATIONS

#define MAX_BONES 32
#define MAX_NAME_LEN 64

typedef struct Transform {
    HMM_Quat rot;
    HMM_Vec3 pos;
    HMM_Vec3 scale;
} Transform;

typedef struct AnimInfo {
    char name[MAX_NAME_LEN];
    unsigned int first_frame;
    unsigned int num_frames;
    float framerate;
} AnimInfo;

typedef struct AnimSet {
    AnimInfo* anims;
    HMM_Mat4* frames;
    int* joint_parents;
    int num_anims;
    int num_frames;
    int num_joints;
} AnimSet;

#define ANIM_FLAG_NONE (0)
#define ANIM_FLAG_PLAY (1 << 0)
#define ANIM_FLAG_LOOP (1 << 1)

#define ANIM_BLEND_DURATION 0.25f

typedef struct AnimState {
    int flags;
    int anim;
    float current_frame;
} AnimState;

typedef struct RenderContextDesc {
    sg_environment environment;
    sg_swapchain swapchain;
    int width, height;
    size_t max_anim_data;
    uint16_t max_anim_sets;
    uint16_t max_meshes;
    uint16_t max_textures;
} RenderContextDesc;

#define GFX_PIPELINE_COUNT 3
#define GFX_PIP_DEFAULT 0
#define GFX_PIP_SKINNED 1
#define GFX_PIP_CUBEMAP 2

typedef struct RenderContext {
    struct {
        hp_Pool pool;
        AnimSet* data;
        ArenaAlloc alloc;
    } anims;
    struct {
        hp_Pool pool;
        Model* data;
    } meshes;
    struct {
        hp_Pool pool;
        Texture* data;
    } textures;
    struct {
        struct {
            Texture tex;
            sg_buffer ibuf;
            sg_buffer vbuf;
            sg_sampler smp;
        } cubemap;
        int width, height;
        sg_image color_img;
        sg_image depth_img;
        sg_pipeline pip[GFX_PIPELINE_COUNT];
        sgl_pipeline physics_pip;
        sg_sampler default_sampler;
        u_dir_light_t light;
        sg_pass pass;
    } offscreen;
    struct {
        sg_pass_action action;
        sg_pipeline pip;
        sg_bindings rect;
        HMM_Vec2 mouse_pos;
    } display;
    bool draw_physics;
} RenderContext;

RenderContext* gfx_new_context(Allocator* alloc, const RenderContextDesc* desc);
void gfx_render(RenderContext* gfx, Scene* scene, Camera* cam, sg_swapchain swapchain, float dt);
void gfx_event(RenderContext* gfx, const sapp_event* e);
void gfx_shutdown(RenderContext* gfx);
void gfx_load_cubemap(RenderContext* ctx, ArenaAlloc* alloc, IoMemory* mem);

typedef struct ModelHandle { hp_Handle id; } ModelHandle;
ModelHandle gfx_load_model(RenderContext* ctx, ArenaAlloc* alloc, const IoMemory* data);
void gfx_release_model(RenderContext* ctx, ModelHandle mesh);

typedef struct TextureHandle { hp_Handle id; } TextureHandle;
TextureHandle gfx_load_texture(RenderContext* ctx, ArenaAlloc* alloc, IoMemory* data);
void gfx_release_texture(RenderContext* ctx, TextureHandle tex);

typedef struct AnimSetHandle { hp_Handle id; } AnimSetHandle;
AnimSetHandle gfx_load_anims(RenderContext* ctx, const IoMemory* data);
void gfx_clear_anims(RenderContext* ctx);

void update_anim_state(AnimState* state, AnimSet* set, float dt);
void play_anim(u_skeleton_t* out, AnimSet* set, AnimState* state);
void blend_anims(u_skeleton_t* out_a, const u_skeleton_t* out_b, float weight, int num_joints);

//--IMMEDIATE-MODE-HELPERS-------------------------------------------------------

void imdraw_box(HMM_Vec3 pos, HMM_Quat rot, HMM_Vec3 scale);
void imdraw_cylinder(HMM_Vec3 pos, HMM_Quat rot, float diameter, float height);

//--SFX--------------------------------------------------------------------------

typedef const tm_buffer* SoundBuffer;
typedef tm_channel SoundChannel;

typedef struct SoundProps {
    float volume;
    float min_range;
    float max_range;
} SoundProps;

static inline SoundProps sound_props_default(void) {
    SoundProps props;
    props.volume = 0.75f;
    props.min_range = 1.0f;
    props.max_range = 100.f;
    return props;
}

typedef struct SoundListener {
    HMM_Vec3 current_pos;
    HMM_Vec3 target_pos;
    HMM_Vec3 velocity;
    HMM_Vec3 smoothed_velocity;
    HMM_Vec3 current_forward;
    HMM_Vec3 target_forward;
    float frame_dt;
    float time_since_update;
    float smoothing;
    float velocity_smoothing;
} SoundListener;

typedef struct AudioContext {
    struct {
        hp_Pool pool;
        SoundBuffer* data;
    } buffers;
    SoundListener listener;
} AudioContext;

AudioContext* sfx_new_context(Allocator* alloc, uint16_t max_buffers);
void sfx_update(AudioContext* sfx, HMM_Vec3 listener_pos, HMM_Vec3 listener_forward, Scene* scene, float dt);
void sfx_shutdown(AudioContext* sfx);

typedef struct SoundBufferHandle { hp_Handle id; } SoundBufferHandle;
SoundBufferHandle sfx_load_buffer(AudioContext* ctx, IoMemory* data);
void sfx_release_buffer(AudioContext* sfx, SoundBufferHandle buf);


//--ENTITY---------------------------------


#define ENTITY_HAS_PARENT   (1U << 0)
#define ENTITY_HAS_CHILDREN (1U << 1)

#define ENTITY_VISIBLE      (1U << 0)
#define ENTITY_HAS_MODEL    (1U << 1)
#define ENTITY_HAS_ANIM     (1U << 2)

#define ENTITY_HAS_SOUND     (1U << 0)
#define ENTITY_SOUND_PLAY    (1U << 1)
#define ENTITY_SOUND_LOOP    (1U << 2)
#define ENTITY_SOUND_SPATIAL (1U << 3)
#define ENTITY_SOUND_PLAYING (1U << 4)

#define ENTITY_MAX_CHILDREN 8


typedef struct {
    Entity data[ENTITY_MAX_CHILDREN];
} Children;

typedef struct {
    TextureHandle tex[4];
} TextureSet;

typedef uint16_t RelationFlags;
typedef uint16_t ModelFlags;
typedef uint16_t AnimFlags;
typedef uint16_t SoundFlags;

typedef struct Scene {
    hp_Pool pool;

    RelationFlags* relation_flags;
    Transform* transforms;
    Entity* parents;
    Children* childs;

    ModelFlags* model_flags;
    ModelHandle* models;
    TextureSet* textures;

    AnimFlags* anim_flags;
    AnimSetHandle* anims;
    AnimState* anim_states;
    AnimState* prev_anim_states;
    float* anim_blend_weights;

    SoundFlags* sound_flags;
    SoundBufferHandle* sound_buffers;
    SoundChannel* sound_channels;
    SoundProps* sound_props;
} Scene;

Scene* scene_new(Allocator* alloc, uint16_t max_things);
void scene_destroy(Allocator* alloc, Scene* scene);
Entity entity_new(Scene* scene);
bool entity_valid(Scene* scene, Entity entity);
void entity_destroy(Scene* scene, Entity entity);

void entity_set_position(Scene* scene, Entity e, HMM_Vec3 pos);
HMM_Vec3 entity_get_position(Scene* scene, Entity e);
void entity_set_rotation(Scene* scene, Entity e, HMM_Quat rot);
HMM_Quat entity_get_rotation(Scene* scene, Entity e);
void entity_set_scale(Scene* scene, Entity e, HMM_Vec3 scale);
HMM_Vec3 entity_get_scale(Scene* scene, Entity e);
void entity_set_transform(Scene* scene, Entity e, Transform trs);
Transform entity_get_transform(Scene* scene, Entity e);
HMM_Mat4 entity_mtx(Scene* scene, Entity entity);

void entity_set_parent(Scene* scene, Entity entity, Entity parent);
void entity_remove_parent(Scene* scene, Entity entity);
void entity_add_child(Scene* scene, Entity entity, Entity child);
void entity_set_children(Scene* scene, Entity e, Entity* children);
void entity_clear_children(Scene* scene, Entity e);

void entity_set_model(Scene* scene, Entity entity, ModelHandle mesh);
void entity_clear_model(Scene* scene, Entity entity);
void entity_set_textures(Scene* scene, Entity entity, TextureSet set);
void entity_clear_textures(Scene* scene, Entity entity);
void entity_set_anim(Scene* scene, Entity entity, AnimSetHandle set, AnimState state);
void entity_clear_anim(Scene* scene, Entity entity);

void entity_set_sound(Scene* scene, Entity e, SoundBufferHandle buffer, SoundProps props, uint32_t flags);
void entity_play_sound(Scene* scene, Entity e);
void entity_stop_sound(Scene* scene, Entity e);
void entity_clear_sound(Scene* scene, Entity e);


#ifdef __cplusplus
}
#endif
