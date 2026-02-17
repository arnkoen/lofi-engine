#include "core.h"
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "deps/sokol_app.h"
#include "deps/sokol_gfx.h"
#include "deps/sokol_glue.h"
#include "deps/sokol_gl.h"
#include "deps/sokol_audio.h"
#include "deps/sokol_debugtext.h"
#include "deps/sokol_log.h"
#include "deps/tlsf.h"
#define WA_IMPLEMENTATION
//#define DEBUG
#include "deps/wa.h"
#include "lofi.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static struct {
    Camera cam;
    RenderContext* gfx;
    AudioContext* sfx;
    Scene* scene;
    Module mod;
    ArenaAlloc arena;
    void* tlsf_pool;
    tlsf_t tlsf;
    Allocator allocator;
    int function;
} ctx;

static inline void* wa_ptr(uint32_t offset) {
    return ctx.mod.memory[0].bytes + offset;
}

lo_Texture lo_load_texture(const char* path) {
    IoMemory data = {0};
    Result result = load_file(&ctx.arena, &data, path, false);
    if(result == RESULT_SUCCESS) {
        TextureHandle ret = gfx_load_texture(ctx.gfx, &ctx.arena, &data);
        arena_reset(&ctx.arena);
        return (lo_Texture){ret.id};
    }
    arena_reset(&ctx.arena);
    LOG_ERROR("Failed to load Texture %s\n", path);
    return (lo_Texture){HP_INVALID_HANDLE};
}

lo_Model lo_load_model(const char* path) {
    IoMemory data = {0};
    Result result = load_file(&ctx.arena, &data, path, false);
    if(result == RESULT_SUCCESS) {
        ModelHandle ret = gfx_load_model(ctx.gfx, &ctx.arena, &data);
        arena_reset(&ctx.arena);
        return (lo_Model){ret.id};
    }
    arena_reset(&ctx.arena);
    LOG_ERROR("Failed to load Model %s\n", path);
    return (lo_Model){HP_INVALID_HANDLE};
}

lo_AnimSet lo_load_anims(const char* path) {
    IoMemory data = {0};
    Result result = load_file(&ctx.arena, &data, path, false);
    if(result == RESULT_SUCCESS) {
        AnimSetHandle ret = gfx_load_anims(ctx.gfx, &data);
        arena_reset(&ctx.arena);
        return (lo_AnimSet){ret.id};
    }
    arena_reset(&ctx.arena);
    LOG_ERROR("Failed to load AnimSet %s\n", path);
    return (lo_AnimSet){HP_INVALID_HANDLE};
}

lo_Sound lo_load_sound(const char* path) {
    IoMemory data = {0};
    Result result = load_file(&ctx.arena, &data, path, false);
    if(result == RESULT_SUCCESS) {
        SoundBufferHandle ret = sfx_load_buffer(ctx.sfx, &data);
        arena_reset(&ctx.arena);
        return (lo_Sound){ret.id};
    }
    arena_reset(&ctx.arena);
    LOG_ERROR("Failed to load Sound %s\n", path);
    return (lo_Sound){HP_INVALID_HANDLE};
}

static uint32_t wa_load_texture(uint64_t path_ptr) {
    return lo_load_texture((const char*)wa_ptr((uint32_t)path_ptr)).id;
}
static uint32_t wa_load_model(uint64_t path_ptr) {
    return lo_load_model((const char*)wa_ptr((uint32_t)path_ptr)).id;
}
static uint32_t wa_load_anims(uint64_t path_ptr) {
    return lo_load_anims((const char*)wa_ptr((uint32_t)path_ptr)).id;
}
static uint32_t wa_load_sound(uint64_t path_ptr) {
    return lo_load_sound((const char*)wa_ptr((uint32_t)path_ptr)).id;
}

static uint32_t wa_create(void) {
    return entity_new(ctx.scene).id;
}
static uint32_t wa_valid(uint64_t id) {
    return entity_valid(ctx.scene, (Entity){(uint32_t)id});
}
static void wa_destroy(uint64_t id) {
    entity_destroy(ctx.scene, (Entity){(uint32_t)id});
}

