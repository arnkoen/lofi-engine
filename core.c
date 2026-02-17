#include "core.h"
#include "deps/sokol_app.h"
#include "deps/sokol_gl.h"
#include "deps/sokol_debugtext.h"
#include "deps/sokol_audio.h"
#include "deps/sokol_log.h"
#include "deps/dds-ktx.h"
#include "deps/iqm.h"
#include "deps/tinyendian.c"
#include "deps/tmixer.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

//--ALLOCATORS-------------------------------------------------------

void* core_alloc(Allocator* alloc, size_t size, size_t align) {
    if (alloc && alloc->alloc) {
        return alloc->alloc(size, align, alloc->udata);
    }
    return NULL;
}

void core_free(Allocator* alloc, void* ptr) {
    if (alloc && alloc->free) {
        alloc->free(ptr, alloc->udata);
    }
}

//DEFAULTS

static void* _default_alloc(size_t size, size_t align, void* udata) {
    (void)udata;
    (void)align;
    return malloc(size);
}

static void _default_free(void* ptr, void* udata) {
    (void)udata;
    free(ptr);
}

Allocator default_allocator(void) {
    Allocator alloc = {0};
    alloc.alloc = _default_alloc;
    alloc.free = _default_free;
    return alloc;
}

//--IO------------------------------------------------------------------------------------------------------------------


Result load_file(ArenaAlloc *alloc, IoMemory* out, const char *path, bool null_terminate) {
    if (!alloc || !out || !path) return RESULT_INVALID_PARAMS;

    FILE *file = fopen(path, "rb");
    if (!file) {
        LOG_ERROR("Failed to open file: %s", path);
        return RESULT_FILE_NOT_FOUND;
    }
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    out->ptr = arena_alloc(alloc, filesize + 1, alignof(uint8_t));
    if (!out->ptr) {
        fclose(file);
        LOG_ERROR("Failed to allocate IoMemory for file: %s", path);
        return RESULT_NOMEM;
    }

    fread(out->ptr, 1, filesize, file);
    fclose(file);

    if (null_terminate) {
        out->ptr[filesize] = '\0';
    }

    out->size = filesize;
    LOG_INFO("Loaded file: %s (%ld bytes)\n", path, filesize);
    return RESULT_SUCCESS;
}



//--CAMERA-------------------------------------------------------------------------------


HMM_Mat4 camera_view_mtx(Camera *cam) {
    return HMM_LookAt_RH(cam->position, cam->target, HMM_V3(0.0f, 1.0f, 0.0f));
}

HMM_Mat4 camera_proj_mtx(Camera *cam, int width, int height) {
    return HMM_Perspective_RH_ZO(cam->fov * HMM_DegToRad, (float)width / (float)height, cam->nearz, cam->farz);
}


//--IMAGES-------------------------------------------------------------------------------

static sg_pixel_format dds_to_sg_pixelformt(ddsktx_format fmt) {
    switch(fmt) {
        case DDSKTX_FORMAT_BC1:     return SG_PIXELFORMAT_BC1_RGBA; break;
        case DDSKTX_FORMAT_BC2:     return SG_PIXELFORMAT_BC2_RGBA; break;
        case DDSKTX_FORMAT_BC3:     return SG_PIXELFORMAT_BC3_RGBA; break;
        case DDSKTX_FORMAT_BC4:     return SG_PIXELFORMAT_BC4_R; break;
        case DDSKTX_FORMAT_BC5:     return SG_PIXELFORMAT_BC5_RG; break;
        case DDSKTX_FORMAT_BC6H:    return SG_PIXELFORMAT_BC6H_RGBF; break;
        case DDSKTX_FORMAT_BC7:     return SG_PIXELFORMAT_BC7_RGBA; break;
        case DDSKTX_FORMAT_A8:
        case DDSKTX_FORMAT_R8:      return SG_PIXELFORMAT_R8; break;
        case DDSKTX_FORMAT_RGBA8:
        case DDSKTX_FORMAT_RGBA8S:  return SG_PIXELFORMAT_RGBA8; break;
        case DDSKTX_FORMAT_RG16:    return SG_PIXELFORMAT_RG16; break;
        case DDSKTX_FORMAT_RGB8:    return SG_PIXELFORMAT_RGBA8; break;
        case DDSKTX_FORMAT_R16:     return SG_PIXELFORMAT_R16; break;
        case DDSKTX_FORMAT_R32F:    return SG_PIXELFORMAT_R32F; break;
        case DDSKTX_FORMAT_R16F:    return SG_PIXELFORMAT_R16F; break;
        case DDSKTX_FORMAT_RG16F:   return SG_PIXELFORMAT_RG16F; break;
        case DDSKTX_FORMAT_RG16S:   return SG_PIXELFORMAT_RG16; break;
        case DDSKTX_FORMAT_RGBA16F: return SG_PIXELFORMAT_RGBA16F; break;
        case DDSKTX_FORMAT_RGBA16:  return SG_PIXELFORMAT_RGBA16; break;
        case DDSKTX_FORMAT_BGRA8:   return SG_PIXELFORMAT_BGRA8; break;
        case DDSKTX_FORMAT_RGB10A2: return SG_PIXELFORMAT_RGB10A2; break;
        case DDSKTX_FORMAT_RG11B10F:return SG_PIXELFORMAT_RG11B10F; break;
        case DDSKTX_FORMAT_RG8:     return SG_PIXELFORMAT_RG8; break;
        case DDSKTX_FORMAT_RG8S:    return SG_PIXELFORMAT_RG8; break;
        default: return SG_PIXELFORMAT_NONE;
    }
}

sg_image_type dds_to_sg_image_type(unsigned int flags) {
    if (flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) {
        return SG_IMAGETYPE_CUBE;
    }
    if(flags & DDSKTX_TEXTURE_FLAG_VOLUME) {
        return SG_IMAGETYPE_3D;
    }
    return SG_IMAGETYPE_2D;
}

Result load_texture(ArenaAlloc* alloc, Texture* out, const IoMemory* mem) {
    ddsktx_texture_info tc = {0};
    if (ddsktx_parse(&tc, (const void*)mem->ptr, (int)mem->size, NULL)) {
        sg_image_desc desc = {0};
        desc.num_mipmaps = tc.num_mips;
        desc.num_slices = tc.num_layers;
        desc.pixel_format = dds_to_sg_pixelformt(tc.format);
        desc.width = tc.width;
        desc.height = tc.height;
        desc.type = dds_to_sg_image_type(tc.flags);

        for (int mip = 0; mip < tc.num_mips; mip++) {
            ddsktx_sub_data sub_data;
            ddsktx_get_sub(&tc, &sub_data, (const void*)mem->ptr, (int)mem->size, 0, 0, mip);
            void* ptr = arena_alloc(alloc, sub_data.size_bytes, 0);
            assert(ptr);
            memcpy(ptr, sub_data.buff, sub_data.size_bytes);
            desc.data.mip_levels[mip] = (sg_range){ptr, sub_data.size_bytes};
        }
        out->image = sg_make_image(&desc);
        out->view = sg_make_view(&(sg_view_desc) {
            .texture = out->image,
        });
        return RESULT_SUCCESS;
    }
    return RESULT_UNKNOWN_ERROR;
}

//--MODEL--------------------------------------------------------------------------------

sg_vertex_layout_state pnt_vtx_layout() {
    return (sg_vertex_layout_state) {
        .buffers[0].stride = sizeof(VertexPNT),
        .attrs = {
            [0].format = SG_VERTEXFORMAT_FLOAT3,
            [1].format = SG_VERTEXFORMAT_FLOAT3,
            [2].format = SG_VERTEXFORMAT_FLOAT2,
        },
    };
}

sg_vertex_layout_state skinned_vtx_layout() {
    return (sg_vertex_layout_state) {
        .buffers = {
            [0].stride = sizeof(VertexPNT),
            [1].stride = sizeof(VertexSkin),
        },
        .attrs = {
            [0] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT3},
            [1] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT3},
            [2] = {.buffer_index = 0, .format = SG_VERTEXFORMAT_FLOAT2},
            [3] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_UBYTE4},
            [4] = {.buffer_index = 1, .format = SG_VERTEXFORMAT_UBYTE4N},
        }
    };
}