static void wa_set_position(uint64_t id, uint64_t ptr) {
    float* pos = (float*)wa_ptr((uint32_t)ptr);
    entity_set_position(ctx.scene, (Entity){(uint32_t)id}, HMM_V3(pos[0], pos[1], pos[2]));
}
static void wa_get_position(uint64_t id, uint64_t ptr) {
    HMM_Vec3 pos = entity_get_position(ctx.scene, (Entity){(uint32_t)id});
    float* out = (float*)wa_ptr((uint32_t)ptr);
    out[0] = pos.X; out[1] = pos.Y; out[2] = pos.Z;
}
static void wa_set_rotation(uint64_t id, uint64_t ptr) {
    float* rot = (float*)wa_ptr((uint32_t)ptr);
    entity_set_rotation(ctx.scene, (Entity){(uint32_t)id}, HMM_Q(rot[0], rot[1], rot[2], rot[3]));
}
static void wa_get_rotation(uint64_t id, uint64_t ptr) {
    HMM_Quat rot = entity_get_rotation(ctx.scene, (Entity){(uint32_t)id});
    float* out = (float*)wa_ptr((uint32_t)ptr);
    out[0] = rot.X; out[1] = rot.Y; out[2] = rot.Z; out[3] = rot.W;
}
static void wa_set_scale(uint64_t id, uint64_t ptr) {
    float* scale = (float*)wa_ptr((uint32_t)ptr);
    entity_set_scale(ctx.scene, (Entity){(uint32_t)id}, HMM_V3(scale[0], scale[1], scale[2]));
}
static void wa_get_scale(uint64_t id, uint64_t ptr) {
    HMM_Vec3 scale = entity_get_scale(ctx.scene, (Entity){(uint32_t)id});
    float* out = (float*)wa_ptr((uint32_t)ptr);
    out[0] = scale.X; out[1] = scale.Y; out[2] = scale.Z;
}

static void wa_set_parent(uint64_t entity, uint64_t parent) {
    entity_set_parent(ctx.scene, (Entity){(uint32_t)entity}, (Entity){(uint32_t)parent});
}
static void wa_remove_parent(uint64_t entity) {
    entity_remove_parent(ctx.scene, (Entity){(uint32_t)entity});
}
static void wa_add_child(uint64_t entity, uint64_t child) {
    entity_add_child(ctx.scene, (Entity){(uint32_t)entity}, (Entity){(uint32_t)child});
}
static void wa_set_children(uint64_t entity, uint64_t ptr) {
    uint32_t* ids = (uint32_t*)wa_ptr((uint32_t)ptr);
    Entity children[ENTITY_MAX_CHILDREN + 1];
    int i;
    for (i = 0; i < ENTITY_MAX_CHILDREN && ids[i] != 0; i++)
        children[i] = (Entity){ids[i]};
    children[i] = (Entity){0};
    entity_set_children(ctx.scene, (Entity){(uint32_t)entity}, children);
}
static void wa_clear_children(uint64_t entity) {
    entity_clear_children(ctx.scene, (Entity){(uint32_t)entity});
}

static void wa_set_model(uint64_t entity, uint64_t model) {
    entity_set_model(ctx.scene, (Entity){(uint32_t)entity}, (ModelHandle){(uint32_t)model});
}
static void wa_clear_model(uint64_t entity) {
    entity_clear_model(ctx.scene, (Entity){(uint32_t)entity});
}
static void wa_set_texture(uint64_t entity, uint64_t texture, uint64_t slot) {
    Entity e = {(uint32_t)entity};
    if (!entity_valid(ctx.scene, e)) return;
    int idx = hp_index(e.id);
    int s = (int)slot;
    if (s < 0 || s >= 4) return;
    ctx.scene->textures[idx].tex[s] = (TextureHandle){(uint32_t)texture};
}
static void wa_clear_textures(uint64_t entity) {
    entity_clear_textures(ctx.scene, (Entity){(uint32_t)entity});
}

static void wa_set_anims(uint64_t entity, uint64_t ptr) {
    lo_AnimDesc* desc = (lo_AnimDesc*)wa_ptr((uint32_t)ptr);
    AnimState state = { .flags = desc->flags, .anim = desc->anim, .current_frame = 0.0f };
    entity_set_anim(ctx.scene, (Entity){(uint32_t)entity},
        (AnimSetHandle){desc->set.id}, state);
}
static void wa_clear_anims(uint64_t entity) {
    entity_clear_anim(ctx.scene, (Entity){(uint32_t)entity});
}