Result load_model(ArenaAlloc *alloc, Model* out, const IoMemory* mem) {
    assert(mem && mem->size > sizeof(iqmheader));
    iqmheader* header = (iqmheader*)mem->ptr;

    {
        uint32_t* fields = &header->version;
        int n = (sizeof(iqmheader) - sizeof(header->magic)) / sizeof(uint32_t);
        for (int i = 0; i < n; i++) fields[i] = tole32(fields[i]);
    }

    if (header->num_vertexarrays) {
        uint32_t* p = (uint32_t*)(mem->ptr + header->ofs_vertexarrays);
        for (unsigned int i = 0; i < header->num_vertexarrays * sizeof(iqmvertexarray) / sizeof(uint32_t); i++) p[i] = tole32(p[i]);
    }
    if (header->num_triangles) {
        uint32_t* p = (uint32_t*)(mem->ptr + header->ofs_triangles);
        for (unsigned int i = 0; i < header->num_triangles * sizeof(iqmtriangle) / sizeof(uint32_t); i++) p[i] = tole32(p[i]);
    }
    if (header->num_meshes) {
        uint32_t* p = (uint32_t*)(mem->ptr + header->ofs_meshes);
        for (unsigned int i = 0; i < header->num_meshes * sizeof(iqmmesh) / sizeof(uint32_t); i++) p[i] = tole32(p[i]);
    }
    if (header->num_joints) {
        uint32_t* p = (uint32_t*)(mem->ptr + header->ofs_joints);
        for (unsigned int i = 0; i < header->num_joints * sizeof(iqmjoint) / sizeof(uint32_t); i++) p[i] = tole32(p[i]);
    }
    if (header->ofs_bounds) {
        uint32_t* p = (uint32_t*)(mem->ptr + header->ofs_bounds);
        for (unsigned int i = 0; i < header->num_frames * sizeof(iqmbounds) / sizeof(uint32_t); i++) p[i] = tole32(p[i]);
    }

    iqmmesh* imesh = (iqmmesh*)(mem->ptr + header->ofs_meshes);
    iqmtriangle* tri = (iqmtriangle*)(mem->ptr + header->ofs_triangles);
    iqmvertexarray* va = (iqmvertexarray*)(mem->ptr + header->ofs_vertexarrays);

    memset(out, 0, sizeof(Model));
    out->meshes_count = header->num_meshes > 4 ? 4 : (int)header->num_meshes;

    uint32_t total_verts = header->num_vertexes;
    VertexPNT* vertices = arena_alloc(alloc, sizeof(VertexPNT) * total_verts, alignof(VertexPNT));
    VertexSkin* skin = arena_alloc(alloc, sizeof(VertexSkin) * total_verts, alignof(VertexSkin));
    if (!vertices || !skin) return RESULT_NOMEM;
    memset(vertices, 0, sizeof(VertexPNT) * total_verts);
    memset(skin, 0, sizeof(VertexSkin) * total_verts);

    bool has_skin = false;
    for (unsigned int i = 0; i < header->num_vertexarrays; i++) {
        switch (va[i].type) {
        case IQM_POSITION: {
            assert(va[i].format == IQM_FLOAT && va[i].size == 3);
            float* positions = (float*)(mem->ptr + va[i].offset);
            { uint32_t* p = (uint32_t*)positions; for (unsigned int k = 0; k < 3 * total_verts; k++) p[k] = tole32(p[k]); }
            for (unsigned int v = 0; v < total_verts; v++) {
                vertices[v].pos = HMM_V3(positions[v*3+0], positions[v*3+1], positions[v*3+2]);
            }
        } break;
        case IQM_NORMAL: {
            assert(va[i].format == IQM_FLOAT && va[i].size == 3);
            float* normals = (float*)(mem->ptr + va[i].offset);
            { uint32_t* p = (uint32_t*)normals; for (unsigned int k = 0; k < 3 * total_verts; k++) p[k] = tole32(p[k]); }
            for (unsigned int v = 0; v < total_verts; v++) {
                vertices[v].nrm = HMM_V3(normals[v*3+0], normals[v*3+1], normals[v*3+2]);
            }
        } break;
        case IQM_TEXCOORD: {
            assert(va[i].format == IQM_FLOAT && va[i].size == 2);
            float* uvs = (float*)(mem->ptr + va[i].offset);
            { uint32_t* p = (uint32_t*)uvs; for (unsigned int k = 0; k < 2 * total_verts; k++) p[k] = tole32(p[k]); }
            for (unsigned int v = 0; v < total_verts; v++) {
                vertices[v].uv = HMM_V2(uvs[v*2+0], uvs[v*2+1]);
            }
        } break;
        case IQM_BLENDINDEXES: {
            assert(va[i].format == IQM_UBYTE && va[i].size == 4);
            has_skin = true;
            uint8_t* bi = (uint8_t*)(mem->ptr + va[i].offset);
            for (unsigned int v = 0; v < total_verts; v++) {
                skin[v].indices[0] = bi[v*4+0]; skin[v].indices[1] = bi[v*4+1];
                skin[v].indices[2] = bi[v*4+2]; skin[v].indices[3] = bi[v*4+3];
            }
        } break;
        case IQM_BLENDWEIGHTS: {
            assert(va[i].format == IQM_UBYTE && va[i].size == 4);
            uint8_t* bw = (uint8_t*)(mem->ptr + va[i].offset);
            for (unsigned int v = 0; v < total_verts; v++) {
                skin[v].weights[0] = bw[v*4+0]; skin[v].weights[1] = bw[v*4+1];
                skin[v].weights[2] = bw[v*4+2]; skin[v].weights[3] = bw[v*4+3];
            }
        } break;
        }
    }

    if (header->ofs_bounds) {
        iqmbounds* bounds = (iqmbounds*)(mem->ptr + header->ofs_bounds);
        memcpy(out->bounds.min, bounds->bbmin, sizeof(float) * 3);
        memcpy(out->bounds.max, bounds->bbmax, sizeof(float) * 3);
        out->bounds.radius_xy = bounds->xyradius;
        out->bounds.radius = bounds->radius;
    }

    for (int m = 0; m < out->meshes_count; m++) {
        uint32_t num_indices = imesh[m].num_triangles * 3;
        uint32_t* mesh_indices = arena_alloc(alloc, sizeof(uint32_t) * num_indices, alignof(uint32_t));
        if (!mesh_indices) return RESULT_NOMEM;

        int tcounter = 0;
        for (unsigned int i = imesh[m].first_triangle;
             i < imesh[m].first_triangle + imesh[m].num_triangles; i++) {
            mesh_indices[tcounter++] = tri[i].vertex[0] - imesh[m].first_vertex;
            mesh_indices[tcounter++] = tri[i].vertex[1] - imesh[m].first_vertex;
            mesh_indices[tcounter++] = tri[i].vertex[2] - imesh[m].first_vertex;
        }

        VertexPNT* mesh_verts = &vertices[imesh[m].first_vertex];
        VertexSkin* mesh_skin = &skin[imesh[m].first_vertex];

        out->meshes[m].vbufs[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = (sg_range){ mesh_verts, sizeof(VertexPNT) * imesh[m].num_vertexes },
            .label = "iqm vertex buffer"
        });

        if (has_skin) {
            out->meshes[m].vbufs[1] = sg_make_buffer(&(sg_buffer_desc){
                .data = (sg_range){ mesh_skin, sizeof(VertexSkin) * imesh[m].num_vertexes },
                .label = "iqm skin buffer"
            });
        }

        out->meshes[m].ibuf = sg_make_buffer(&(sg_buffer_desc){
            .usage.index_buffer = true,
            .data = (sg_range){ mesh_indices, sizeof(uint32_t) * num_indices },
            .label = "iqm index buffer"
        });

        out->meshes[m].first_element = 0;
        out->meshes[m].element_count = num_indices;
    }

    LOG_INFO("Loaded IQM model (%d meshes, %u verts)\n", out->meshes_count, total_verts);

    return RESULT_SUCCESS;
}

void release_model(Model* model) {
    for (int m = 0; m < model->meshes_count; m++) {
        for (int i = 0; i < MESH_MAX_VBUFS; ++i) {
            sg_destroy_buffer(model->meshes[m].vbufs[i]);
        }
        sg_destroy_buffer(model->meshes[m].ibuf);
    }
    memset(model, 0, sizeof(Model));
}

//--ANIMATION----------------------------------------------------------------------------


static HMM_Mat4 HMM_TRS(HMM_Vec3 pos, HMM_Quat rotation, HMM_Vec3 scale) {
    HMM_Mat4 T = HMM_Translate(pos);
    HMM_Mat4 R = HMM_QToM4(rotation);
    HMM_Mat4 S = HMM_Scale(scale);
    return HMM_MulM4(HMM_MulM4(T, R), S);
}