static void wa_set_sound(uint64_t entity, uint64_t ptr) {
    lo_SoundDesc* desc = (lo_SoundDesc*)wa_ptr((uint32_t)ptr);
    SoundProps props = { .volume = desc->vol, .min_range = desc->min_range, .max_range = desc->max_range };
    entity_set_sound(ctx.scene, (Entity){(uint32_t)entity},
        (SoundBufferHandle){desc->sound.id}, props, desc->flags);
}
static void wa_play_sound(uint64_t entity) {
    entity_play_sound(ctx.scene, (Entity){(uint32_t)entity});
}
static void wa_stop_sound(uint64_t entity) {
    entity_stop_sound(ctx.scene, (Entity){(uint32_t)entity});
}
static void wa_clear_sound(uint64_t entity) {
    entity_clear_sound(ctx.scene, (Entity){(uint32_t)entity});
}

static float wa_sinf(float x) { return HMM_SinF(x); }
static float wa_cosf(float x) { return HMM_CosF(x); }

static void wa_set_campos(uint64_t ptr) {
    float* pos = (float*)wa_ptr((uint32_t)ptr);
    ctx.cam.position = HMM_V3(pos[0], pos[1], pos[2]);
}
static void wa_set_cam_target(uint64_t ptr) {
    float* t = (float*)wa_ptr((uint32_t)ptr);
    ctx.cam.target = HMM_V3(t[0], t[1], t[2]);
}

/* Debug text */
static void wa_dtx_layer(uint64_t layer_id) { sdtx_layer((int)layer_id); }
static void wa_dtx_font(uint64_t font_index) { sdtx_font((int)font_index); }
static void wa_dtx_canvas(float w, float h) { sdtx_canvas(w, h); }
static void wa_dtx_origin(float x, float y) { sdtx_origin(x, y); }
static void wa_dtx_home(void) { sdtx_home(); }
static void wa_dtx_pos(float x, float y) { sdtx_pos(x, y); }
static void wa_dtx_pos_x(float x) { sdtx_pos_x(x); }
static void wa_dtx_pos_y(float y) { sdtx_pos_y(y); }
static void wa_dtx_move(float dx, float dy) { sdtx_move(dx, dy); }
static void wa_dtx_move_x(float dx) { sdtx_move_x(dx); }
static void wa_dtx_move_y(float dy) { sdtx_move_y(dy); }
static void wa_dtx_crlf(void) { sdtx_crlf(); }
static void wa_dtx_color3b(uint64_t r, uint64_t g, uint64_t b) { sdtx_color3b((uint8_t)r, (uint8_t)g, (uint8_t)b); }
static void wa_dtx_color3f(float r, float g, float b) { sdtx_color3f(r, g, b); }
static void wa_dtx_color4f(uint64_t ptr) { float* c = (float*)wa_ptr((uint32_t)ptr); sdtx_color4f(c[0], c[1], c[2], c[3]); }
static void wa_dtx_color1i(uint64_t rgba) { sdtx_color1i((uint32_t)rgba); }
static void wa_dtx_putc(uint64_t c) { sdtx_putc((char)c); }
static void wa_dtx_puts(uint64_t ptr) { sdtx_puts((const char*)wa_ptr((uint32_t)ptr)); }
static void wa_dtx_putr(uint64_t ptr, uint64_t len) { sdtx_putr((const char*)wa_ptr((uint32_t)ptr), (int)len); }

static void* tlsf_alloc_wrapper(size_t size, size_t align, void* udata) {
    tlsf_t tlsf = (tlsf_t)udata;
    return tlsf_memalign(tlsf, align, size);
}

static void tlsf_free_wrapper(void* ptr, void* udata) {
    tlsf_t tlsf = (tlsf_t)udata;
    tlsf_free(tlsf, ptr);
}

static void* ne_alloc_wrapper(size_t size, int32_t alignment, void* udata) {
    tlsf_t tlsf = (tlsf_t)udata;
    if (alignment > 0) {
        return tlsf_memalign(tlsf, (size_t)alignment, size);
    }
    return tlsf_malloc(tlsf, size);
}

static void ne_free_wrapper(void* ptr, void* udata) {
    tlsf_t tlsf = (tlsf_t)udata;
    tlsf_free(tlsf, ptr);
}