Result load_anims(ArenaAlloc* allocator, AnimSet* out, const IoMemory* mem) {
    assert(mem && mem->size > sizeof(iqmheader));
    iqmheader* hdr = (iqmheader*)mem->ptr;

    if (!hdr->num_joints || !hdr->num_poses) {
        LOG_ERROR("IQM data does not contain skeleton!\n");
        return RESULT_INVALID_PARAMS;
    }

    iqmjoint* joints = (iqmjoint*)&mem->ptr[hdr->ofs_joints];
    out->num_joints = hdr->num_joints;
    out->joint_parents = arena_alloc(allocator, sizeof(int) * hdr->num_joints, alignof(int));
    if (!out->joint_parents) return RESULT_NOMEM;

    for (int i = 0; i < (int)hdr->num_joints; i++) {
        out->joint_parents[i] = joints[i].parent;
    }

    //validate poses and joints
    if ((int)hdr->num_poses != out->num_joints) {
        LOG_ERROR("IQM poses (%u) don't match joints (%d)!\n", hdr->num_poses, out->num_joints);
        return RESULT_INVALID_PARAMS;
    }

    const char* str = hdr->ofs_text ? (char*)&mem->ptr[hdr->ofs_text] : "";
    iqmanim* iqm_anims = (iqmanim*)&mem->ptr[hdr->ofs_anims];
    iqmpose* poses = (iqmpose*)&mem->ptr[hdr->ofs_poses];

    {
        uint32_t* p = (uint32_t*)poses;
        for (unsigned int i = 0; i < hdr->num_poses * sizeof(iqmpose) / sizeof(uint32_t); i++) p[i] = tole32(p[i]);
    }
    {
        uint32_t* p = (uint32_t*)iqm_anims;
        for (unsigned int i = 0; i < hdr->num_anims * sizeof(iqmanim) / sizeof(uint32_t); i++) p[i] = tole32(p[i]);
    }

    //allocate persistent data first
    out->num_anims = hdr->num_anims;
    out->num_frames = hdr->num_frames;
    out->anims = arena_alloc(allocator, sizeof(AnimInfo) * hdr->num_anims, alignof(AnimInfo));
    out->frames = arena_alloc(allocator, sizeof(HMM_Mat4) * hdr->num_frames * hdr->num_poses, alignof(HMM_Mat4));
    if (!out->anims || !out->frames) return RESULT_NOMEM;

    //temp baseframe/inverse_baseframe allocated last so we can pop it after pre-baking
    HMM_Mat4* temp = arena_alloc(allocator, sizeof(HMM_Mat4) * hdr->num_joints * 2, alignof(HMM_Mat4));
    if (!temp) return RESULT_NOMEM;
    HMM_Mat4* baseframe = temp;
    HMM_Mat4* inverse_baseframe = temp + hdr->num_joints;

    for (int i = 0; i < (int)hdr->num_joints; i++) {
        iqmjoint* j = &joints[i];
        HMM_Quat rot = HMM_NormQ(HMM_Q(j->rotate[0], j->rotate[1], j->rotate[2], j->rotate[3]));
        HMM_Vec3 pos = HMM_V3(j->translate[0], j->translate[1], j->translate[2]);
        HMM_Vec3 scl = HMM_V3(j->scale[0], j->scale[1], j->scale[2]);
        baseframe[i] = HMM_TRS(pos, rot, scl);
        inverse_baseframe[i] = HMM_InvGeneralM4(baseframe[i]);
        if (j->parent >= 0) {
            baseframe[i] = HMM_MulM4(baseframe[j->parent], baseframe[i]);
            inverse_baseframe[i] = HMM_MulM4(inverse_baseframe[i], inverse_baseframe[j->parent]);
        }
    }

    uint16_t* framedata = (uint16_t*)&mem->ptr[hdr->ofs_frames];
    {
        unsigned int n = hdr->num_frames * hdr->num_framechannels;
        for (unsigned int i = 0; i < n; i++) framedata[i] = tole16(framedata[i]);
    }

    //just pre-bake the whole shabang
    for (int i = 0; i < (int)hdr->num_frames; i++) {
        for (int j = 0; j < (int)hdr->num_poses; j++) {
            iqmpose* p = &poses[j];
            HMM_Quat rotate;
            HMM_Vec3 translate, scale;
            translate.X = p->channeloffset[0]; if (p->mask & 0x01) translate.X += *framedata++ * p->channelscale[0];
            translate.Y = p->channeloffset[1]; if (p->mask & 0x02) translate.Y += *framedata++ * p->channelscale[1];
            translate.Z = p->channeloffset[2]; if (p->mask & 0x04) translate.Z += *framedata++ * p->channelscale[2];
            rotate.X = p->channeloffset[3]; if (p->mask & 0x08) rotate.X += *framedata++ * p->channelscale[3];
            rotate.Y = p->channeloffset[4]; if (p->mask & 0x10) rotate.Y += *framedata++ * p->channelscale[4];
            rotate.Z = p->channeloffset[5]; if (p->mask & 0x20) rotate.Z += *framedata++ * p->channelscale[5];
            rotate.W = p->channeloffset[6]; if (p->mask & 0x40) rotate.W += *framedata++ * p->channelscale[6];
            scale.X = p->channeloffset[7]; if (p->mask & 0x80) scale.X += *framedata++ * p->channelscale[7];
            scale.Y = p->channeloffset[8]; if (p->mask & 0x100) scale.Y += *framedata++ * p->channelscale[8];
            scale.Z = p->channeloffset[9]; if (p->mask & 0x200) scale.Z += *framedata++ * p->channelscale[9];

            rotate = HMM_NormQ(rotate);
            HMM_Mat4 m = HMM_TRS(translate, rotate, scale);
            if (p->parent >= 0)
                out->frames[i * hdr->num_poses + j] = HMM_MulM4(HMM_MulM4(baseframe[p->parent], m), inverse_baseframe[j]);
            else
                out->frames[i * hdr->num_poses + j] = HMM_MulM4(m, inverse_baseframe[j]);
        }
    }

    //temporary baseframe/inverse_baseframe
    arena_pop(allocator);

    //copy metadata
    for (int i = 0; i < (int)hdr->num_anims; i++) {
        iqmanim* a = &iqm_anims[i];
        out->anims[i].first_frame = a->first_frame;
        out->anims[i].num_frames = a->num_frames;
        out->anims[i].framerate = a->framerate;
        strncpy(out->anims[i].name, &str[a->name], MAX_NAME_LEN - 1);
        out->anims[i].name[MAX_NAME_LEN - 1] = '\0';

        LOG_INFO("Loaded anim: %s (frames %u-%u, fps: %.1f)\n",
            out->anims[i].name,
            out->anims[i].first_frame,
            out->anims[i].first_frame + out->anims[i].num_frames - 1,
            out->anims[i].framerate
        );
    }

    return RESULT_SUCCESS;
}

void update_anim_state(AnimState* state, AnimSet* set, float dt) {
    if (!(state->flags & ANIM_FLAG_PLAY) || state->anim < 0 || state->anim >= set->num_anims) return;

    AnimInfo* anim = &set->anims[state->anim];
    state->current_frame += anim->framerate * dt;

    if (state->flags & ANIM_FLAG_LOOP) {
        float loop_point = (float)(anim->num_frames - 1);
        if (state->current_frame >= loop_point) {
            state->current_frame = fmodf(state->current_frame, loop_point);
        }
    } else {
        if (state->current_frame >= (float)(anim->num_frames - 1)) {
            state->current_frame = (float)(anim->num_frames - 1);
            state->flags &= ~ANIM_FLAG_PLAY;
        }
    }
}

void play_anim(u_skeleton_t* out, AnimSet* set, AnimState* state) {
    if (!out || !set || !state || state->anim < 0 || state->anim >= set->num_anims) return;
    if (set->num_frames <= 0) return;

    AnimInfo* anim = &set->anims[state->anim];
    float curframe = state->current_frame;

    int frame1 = (int)floor(curframe);
    int frame2 = frame1 + 1;
    float frameoffset = curframe - frame1;

    if (state->flags & ANIM_FLAG_LOOP) {
        frame1 = frame1 % (int)anim->num_frames;
        frame2 = frame2 % (int)anim->num_frames;
    } else {
        if (frame1 >= (int)anim->num_frames) frame1 = anim->num_frames - 1;
        if (frame2 >= (int)anim->num_frames) frame2 = anim->num_frames - 1;
    }

    int global_frame1 = anim->first_frame + frame1;
    int global_frame2 = anim->first_frame + frame2;

    HMM_Mat4* mat1 = &set->frames[global_frame1 * set->num_joints];
    HMM_Mat4* mat2 = &set->frames[global_frame2 * set->num_joints];

    for (int i = 0; i < set->num_joints; i++) {
        HMM_Mat4 first = HMM_MulM4F(mat1[i], 1.0f - frameoffset);
        HMM_Mat4 second = HMM_MulM4F(mat2[i], frameoffset);
        HMM_Mat4 mat = HMM_AddM4(first, second);

        if (set->joint_parents[i] >= 0) {
            out->bones[i] = HMM_MulM4(out->bones[set->joint_parents[i]], mat);
        } else {
            out->bones[i] = mat;
        }
    }
}

void blend_anims(u_skeleton_t* out_a, const u_skeleton_t* out_b, float weight, int num_joints) {
    if (!out_a || !out_b) return;
    for (int i = 0; i < num_joints; i++) {
        HMM_Mat4 a = HMM_MulM4F(out_a->bones[i], 1.0f - weight);
        HMM_Mat4 b = HMM_MulM4F(out_b->bones[i], weight);
        out_a->bones[i] = HMM_AddM4(a, b);
    }
}


//--GFX----------------------------------------------------------------------------------

RenderContext* gfx_new_context(Allocator* alloc, const RenderContextDesc* desc) {
    RenderContext* ctx = core_alloc(alloc, sizeof(RenderContext), alignof(RenderContext));
    if (!ctx) {
        LOG_ERROR("Failed to allocate RenderContext\n");
        return NULL;
    }
    memset(ctx, 0, sizeof(RenderContext));

    //init anims
    hp_Handle* anim_dense = core_alloc(alloc, desc->max_anim_sets * sizeof(hp_Handle), alignof(hp_Handle));
    int* anim_sparse = core_alloc(alloc, desc->max_anim_sets * sizeof(int), alignof(int));
    if (!anim_dense || !anim_sparse || !hp_init(&ctx->anims.pool, anim_dense, anim_sparse, desc->max_anim_sets)) {
        LOG_ERROR("Failed to initialize animation pool\n");
        core_free(alloc, ctx);
        return NULL;
    }
    ctx->anims.data = core_alloc(alloc, desc->max_anim_sets * sizeof(AnimSet), alignof(AnimSet));
    if (!ctx->anims.data) {
        LOG_ERROR("Failed to allocate animation data\n");
        return NULL;
    }
    void* anim_arena_buffer = core_alloc(alloc, desc->max_anim_data, 1);
    if (!anim_arena_buffer || !arena_init(&ctx->anims.alloc, anim_arena_buffer, desc->max_anim_data)) {
        LOG_ERROR("Failed to initialize animation arena\n");
        return NULL;
    }

    //init meshes
    hp_Handle* mesh_dense = core_alloc(alloc, desc->max_meshes * sizeof(hp_Handle), alignof(hp_Handle));
    int* mesh_sparse = core_alloc(alloc, desc->max_meshes * sizeof(int), alignof(int));
    if (!mesh_dense || !mesh_sparse || !hp_init(&ctx->meshes.pool, mesh_dense, mesh_sparse, desc->max_meshes)) {
        LOG_ERROR("Failed to initialize mesh pool\n");
        return NULL;
    }
    ctx->meshes.data = core_alloc(alloc, desc->max_meshes * sizeof(Model), alignof(Model));
    if (!ctx->meshes.data) {
        LOG_ERROR("Failed to allocate mesh data\n");
        return NULL;
    }

    //init textures
    hp_Handle* texture_dense = core_alloc(alloc, desc->max_textures * sizeof(hp_Handle), alignof(hp_Handle));
    int* texture_sparse = core_alloc(alloc, desc->max_textures * sizeof(int), alignof(int));
    if (!texture_dense || !texture_sparse || !hp_init(&ctx->textures.pool, texture_dense, texture_sparse, desc->max_textures)) {
        LOG_ERROR("Failed to initialize texture pool\n");
        return NULL;
    }
    ctx->textures.data = core_alloc(alloc, desc->max_textures * sizeof(Texture), alignof(Texture));
    if (!ctx->textures.data) {
        LOG_ERROR("Failed to allocate texture data\n");
        return NULL;
    }

    ctx->offscreen.width = desc->width;
    ctx->offscreen.height = desc->height;

    sg_setup(&(sg_desc){
        .environment = desc->environment,
        .logger.func = slog_func,
    });

    if (sg_isvalid() == false) {
        LOG_ERROR("Failed to initialize sokol_gfx!\n");
        return NULL;
    }

    sgl_setup(&(sgl_desc_t){
        .sample_count = 1,
        .logger.func = slog_func,
        .color_format = SG_PIXELFORMAT_RGBA8,
        .depth_format = SG_PIXELFORMAT_DEPTH,
    });

    ctx->offscreen.physics_pip = sgl_make_pipeline(&(sg_pipeline_desc) {
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
        },
    });

    sdtx_setup(&(sdtx_desc_t) {
        .context = {
            .canvas_width = (float)ctx->offscreen.width,
            .canvas_height = (float)ctx->offscreen.height,
            .depth_format = SG_PIXELFORMAT_DEPTH,
            .color_format = SG_PIXELFORMAT_RGBA8
        },
        .logger.func = slog_func,
        .fonts[0] = sdtx_font_c64(),
    });

    ctx->display.action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f },
        }
    };

    const float dsp_vertices[] = {
         1.0f,  1.0f,   1.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
        -1.0f,  1.0f,   0.0f, 1.0f
    };

    const uint16_t dsp_indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    ctx->offscreen.color_img = sg_make_image(&(sg_image_desc){
        .usage.color_attachment = true,
        .width = ctx->offscreen.width,
        .height = ctx->offscreen.height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 1,
    });

    ctx->offscreen.depth_img = sg_make_image(&(sg_image_desc){
        .usage.depth_stencil_attachment = true,
        .width = ctx->offscreen.width,
        .height = ctx->offscreen.height,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 1,
    });

    ctx->offscreen.pass = (sg_pass){
        .attachments = {
            .colors[0] = sg_make_view(&(sg_view_desc){
                .color_attachment = ctx->offscreen.color_img,
            }),
            .depth_stencil = sg_make_view(&(sg_view_desc){
                .depth_stencil_attachment = ctx->offscreen.depth_img,
            }),
        },
        .action.colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.1f, 0.1f, 0.1f, 1.0f },
        }
    };

    ctx->display.rect = (sg_bindings) {
        .vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(dsp_vertices),
            .usage.immutable = true,
        }),
        .index_buffer = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(dsp_indices),
            .usage.immutable = true,
            .usage.index_buffer = true,
        }),
        .views[0] = sg_make_view(&(sg_view_desc){
            .texture.image = ctx->offscreen.color_img,
        }),
        .views[1] = sg_make_view(&(sg_view_desc){
            .texture.image = ctx->offscreen.depth_img,
        }),
        .samplers[0] = sg_make_sampler(&(sg_sampler_desc){
            .mag_filter = SG_FILTER_NEAREST,
            .min_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        }),
        .samplers[1] = sg_make_sampler(&(sg_sampler_desc){
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        }),
    };

    ctx->display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [ATTR_display_shd_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_display_shd_uv].format = SG_VERTEXFORMAT_FLOAT2,
            }
        },
        .shader = sg_make_shader(display_shd_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLES,
        .label = "display_pip",
    });

    ctx->offscreen.pip[GFX_PIP_DEFAULT] = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = pnt_vtx_layout(),
        .shader = sg_make_shader(tex_lit_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT32,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
    });

    ctx->offscreen.pip[GFX_PIP_SKINNED] = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout = skinned_vtx_layout(),
        .shader = sg_make_shader(tex_lit_skinned_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT32,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
    });

    //CUBEMAP

    ctx->offscreen.pip[GFX_PIP_CUBEMAP] = sg_make_pipeline(&(sg_pipeline_desc) {
        .layout.attrs = {
            [ATTR_cubemap_pos] = {.format = SG_VERTEXFORMAT_FLOAT3, .buffer_index = 0},
        },
        .shader = sg_make_shader(cubemap_shader_desc((sg_query_backend()))),
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
    });

    ctx->offscreen.light = (u_dir_light_t) {
        .ambient = {0.2f, 0.2f, 0.2f, 1.0f},
        .diffuse = {1.0f, 1.0f, 1.0f, 1.0f},
        .direction = {0.25f, -0.75f, -0.25f},
    };

    ctx->offscreen.default_sampler = sg_make_sampler(&(sg_sampler_desc){
        .wrap_u = SG_WRAP_CLAMP_TO_BORDER,
        .wrap_v = SG_WRAP_CLAMP_TO_BORDER,
    });

    LOG_INFO("Graphics initialized.\n");
    return ctx;
}


void gfx_render(RenderContext* ctx, Scene* scene, Camera* cam, sg_swapchain swapchain, float dt) {

    u_skeleton_t u_skel = {0};
    u_skeleton_t u_skel_prev = {0};
    AnimState* anim_states = scene->anim_states;
    AnimState* prev_anim_states = scene->prev_anim_states;
    float* blend_weights = scene->anim_blend_weights;

    u_vs_params_t u_vs = {
        .view = camera_view_mtx(cam),
        .proj = camera_proj_mtx(cam, ctx->offscreen.width, ctx->offscreen.height),
    };

    //offscreen pass
    sg_begin_pass(&ctx->offscreen.pass);

    if(ctx->offscreen.cubemap.vbuf.id != SG_INVALID_ID) {
        sg_apply_pipeline(ctx->offscreen.pip[GFX_PIP_CUBEMAP]);
        sg_bindings binds = {0};
        binds.vertex_buffers[0] = ctx->offscreen.cubemap.vbuf;
        binds.index_buffer = ctx->offscreen.cubemap.ibuf;
        binds.views[0] = ctx->offscreen.cubemap.tex.view;
        binds.samplers[0] = ctx->offscreen.cubemap.smp;
        sg_apply_bindings(&binds);
        u_vs.model = HMM_Scale(HMM_V3(500, 500, 500));
        sg_apply_uniforms(UB_u_vs_params, &SG_RANGE(u_vs));
        sg_draw(0, 36, 1);
    }

    sg_apply_pipeline(ctx->offscreen.pip[GFX_PIP_SKINNED]);
    sg_apply_uniforms(UB_u_dir_light, &SG_RANGE(ctx->offscreen.light));

    for (int i = 0; i < scene->pool.count; i++) {
        Entity handle = { .id = hp_handle_at(&scene->pool, i) };
        int idx = hp_index(handle.id);

        uint16_t model_flags = scene->model_flags[idx];
        uint16_t anim_flags = scene->anim_flags[idx];

        if ((model_flags & ENTITY_HAS_MODEL) && (anim_flags & ENTITY_HAS_ANIM)) {
            if (scene->models[idx].id == 0) continue;

            int mdl_idx = hp_index(scene->models[idx].id);
            Model* model = &ctx->meshes.data[mdl_idx];

            u_vs.model = entity_mtx(scene, handle);

            int anim_idx = hp_index(scene->anims[idx].id);
            AnimSet* set = &ctx->anims.data[anim_idx];

            //advance and sample current animation
            update_anim_state(&anim_states[idx], set, dt);
            memset(&u_skel, 0, sizeof(u_skel));
            play_anim(&u_skel, set, &anim_states[idx]);

            //blend with previous animation if transitioning
            if (blend_weights[idx] < 1.0f) {
                update_anim_state(&prev_anim_states[idx], set, dt);
                memset(&u_skel_prev, 0, sizeof(u_skel_prev));
                play_anim(&u_skel_prev, set, &prev_anim_states[idx]);
                blend_anims(&u_skel, &u_skel_prev, 1.0f - blend_weights[idx], set->num_joints);

                blend_weights[idx] += dt / ANIM_BLEND_DURATION;
                if (blend_weights[idx] > 1.0f) blend_weights[idx] = 1.0f;
            }

            sg_apply_uniforms(UB_u_skeleton, &SG_RANGE(u_skel));
            sg_apply_uniforms(UB_u_vs_params, &SG_RANGE(u_vs));

            for (int j = 0; j < model->meshes_count; j++) {
                Mesh* mesh = &model->meshes[j];
                if (mesh->vbufs[0].id == SG_INVALID_ID || mesh->vbufs[1].id == SG_INVALID_ID) continue;
                sg_bindings binds = {0};
                memcpy(&binds.vertex_buffers, mesh->vbufs, MESH_MAX_VBUFS * sizeof(sg_buffer));
                binds.index_buffer = mesh->ibuf;
                binds.samplers[0] = ctx->offscreen.default_sampler;
                binds.views[0] = ctx->textures.data[hp_index(scene->textures[idx].tex[j].id)].view;
                sg_apply_bindings(&binds);
                sg_draw(0, mesh->element_count, 1);
            }
        }
    }

    sg_apply_pipeline(ctx->offscreen.pip[GFX_PIP_DEFAULT]);
    sg_apply_uniforms(UB_u_dir_light, &SG_RANGE(ctx->offscreen.light));

    for (int i = 0; i < scene->pool.count; i++) {
        Entity handle = { .id = hp_handle_at(&scene->pool, i) };
        int idx = hp_index(handle.id);

        uint16_t model_flags = scene->model_flags[idx];
        uint16_t anim_flags = scene->anim_flags[idx];

        if ((model_flags & ENTITY_HAS_MODEL) && !(anim_flags & ENTITY_HAS_ANIM)) {
            if (scene->models[idx].id == 0) continue;

            int mdl_idx = hp_index(scene->models[idx].id);
            Model* model = &ctx->meshes.data[mdl_idx];

            u_vs.model = entity_mtx(scene, handle);

            sg_apply_uniforms(UB_u_vs_params, &SG_RANGE(u_vs));

            for (int j = 0; j < model->meshes_count; j++) {
                Mesh* mesh = &model->meshes[j];
                if (mesh->vbufs[0].id == SG_INVALID_ID) continue;
                sg_bindings binds = {0};
                memcpy(&binds.vertex_buffers, mesh->vbufs, MESH_MAX_VBUFS * sizeof(sg_buffer));
                binds.index_buffer = mesh->ibuf;
                binds.samplers[0] = ctx->offscreen.default_sampler;
                binds.views[0] = ctx->textures.data[hp_index(scene->textures[idx].tex[0].id)].view;
                sg_apply_bindings(&binds);
                sg_draw(0, mesh->element_count, 1);
            }
        }
    }

    //(debug visualization)
    sgl_defaults();
    sgl_viewport(0, 0, ctx->offscreen.width, ctx->offscreen.height, true);

    sgl_matrix_mode_projection();
    sgl_load_matrix((const float*)&u_vs.proj);
    sgl_matrix_mode_modelview();
    sgl_load_matrix((const float*)&u_vs.view);

    sgl_load_pipeline(ctx->offscreen.physics_pip);

    //draw mouse cursor point when unlocked
    /*
    sgl_load_default_pipeline();
    sgl_matrix_mode_projection();
    sgl_load_identity();
    sgl_matrix_mode_modelview();
    sgl_load_identity();
    sgl_point_size(4.0f);
    sgl_c3b(200, 125, 0);
    sgl_begin_points();
    //if (!sapp_mouse_locked()) {
        float ndc_x = (2.0f * ctx->display.mouse_pos.X / swapchain.width) - 1.0f;
        float ndc_y = 1.0f - (2.0f * ctx->display.mouse_pos.Y / swapchain.height);
        sgl_v2f(ndc_x, ndc_y);
    //}
    sgl_end();
     */
    sgl_draw();
    sdtx_draw();
    sg_end_pass();

    //display pass
    sg_begin_pass(&(sg_pass) {
        .swapchain = swapchain,
        .action.colors[0] = {
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f },
            .load_action = SG_LOADACTION_CLEAR,
        }
    });
    sg_apply_pipeline(ctx->display.pip);

    display_vs_params_t vs_params = {
        .resolution = HMM_V2((float)swapchain.width, (float)swapchain.height),
        .offscreen_size = HMM_V2((float)ctx->offscreen.width, (float)ctx->offscreen.height),
    };

    sg_apply_uniforms(UB_display_vs_params, &SG_RANGE(vs_params));
    sg_apply_bindings(&ctx->display.rect);
    sg_draw(0, 6, 1);
    sg_end_pass();

    sg_commit();
}