static Result load_wasm(IoMemory* out, const char *path) {
    if  (!out || !path) return RESULT_INVALID_PARAMS;

    FILE *file = fopen(path, "rb");
    if (!file) {
        LOG_ERROR("Failed to open file: %s", path);
        return RESULT_FILE_NOT_FOUND;
    }
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    out->ptr = core_alloc(&ctx.allocator, filesize + 1, alignof(uint8_t));
    if (!out->ptr) {
        fclose(file);
        LOG_ERROR("Failed to allocate IoMemory for file: %s", path);
        return RESULT_NOMEM;
    }

    fread(out->ptr, 1, filesize, file);
    fclose(file);

    out->size = filesize;
    LOG_INFO("Loaded wasm: %s (%ld bytes)\n", path, filesize);
    return RESULT_SUCCESS;
}

static RTLink link[] = {
    //exports
    { "lo_init",           NULL,              0, WA_v },
    { "lo_frame",          NULL,              0, WA_vf },
    { "lo_cleanup",        NULL,              0, WA_v },
    //imports
    { "lo_load_texture",   &wa_load_texture,  0, WA_il },
    { "lo_load_model",     &wa_load_model,    0, WA_il },
    { "lo_load_anims",     &wa_load_anims,    0, WA_il },
    { "lo_load_sound",     &wa_load_sound,    0, WA_il },
    { "lo_create",         &wa_create,        0, WA_i },
    { "lo_valid",          &wa_valid,         0, WA_il },
    { "lo_destroy",        &wa_destroy,       0, WA_vl },
    { "lo_set_position",   &wa_set_position,  0, WA_vll },
    { "lo_get_position",   &wa_get_position,  0, WA_vll },
    { "lo_set_rotation",   &wa_set_rotation,  0, WA_vll },
    { "lo_get_rotation",   &wa_get_rotation,  0, WA_vll },
    { "lo_set_scale",      &wa_set_scale,     0, WA_vll },
    { "lo_get_scale",      &wa_get_scale,     0, WA_vll },
    { "lo_set_parent",     &wa_set_parent,    0, WA_vll },
    { "lo_remove_parent",  &wa_remove_parent, 0, WA_vl },
    { "lo_add_child",      &wa_add_child,     0, WA_vll },
    { "lo_set_children",   &wa_set_children,  0, WA_vll },
    { "lo_clear_children", &wa_clear_children,0, WA_vl },
    { "lo_set_model",      &wa_set_model,     0, WA_vll },
    { "lo_clear_model",    &wa_clear_model,   0, WA_vl },
    { "lo_set_texture",    &wa_set_texture,   0, WA_vlll },
    { "lo_clear_textures", &wa_clear_textures,0, WA_vl },
    { "lo_set_anims",      &wa_set_anims,     0, WA_vll },
    { "lo_clear_anims",    &wa_clear_anims,   0, WA_vl },
    { "lo_set_sound",      &wa_set_sound,     0, WA_vll },
    { "lo_play_sound",     &wa_play_sound,    0, WA_vl },
    { "lo_stop_sound",     &wa_stop_sound,    0, WA_vl },
    { "lo_clear_sound",    &wa_clear_sound,   0, WA_vl },
    { "lo_sinf",           &wa_sinf,          0, WA_ff },
    { "lo_cosf",           &wa_cosf,          0, WA_ff },
    { "lo_set_campos",     &wa_set_campos,    0, WA_vl },
    { "lo_set_cam_target", &wa_set_cam_target,0, WA_vl },
    //debug text
    { "lo_dtx_layer",      &wa_dtx_layer,     0, WA_vl },
    { "lo_dtx_font",       &wa_dtx_font,      0, WA_vl },
    { "lo_dtx_canvas",     &wa_dtx_canvas,    0, WA_vff },
    { "lo_dtx_origin",     &wa_dtx_origin,    0, WA_vff },
    { "lo_dtx_home",       &wa_dtx_home,      0, WA_v },
    { "lo_dtx_pos",        &wa_dtx_pos,       0, WA_vff },
    { "lo_dtx_pos_x",      &wa_dtx_pos_x,     0, WA_vf },
    { "lo_dtx_pos_y",      &wa_dtx_pos_y,     0, WA_vf },
    { "lo_dtx_move",       &wa_dtx_move,      0, WA_vff },
    { "lo_dtx_move_x",     &wa_dtx_move_x,    0, WA_vf },
    { "lo_dtx_move_y",     &wa_dtx_move_y,    0, WA_vf },
    { "lo_dtx_crlf",       &wa_dtx_crlf,      0, WA_v },
    { "lo_dtx_color3b",    &wa_dtx_color3b,   0, WA_vlll },
    { "lo_dtx_color3f",    &wa_dtx_color3f,   0, WA_vfff },
    { "lo_dtx_color4f",    &wa_dtx_color4f,   0, WA_vl },
    { "lo_dtx_color1i",    &wa_dtx_color1i,   0, WA_vl },
    { "lo_dtx_putc",       &wa_dtx_putc,      0, WA_vl },
    { "lo_dtx_puts",       &wa_dtx_puts,      0, WA_vl },
    { "lo_dtx_putr",       &wa_dtx_putr,      0, WA_vll },
    { 0 }
};