void gfx_shutdown(RenderContext* ctx) {
    sdtx_shutdown();
    sgl_shutdown();
    sg_shutdown();
    LOG_INFO("Graphics shutdown.\n");
}

void gfx_load_cubemap(RenderContext* ctx, ArenaAlloc* alloc, IoMemory* mem) {
    ddsktx_texture_info tc = {0};
    if (!ddsktx_parse(&tc, (const void*)mem->ptr, (int)mem->size, NULL)) {
        LOG_ERROR("Failed to parse cubemap DDS file\n");
        return;
    }

    if (!(tc.flags & DDSKTX_TEXTURE_FLAG_CUBEMAP)) {
        LOG_ERROR("Texture is not a cubemap\n");
        return;
    }

    sg_image_desc desc = {0};
    desc.type = SG_IMAGETYPE_CUBE;
    desc.width = tc.width;
    desc.height = tc.height;
    desc.num_mipmaps = tc.num_mips;
    desc.pixel_format = dds_to_sg_pixelformt(tc.format);

    //for cubemaps, each mip level contains all 6 faces packed contiguously
    // Order: +X, -X, +Y, -Y, +Z, -Z
    for (int mip = 0; mip < tc.num_mips; mip++) {
        size_t mip_total_size = 0;
        for (int face = 0; face < 6; face++) {
            ddsktx_sub_data sub_data;
            ddsktx_get_sub(&tc, &sub_data, (const void*)mem->ptr, (int)mem->size, 0, face, mip);
            mip_total_size += sub_data.size_bytes;
        }

        uint8_t* mip_buffer = arena_alloc(alloc, mip_total_size, 0);
        assert(mip_buffer);

        size_t offset = 0;
        for (int face = 0; face < 6; face++) {
            ddsktx_sub_data sub_data;
            ddsktx_get_sub(&tc, &sub_data, (const void*)mem->ptr, (int)mem->size, 0, face, mip);
            memcpy(mip_buffer + offset, sub_data.buff, sub_data.size_bytes);
            offset += sub_data.size_bytes;
        }

        desc.data.mip_levels[mip] = (sg_range){mip_buffer, mip_total_size};
    }

    ctx->offscreen.cubemap.tex.image = sg_make_image(&desc);
    ctx->offscreen.cubemap.tex.view = sg_make_view(&(sg_view_desc){
        .texture = ctx->offscreen.cubemap.tex.image,
    });

    ctx->offscreen.cubemap.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_w = SG_WRAP_CLAMP_TO_EDGE,
    });

    const float cube_vertices[] = {
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
    };

    const uint16_t cube_indices[] = {
        0, 1, 2,  0, 2, 3,
        4, 6, 5,  4, 7, 6,
        0, 4, 5,  0, 5, 1,
        2, 6, 7,  2, 7, 3,
        0, 3, 7,  0, 7, 4,
        1, 5, 6,  1, 6, 2,
    };

    ctx->offscreen.cubemap.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_vertices),
        .usage.immutable = true,
    });

    ctx->offscreen.cubemap.ibuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(cube_indices),
        .usage.immutable = true,
        .usage.index_buffer = true,
    });

    LOG_INFO("Loaded cubemap: %dx%d, %d mips\n", tc.width, tc.height, tc.num_mips);
}

ModelHandle gfx_load_model(RenderContext* ctx, ArenaAlloc* alloc, const IoMemory* data) {
    hp_Handle hnd = hp_create_handle(&ctx->meshes.pool);
    if (hnd == HP_INVALID_HANDLE) {
        LOG_ERROR("Failed to allocate handle!");
        return (ModelHandle) {HP_INVALID_HANDLE};
    }
    Result result = load_model(alloc, &ctx->meshes.data[hp_index(hnd)], data);
    if (result != RESULT_SUCCESS) {
        LOG_ERROR("Failed to load model");
        hp_release_handle(&ctx->meshes.pool, hnd);
        return (ModelHandle) {HP_INVALID_HANDLE};
    }
    return (ModelHandle) { hnd };
}

void gfx_release_model(RenderContext* ctx, ModelHandle mesh) {
    int idx = hp_index(mesh.id);
    release_model(&ctx->meshes.data[idx]);
}

TextureHandle gfx_load_texture(RenderContext* ctx, ArenaAlloc* alloc, IoMemory* data) {
    hp_Handle hnd = hp_create_handle(&ctx->textures.pool);
    if (hnd == HP_INVALID_HANDLE) {
        LOG_ERROR("Failed to allocate handle!");
        return (TextureHandle) {HP_INVALID_HANDLE};
    }
    Result result = load_texture(alloc, &ctx->textures.data[hp_index(hnd)], data);
    if (result != RESULT_SUCCESS) {
        LOG_ERROR("Failed to create mesh");
        hp_release_handle(&ctx->textures.pool, hnd);
        return (TextureHandle) {HP_INVALID_HANDLE};
    }
    return (TextureHandle) { hnd };
}

void gfx_release_texture(RenderContext* ctx, TextureHandle tex) {
    int idx = hp_index(tex.id);
    sg_destroy_image(ctx->textures.data[idx].image);
    sg_destroy_view(ctx->textures.data[idx].view);
}

AnimSetHandle gfx_load_anims(RenderContext* ctx, const IoMemory* data) {
    hp_Handle hnd = hp_create_handle(&ctx->anims.pool);
    if (hnd == HP_INVALID_HANDLE) {
        LOG_ERROR("Failed to allocate handle!");
        return (AnimSetHandle) {HP_INVALID_HANDLE};
    }
    Result result = load_anims(&ctx->anims.alloc, &ctx->anims.data[hp_index(hnd)], data);
    if (result != RESULT_SUCCESS) {
        LOG_ERROR("Failed to load anims");
        hp_release_handle(&ctx->anims.pool, hnd);
        return (AnimSetHandle) {HP_INVALID_HANDLE};
    }
    return (AnimSetHandle) { hnd };
}

void gfx_clear_anims(RenderContext* ctx) {
    arena_reset(&ctx->anims.alloc);
    hp_reset(&ctx->anims.pool);
}

//--IMMEDIATE-MODE-HELPERS--------------------------------------------------------------------

static inline HMM_Vec3 transform_point(HMM_Vec3 p, HMM_Vec3 pos, HMM_Quat rot, HMM_Vec3 scale) {
    p = HMM_MulV3(p, scale);
    p = HMM_RotateV3Q(p, rot);
    p = HMM_AddV3(p, pos);
    return p;
}

void imdraw_box(HMM_Vec3 pos, HMM_Quat rot, HMM_Vec3 scale) {
    HMM_Vec3 v[8] = {
        HMM_V3(-0.5f, -0.5f, -0.5f),
        HMM_V3( 0.5f, -0.5f, -0.5f),
        HMM_V3( 0.5f,  0.5f, -0.5f),
        HMM_V3(-0.5f,  0.5f, -0.5f),
        HMM_V3(-0.5f, -0.5f,  0.5f),
        HMM_V3( 0.5f, -0.5f,  0.5f),
        HMM_V3( 0.5f,  0.5f,  0.5f),
        HMM_V3(-0.5f,  0.5f,  0.5f),
    };

    for (int i = 0; i < 8; i++) {
        v[i] = transform_point(v[i], pos, rot, scale);
    }

    sgl_begin_lines();
    //back
    sgl_v3f(v[0].X, v[0].Y, v[0].Z); sgl_v3f(v[1].X, v[1].Y, v[1].Z);
    sgl_v3f(v[1].X, v[1].Y, v[1].Z); sgl_v3f(v[2].X, v[2].Y, v[2].Z);
    sgl_v3f(v[2].X, v[2].Y, v[2].Z); sgl_v3f(v[3].X, v[3].Y, v[3].Z);
    sgl_v3f(v[3].X, v[3].Y, v[3].Z); sgl_v3f(v[0].X, v[0].Y, v[0].Z);
    //front
    sgl_v3f(v[4].X, v[4].Y, v[4].Z); sgl_v3f(v[5].X, v[5].Y, v[5].Z);
    sgl_v3f(v[5].X, v[5].Y, v[5].Z); sgl_v3f(v[6].X, v[6].Y, v[6].Z);
    sgl_v3f(v[6].X, v[6].Y, v[6].Z); sgl_v3f(v[7].X, v[7].Y, v[7].Z);
    sgl_v3f(v[7].X, v[7].Y, v[7].Z); sgl_v3f(v[4].X, v[4].Y, v[4].Z);
    //connecting edges (back to front)
    sgl_v3f(v[0].X, v[0].Y, v[0].Z); sgl_v3f(v[4].X, v[4].Y, v[4].Z);
    sgl_v3f(v[1].X, v[1].Y, v[1].Z); sgl_v3f(v[5].X, v[5].Y, v[5].Z);
    sgl_v3f(v[2].X, v[2].Y, v[2].Z); sgl_v3f(v[6].X, v[6].Y, v[6].Z);
    sgl_v3f(v[3].X, v[3].Y, v[3].Z); sgl_v3f(v[7].X, v[7].Y, v[7].Z);

    sgl_end();
}

void imdraw_cylinder(HMM_Vec3 pos, HMM_Quat rot, float diameter, float height) {
    #define CYL_SEGMENTS 16
    float radius = diameter * 0.5f;
    //tokamak adds radius to cylinder height for collision (rounded caps)
    float half_height = height * 0.5f + radius;

    HMM_Vec3 top[CYL_SEGMENTS];
    HMM_Vec3 bottom[CYL_SEGMENTS];

    for (int i = 0; i < CYL_SEGMENTS; i++) {
        float angle = (float)i / CYL_SEGMENTS * 2.0f * HMM_PI32;
        float x = HMM_CosF(angle) * radius;
        float z = HMM_SinF(angle) * radius;

        HMM_Vec3 top_local = HMM_V3(x, half_height, z);
        HMM_Vec3 bottom_local = HMM_V3(x, -half_height, z);

        top[i] = transform_point(top_local, pos, rot, HMM_V3(1, 1, 1));
        bottom[i] = transform_point(bottom_local, pos, rot, HMM_V3(1, 1, 1));
    }

    sgl_begin_lines();

    for (int i = 0; i < CYL_SEGMENTS; i++) {
        int next = (i + 1) % CYL_SEGMENTS;
        sgl_v3f(top[i].X, top[i].Y, top[i].Z);
        sgl_v3f(top[next].X, top[next].Y, top[next].Z);
    }

    for (int i = 0; i < CYL_SEGMENTS; i++) {
        int next = (i + 1) % CYL_SEGMENTS;
        sgl_v3f(bottom[i].X, bottom[i].Y, bottom[i].Z);
        sgl_v3f(bottom[next].X, bottom[next].Y, bottom[next].Z);
    }

    for (int i = 0; i < CYL_SEGMENTS; i += CYL_SEGMENTS / 4) {
        sgl_v3f(top[i].X, top[i].Y, top[i].Z);
        sgl_v3f(bottom[i].X, bottom[i].Y, bottom[i].Z);
    }

    sgl_end();
    #undef CYL_SEGMENTS
}

//--SFX---------------------------------------------------------------------------------------

Result load_sound_buffer(IoMemory* mem, SoundBuffer* out) {
    tm_create_buffer_vorbis_stream(mem->ptr, (int)mem->size, NULL, NULL, out);
    return RESULT_SUCCESS;
}

static void _sfx_stream_cb(float* buffer, int num_frames, int num_channels, void* udata) {
    (void)num_channels;
    SoundListener* listener = (SoundListener*)udata;

    if (listener->frame_dt > 0.0f) {
        float sample_rate = (float)saudio_sample_rate();
        float audio_dt = (float)num_frames / sample_rate;
        listener->time_since_update += audio_dt;

        //extrapolate target position based on smoothed velocity
        HMM_Vec3 predicted = HMM_AddV3(listener->target_pos,
            HMM_MulV3F(listener->smoothed_velocity, listener->time_since_update));

        //smooth toward predicted position
        float t = expf(-audio_dt / listener->smoothing);
        listener->current_pos = HMM_LerpV3(listener->current_pos, 1.0f - t, predicted);

        // Smooth forward direction (slerp would be better, but lerp + normalize is simpler)
        HMM_Vec3 fwd = HMM_LerpV3(listener->current_forward, 1.0f - t, listener->target_forward);
        float fwd_len = HMM_LenV3(fwd);
        if (fwd_len > 1.0e-8f) {
            listener->current_forward = HMM_DivV3F(fwd, fwd_len);
        }

        tm_update_listener((const float*)listener->current_pos.Elements,
                           (const float*)listener->current_forward.Elements);
    }
    tm_getsamples(buffer, num_frames);
}

AudioContext* sfx_new_context(Allocator* alloc, uint16_t max_buffers) {
    AudioContext* ctx = core_alloc(alloc, sizeof(AudioContext), alignof(AudioContext));
    if (!ctx) return NULL;

    hp_Handle* buffer_dense = core_alloc(alloc, max_buffers * sizeof(hp_Handle), alignof(hp_Handle));
    int* buffer_sparse = core_alloc(alloc, max_buffers * sizeof(int), alignof(int));
    if (!buffer_dense || !buffer_sparse || !hp_init(&ctx->buffers.pool, buffer_dense, buffer_sparse, max_buffers)) {
        return NULL;
    }
    ctx->buffers.data = core_alloc(alloc, max_buffers * sizeof(SoundBuffer), alignof(SoundBuffer));
    if (!ctx->buffers.data) {
        return NULL;
    }

    ctx->listener.current_pos = HMM_V3(0, 0, 0);
    ctx->listener.target_pos = HMM_V3(0, 0, 0);
    ctx->listener.velocity = HMM_V3(0, 0, 0);
    ctx->listener.smoothed_velocity = HMM_V3(0, 0, 0);
    ctx->listener.current_forward = HMM_V3(0, 0, -1);
    ctx->listener.target_forward = HMM_V3(0, 0, -1);
    ctx->listener.frame_dt = 0.016f;
    ctx->listener.time_since_update = 0.0f;
    ctx->listener.smoothing = 0.1f;  // Smoothing time constant in seconds
    ctx->listener.velocity_smoothing = 0.8f;  // Exponential smoothing factor for velocity
    saudio_setup(&(saudio_desc) {
        .num_channels = 2,
        .sample_rate = 44100,
        .stream_userdata_cb = _sfx_stream_cb,
        .user_data = (void*)&ctx->listener,
    });

    tm_callbacks callbacks = {0};
    tm_init(callbacks, saudio_sample_rate());
    LOG_INFO("Audio initialized.\n");
    return ctx;
}