#define TLSF_POOL_SIZE (32 * 1024 * 1024) // 32MB pool

static void init(void) {
    ctx.tlsf_pool = malloc(TLSF_POOL_SIZE);
    if (!ctx.tlsf_pool) {
        LOG_ERROR("Failed to allocate TLSF pool\n");
        return;
    }
    ctx.tlsf = tlsf_create_with_pool(ctx.tlsf_pool, TLSF_POOL_SIZE);
    if (!ctx.tlsf) {
        free(ctx.tlsf_pool);
        LOG_ERROR("Failed to create TLSF allocator\n");
        return;
    }

    ctx.allocator = (Allocator) {
        .udata = ctx.tlsf,
        .alloc = tlsf_alloc_wrapper,
        .free = tlsf_free_wrapper,
    };

    size_t memsize = 1024 * 1024;
    void* buffer = core_alloc(&ctx.allocator, memsize, 0);

    arena_init(&ctx.arena, buffer, memsize);

    ctx.cam = (Camera){
        .position = HMM_V3(0, 0.5f, 10.f),
        .target = HMM_V3(0, 0.75f, 0),
        .farz = 1000.f,
        .nearz = 0.1f,
        .fov = 60.f,
    };

    ctx.gfx = gfx_new_context(&ctx.allocator, &(RenderContextDesc) {
        .environment = sglue_environment(),
        .swapchain = sglue_swapchain(),
        .max_anim_data = (1024 * 1024),
        .max_anim_sets = 32,
        .max_meshes = 32,
        .max_textures = 32,
        .width = 800,
        .height = 600,
    });

    ctx.sfx = sfx_new_context(&ctx.allocator, 32);

    ctx.scene = scene_new(&ctx.allocator, 512);
    IoMemory mem = {};
    Result result = load_wasm(&mem, "game.wasm");
    if (result != RESULT_SUCCESS) {
        LOG_ERROR("Failed to load game.wasm!\n");
        sapp_quit();
    }

    wa_init(&ctx.mod, mem.ptr, mem.size, link);
    core_free(&ctx.allocator, mem.ptr);

    ctx.function = wa_sym(&ctx.mod, "lo_init");
    printf("fidx %d\n\n", ctx.function);
    StackValue ret = {0};
    ret = wa_call(&ctx.mod, ctx.function);
    printf("WASM function returned: %lld (err_code %d)\n\n", ret.i64, ctx.mod.err_code);
    ctx.function = wa_sym(&ctx.mod, "lo_frame");
}

static void frame(void) {
    float dt = (float)sapp_frame_duration();

    wa_push_f32(&ctx.mod, dt);
    wa_call(&ctx.mod, ctx.function);

    HMM_Vec3 listener_forward = HMM_Norm(HMM_SubV3(ctx.cam.target, ctx.cam.position));
    sfx_update(ctx.sfx, ctx.cam.position, listener_forward, ctx.scene, dt);

    gfx_render(ctx.gfx, ctx.scene, &ctx.cam, sglue_swapchain(), dt);
}

static void cleanup(void) {
    sfx_shutdown(ctx.sfx);
    gfx_shutdown(ctx.gfx);
}

static void event(const sapp_event* ev) {
    if(ev->type == SAPP_EVENTTYPE_KEY_DOWN && ev->key_repeat == false) {
        if (ev->key_code == SAPP_KEYCODE_F) {
            sapp_toggle_fullscreen();
        }
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 1280,
        .height = 720,
        .window_title = "LOFI",
        .swap_interval = 1,
        .icon.sokol_default = true,
        .win32_console_attach = true,
    };
}