void sfx_update(AudioContext* ctx, HMM_Vec3 listener_pos, HMM_Vec3 listener_forward, Scene* scene, float dt) {
    HMM_Vec3 new_target = HMM_V3(listener_pos.X, listener_pos.Y, listener_pos.Z);

    //calculate velocity from position change
    if (dt > 0.0f) {
        ctx->listener.velocity = HMM_DivV3F(
            HMM_SubV3(new_target, ctx->listener.target_pos), dt);

        //smooth velocity to reduce jitter from frame time variations
        ctx->listener.smoothed_velocity = HMM_AddV3(
            HMM_MulV3F(ctx->listener.smoothed_velocity, ctx->listener.velocity_smoothing),
            HMM_MulV3F(ctx->listener.velocity, 1.0f - ctx->listener.velocity_smoothing)
        );
    }

    ctx->listener.target_pos = new_target;
    ctx->listener.target_forward = listener_forward;
    ctx->listener.frame_dt = dt;
    ctx->listener.time_since_update = 0.0f;

    for (int i = 0; i < scene->pool.count; i++) {
        Entity handle = { .id = hp_handle_at(&scene->pool, i) };
        int idx = hp_index(handle.id);
        uint32_t flags = scene->sound_flags[idx];

        if (!(flags & ENTITY_HAS_SOUND)) continue;

        bool should_play = flags & ENTITY_SOUND_PLAY;
        bool is_playing = flags & ENTITY_SOUND_PLAYING;
        tm_channel channel = scene->sound_channels[idx];

        // Start sound if PLAY flag set but not currently playing
        if (should_play && !is_playing) {
            int buf_idx = hp_index(scene->sound_buffers[idx].id);
            if (ctx->buffers.data[buf_idx]) {
                const tm_buffer* buf = ctx->buffers.data[buf_idx];
                bool loop = flags & ENTITY_SOUND_LOOP;
                bool spatial = flags & ENTITY_SOUND_SPATIAL;
                SoundProps* props = &scene->sound_props[idx];
                bool ok = false;

                if (spatial) {
                    HMM_Vec3 pos = scene->transforms[idx].pos;
                    float audio_pos[3] = { pos.X, pos.Y, pos.Z };
                    if (loop) {
                        ok = tm_add_spatial_loop(buf, 0, props->volume, 1.0f, audio_pos, props->min_range, props->max_range, &channel);
                    } else {
                        ok = tm_add_spatial(buf, 0, props->volume, 1.0f, audio_pos, props->min_range, props->max_range, &channel);
                    }
                } else {
                    if (loop) {
                        ok = tm_add_loop(buf, 0, props->volume, 1.0f, &channel);
                    } else {
                        ok = tm_add(buf, 0, props->volume, 1.0f, &channel);
                    }
                }

                if (ok) {
                    scene->sound_channels[idx] = channel;
                    scene->sound_flags[idx] |= ENTITY_SOUND_PLAYING;
                }
            }
        }

        if (!should_play && is_playing && tm_channel_isvalid(channel)) {
            tm_channel_stop(channel);
            scene->sound_channels[idx] = (tm_channel){0};
            scene->sound_flags[idx] &= ~ENTITY_SOUND_PLAYING;
        }

        if ((flags & ENTITY_SOUND_PLAYING) && tm_channel_isvalid(channel) && (flags & ENTITY_SOUND_SPATIAL)) {
            HMM_Vec3 pos = scene->transforms[idx].pos;
            float audio_pos[3] = { pos.X, pos.Y, pos.Z };
            tm_channel_set_position(channel, audio_pos);
        }

        if ((flags & ENTITY_SOUND_PLAYING) && !tm_channel_isvalid(scene->sound_channels[idx]) && !(flags & ENTITY_SOUND_LOOP)) {
            scene->sound_flags[idx] &= ~(ENTITY_SOUND_PLAYING | ENTITY_SOUND_PLAY);
            scene->sound_channels[idx] = (tm_channel){0};
        }
    }
}

void sfx_shutdown(AudioContext* ctx) {
    if (saudio_isvalid()) {
        tm_stop_all_sources();
/*
        for (int i = 0; i < ctx->buffer_count; i++) {
            if (ctx->buffers[i]) {
                tm_release_buffer(ctx->buffers[i]);
            }
        }
*/
        tm_shutdown();
        saudio_shutdown();
        LOG_INFO("Audio shutdown.\n");
    }
}

SoundBufferHandle sfx_load_buffer(AudioContext* ctx, IoMemory* data) {
    hp_Handle hnd = hp_create_handle(&ctx->buffers.pool);
    if (hnd == HP_INVALID_HANDLE) {
        LOG_ERROR("Failed to allocate handle!");
        return (SoundBufferHandle) {HP_INVALID_HANDLE};
    }
    Result result = load_sound_buffer(data, &ctx->buffers.data[hp_index(hnd)]);
    if (result != RESULT_SUCCESS) {
        LOG_ERROR("Failed to create mesh");
        hp_release_handle(&ctx->buffers.pool, hnd);
        return (SoundBufferHandle) {HP_INVALID_HANDLE};
    }
    return (SoundBufferHandle) { hnd };
}

void sfx_release_buffer(AudioContext* ctx, SoundBufferHandle buf) {
    int idx = hp_index(buf.id);
    tm_release_buffer(ctx->buffers.data[idx]);
}




//--SCENE------------------------------------------------------------------------------------


Scene* scene_new(Allocator* alloc, uint16_t max_things) {
    Scene* t = core_alloc(alloc, sizeof(Scene), alignof(Scene));
    if (!t) return NULL;
    memset(t, 0, sizeof(Scene));

    hp_Handle* dense = core_alloc(alloc, max_things * sizeof(hp_Handle), alignof(hp_Handle));
    if (!dense) return NULL;
    int* sparse = core_alloc(alloc, max_things * sizeof(int), alignof(int));
    if (!sparse) return NULL;
    if (!hp_init(&t->pool, dense, sparse, max_things)) return NULL;

    t->relation_flags = core_alloc(alloc, max_things * sizeof(RelationFlags), alignof(RelationFlags));
    if (!t->relation_flags) return NULL;
    t->transforms = core_alloc(alloc, max_things * sizeof(Transform), alignof(Transform));
    if (!t->transforms) return NULL;
    t->parents = core_alloc(alloc, max_things * sizeof(Entity), alignof(Entity));
    if (!t->parents) return NULL;
    t->childs = core_alloc(alloc, max_things * sizeof(Children), alignof(Children));
    if (!t->childs) return NULL;

    t->model_flags = core_alloc(alloc, max_things * sizeof(ModelFlags), alignof(ModelFlags));
    if (!t->model_flags) return NULL;
    t->models = core_alloc(alloc, max_things * sizeof(ModelHandle), alignof(ModelHandle));
    if (!t->models) return NULL;
    t->textures = core_alloc(alloc, max_things * sizeof(TextureSet), alignof(TextureSet));
    if (!t->textures) return NULL;

    t->anim_flags = core_alloc(alloc, max_things * sizeof(AnimFlags), alignof(AnimFlags));
    if (!t->anim_flags) return NULL;
    t->anims = core_alloc(alloc, max_things * sizeof(AnimSetHandle), alignof(AnimSetHandle));
    if (!t->anims) return NULL;
    t->anim_states = core_alloc(alloc, max_things * sizeof(AnimState), alignof(AnimState));
    if (!t->anim_states) return NULL;
    t->prev_anim_states = core_alloc(alloc, max_things * sizeof(AnimState), alignof(AnimState));
    if (!t->prev_anim_states) return NULL;
    t->anim_blend_weights = core_alloc(alloc, max_things * sizeof(float), alignof(float));
    if (!t->anim_blend_weights) return NULL;

    t->sound_flags = core_alloc(alloc, max_things * sizeof(SoundFlags), alignof(SoundFlags));
    if (!t->sound_flags) return NULL;
    t->sound_buffers = core_alloc(alloc, max_things * sizeof(SoundBufferHandle), alignof(SoundBufferHandle));
    if (!t->sound_buffers) return NULL;
    t->sound_channels = core_alloc(alloc, max_things * sizeof(tm_channel), alignof(tm_channel));
    if (!t->sound_channels) return NULL;
    t->sound_props = core_alloc(alloc, max_things * sizeof(SoundProps), alignof(SoundProps));
    if (!t->sound_props) return NULL;

    memset(t->relation_flags, 0, max_things * sizeof(RelationFlags));
    memset(t->parents, 0, max_things * sizeof(Entity));
    memset(t->childs, 0, max_things * sizeof(Children));
    memset(t->model_flags, 0, max_things * sizeof(ModelFlags));
    memset(t->models, 0, max_things * sizeof(ModelHandle));
    memset(t->anim_flags, 0, max_things * sizeof(AnimFlags));
    memset(t->anims, 0, max_things * sizeof(AnimSetHandle));
    memset(t->anim_states, 0, max_things * sizeof(AnimState));
    memset(t->prev_anim_states, 0, max_things * sizeof(AnimState));
    for (int i = 0; i < max_things; i++) {
        t->anim_blend_weights[i] = 1.0f;
    }
    memset(t->sound_flags, 0, max_things * sizeof(SoundFlags));
    memset(t->sound_buffers, 0, max_things * sizeof(SoundBufferHandle));
    memset(t->sound_channels, 0, max_things * sizeof(tm_channel));
    memset(t->sound_props, 0, max_things * sizeof(SoundProps));

    //note: using direct field access instead of HMM macros to avoid issues with TLSF allocator
    for (int i = 0; i < max_things; i++) {
        t->transforms[i].pos = HMM_V3(0, 0, 0);
        t->transforms[i].scale = HMM_V3(1, 1, 1);
        t->transforms[i].rot = HMM_Q(0, 0, 0, 1);
    }

    return t;
}

void scene_destroy(Allocator* alloc, Scene* scene) {
    if (!scene) return;
    core_free(alloc, scene->pool.dense);
    core_free(alloc, scene->pool.sparse);

    core_free(alloc, scene->relation_flags);
    core_free(alloc, scene->transforms);
    core_free(alloc, scene->parents);
    core_free(alloc, scene->childs);

    core_free(alloc, scene->model_flags);
    core_free(alloc, scene->models);
    core_free(alloc, scene->textures);

    core_free(alloc, scene->anim_flags);
    core_free(alloc, scene->anims);
    core_free(alloc, scene->anim_states);
    core_free(alloc, scene->prev_anim_states);
    core_free(alloc, scene->anim_blend_weights);

    core_free(alloc, scene->sound_flags);
    core_free(alloc, scene->sound_buffers);
    core_free(alloc, scene->sound_channels);
    core_free(alloc, scene->sound_props);

    core_free(alloc, scene);
}

Entity entity_new(Scene* scene) {
    hp_Handle h = hp_create_handle(&scene->pool);
    return (Entity){ .id = h };
}

bool entity_valid(Scene* scene, Entity entity) {
    return hp_valid_handle(&scene->pool, entity.id);
}

void entity_destroy(Scene* scene, Entity entity) {
    if (!entity_valid(scene, entity)) return;

    int idx = hp_index(entity.id);

    scene->relation_flags[idx] = 0;
    scene->transforms[idx].pos = HMM_V3(0, 0, 0);
    scene->transforms[idx].rot = HMM_Q(0, 0, 0, 1);
    scene->transforms[idx].scale = HMM_V3(1, 1, 1);
    scene->parents[idx].id = 0;
    memset(&scene->childs[idx], 0, sizeof(Children));

    scene->model_flags[idx] = 0;
    scene->models[idx] = (ModelHandle){ 0 };

    scene->anim_flags[idx] = 0;
    scene->anims[idx] = (AnimSetHandle) { 0 };
    memset(&scene->anim_states[idx], 0, sizeof(AnimState));
    memset(&scene->prev_anim_states[idx], 0, sizeof(AnimState));
    scene->anim_blend_weights[idx] = 1.0f;

    scene->sound_flags[idx] = 0;
    scene->sound_buffers[idx] = (SoundBufferHandle) { 0 };
    scene->sound_channels[idx] = (SoundChannel){0};
    scene->sound_props[idx] = (SoundProps){0};

    hp_release_handle(&scene->pool, entity.id);
}

void entity_set_position(Scene* scene, Entity e, HMM_Vec3 pos) {
    if (!entity_valid(scene, e)) return;
    int idx = hp_index(e.id);
    scene->transforms[idx].pos = pos;
}

HMM_Vec3 entity_get_position(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return (HMM_Vec3){0};
    int idx = hp_index(e.id);
    return scene->transforms[idx].pos;
}

void entity_set_rotation(Scene* scene, Entity e, HMM_Quat rot) {
    if (!entity_valid(scene, e)) {
        return;
    }
    int idx = hp_index(e.id);
    scene->transforms[idx].rot = rot;
}

HMM_Quat entity_get_rotation(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return (HMM_Quat){0};
    int idx = hp_index(e.id);
    return scene->transforms[idx].rot;
}

void entity_set_scale(Scene* scene, Entity e, HMM_Vec3 scale) {
    if (!entity_valid(scene, e)) return;
    int idx = hp_index(e.id);
    scene->transforms[idx].scale = scale;
}

HMM_Vec3 entity_get_scale(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return (HMM_Vec3){0};
    int idx = hp_index(e.id);
    return scene->transforms[idx].scale;
}

void entity_set_transform(Scene* scene, Entity e, Transform trs) {
    if (!entity_valid(scene, e)) return;
    int idx = hp_index(e.id);
    scene->transforms[idx] = trs;
}

Transform entity_get_transform(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return (Transform){0};
    int idx = hp_index(e.id);
    return scene->transforms[idx];
}

HMM_Mat4 entity_mtx(Scene* scene, Entity entity) {
    if (!entity_valid(scene, entity)) {
        return HMM_M4D(1.0f);
    }

    int idx = hp_index(entity.id);
    Transform* t = &scene->transforms[idx];

    HMM_Mat4 pos = HMM_Translate(t->pos);
    HMM_Mat4 rot = HMM_QToM4(t->rot);
    HMM_Mat4 scl = HMM_Scale(t->scale);

    HMM_Mat4 ret = HMM_Mul(pos, HMM_Mul(rot, scl));

    if (scene->relation_flags[idx] & ENTITY_HAS_PARENT) {
        Entity parent = scene->parents[idx];
        HMM_Mat4 parent_mtx = entity_mtx(scene, parent);
        return HMM_Mul(parent_mtx, ret);
    }

    return ret;
}

void entity_set_parent(Scene* scene, Entity entity, Entity parent) {
    if (!entity_valid(scene, entity)) return;
    if (!entity_valid(scene, parent)) return;

    int idx = hp_index(entity.id);
    scene->parents[idx] = parent;
    scene->relation_flags[idx] |= ENTITY_HAS_PARENT;

    // Add as child to parent
    entity_add_child(scene, parent, entity);
}

void entity_remove_parent(Scene* scene, Entity entity) {
    if (!entity_valid(scene, entity)) return;

    int idx = hp_index(entity.id);
    if (!(scene->relation_flags[idx] & ENTITY_HAS_PARENT)) return;

    Entity parent = scene->parents[idx];
    scene->parents[idx].id = 0;
    scene->relation_flags[idx] &= ~ENTITY_HAS_PARENT;

    if (entity_valid(scene, parent)) {
        int pidx = hp_index(parent.id);
        Children* children = &scene->childs[pidx];
        for (int i = 0; i < ENTITY_MAX_CHILDREN; i++) {
            if (children->data[i].id == entity.id) {
                children->data[i].id = 0;
                break;
            }
        }
    }
}

void entity_add_child(Scene* scene, Entity entity, Entity child) {
    if (!entity_valid(scene, entity)) return;
    if (!entity_valid(scene, child)) return;

    int idx = hp_index(entity.id);
    Children* children = &scene->childs[idx];

    // Find empty slot
    for (int i = 0; i < ENTITY_MAX_CHILDREN; i++) {
        if (children->data[i].id == 0) {
            children->data[i] = child;
            scene->relation_flags[idx] |= ENTITY_HAS_CHILDREN;
            return;
        }
    }
}

void entity_set_children(Scene* scene, Entity e, Entity* children) {
    if (!entity_valid(scene, e)) return;

    int idx = hp_index(e.id);
    entity_clear_children(scene, e);

    int count = 0;
    for (int i = 0; i < ENTITY_MAX_CHILDREN && children[i].id != 0; i++) {
        if (entity_valid(scene, children[i])) {
            scene->childs[idx].data[count++] = children[i];
        }
    }

    if (count > 0) {
        scene->relation_flags[idx] |= ENTITY_HAS_CHILDREN;
    }
}

void entity_clear_children(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return;

    int idx = hp_index(e.id);
    memset(&scene->childs[idx], 0, sizeof(Children));
    scene->relation_flags[idx] &= ~ENTITY_HAS_CHILDREN;
}

void entity_set_model(Scene* scene, Entity entity, ModelHandle model) {
    if (!entity_valid(scene, entity)) return;

    int idx = hp_index(entity.id);
    scene->models[idx] = model;
    scene->model_flags[idx] |= ENTITY_VISIBLE | ENTITY_HAS_MODEL;
}


void entity_clear_model(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return;
    int idx = hp_index(e.id);
    scene->models[idx].id = HP_INVALID_HANDLE;
    scene->model_flags[idx] &= ~ENTITY_VISIBLE | ~ENTITY_HAS_MODEL;
}

void entity_set_textures(Scene* scene, Entity entity, TextureSet views) {
    if (!entity_valid(scene, entity)) return;
    int idx = hp_index(entity.id);
    scene->textures[idx] = views;
}

void entity_clear_textures(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return;
    int idx = hp_index(e.id);
    memset(&scene->textures[idx], HP_INVALID_HANDLE, sizeof(TextureSet));
}

void entity_set_anim(Scene* scene, Entity entity, AnimSetHandle set, AnimState state) {
    if (!entity_valid(scene, entity)) return;

    int idx = hp_index(entity.id);

    //only blend if entity already had an animation (avoid blending with uninitialized state)
    if (scene->anim_flags[idx] & ENTITY_HAS_ANIM) {
        scene->prev_anim_states[idx] = scene->anim_states[idx];
        scene->anim_blend_weights[idx] = 0.0f;
    }

    scene->anims[idx] = set;
    scene->anim_states[idx] = state;
    scene->anim_flags[idx] |= ENTITY_HAS_ANIM;
}

void entity_clear_anim(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return;
    int idx = hp_index(e.id);
    scene->anim_flags[idx] = 0;
    scene->anims[idx].id = HP_INVALID_HANDLE;
    memset(&scene->anim_states[idx], 0, sizeof(AnimState));
    memset(&scene->prev_anim_states[idx], 0, sizeof(AnimState));
    scene->anim_blend_weights[idx] = 0.0f;
}

void entity_set_sound(Scene* scene, Entity e, SoundBufferHandle buffer, SoundProps props, uint32_t flags) {
    if (!entity_valid(scene, e)) return;

    int idx = hp_index(e.id);
    scene->sound_buffers[idx] = buffer;
    scene->sound_props[idx] = props;
    scene->sound_flags[idx] = ENTITY_HAS_SOUND | (flags);
    scene->sound_channels[idx] = (tm_channel){0};
}

void entity_play_sound(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return;

    int idx = hp_index(e.id);
    if (scene->sound_flags[idx] & ENTITY_HAS_SOUND) {
        scene->sound_flags[idx] |= ENTITY_SOUND_PLAY;
    }
}

void entity_stop_sound(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return;

    int idx = hp_index(e.id);
    scene->sound_flags[idx] &= ~ENTITY_SOUND_PLAY;
}

void entity_clear_sound(Scene* scene, Entity e) {
    if (!entity_valid(scene, e)) return;

    int idx = hp_index(e.id);
    scene->sound_flags[idx] = 0;
    scene->sound_buffers[idx].id = HP_INVALID_HANDLE;
    scene->sound_channels[idx] = (tm_channel){0};
    scene->sound_props[idx] = sound_props_default();
}
