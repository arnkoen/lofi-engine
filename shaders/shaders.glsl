@ctype vec3 HMM_Vec3
@ctype mat4 HMM_Mat4
@ctype vec4 sg_color

@include common.inc.glsl

//--CUBEMAP----------------------------------------------------

@vs cubemap_vs
@include_block vs_uniforms

in vec4 pos;
out vec3 uvw;

void main() {
    gl_Position = proj * view * model * pos;
    uvw = normalize(pos.xyz);
}
@end

@fs cubemap_fs
@include_block dither
layout(binding=0) uniform textureCube tex;
layout(binding=0) uniform sampler smp;

in vec3 uvw;
out vec4 frag_color;

void main() {
    vec4 col = texture(samplerCube(tex, smp), uvw);
    vec3 res = dither(col.xyz, gl_FragCoord.xy, 16.0);
    frag_color = vec4(res, col.a);
}
@end

@program cubemap cubemap_vs cubemap_fs

//--LIT--------------------------------------------------------

//TEXTURED

@vs tex_lit_vs
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;

out vec3 v_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_viewpos;

@include_block vs_uniforms

void main() {
    gl_Position = proj * view * model * vec4(position, 1.0);
    mat4 viewproj = proj * view;
    v_pos = vec3(model * vec4(position, 1.0));
    v_viewpos = vec4(viewproj * vec4(v_pos, 1.0)).xyz;
    v_normal = mat3(model) * normal;
    v_uv = uv;
}
@end

@vs tex_lit_skinned_vs
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location = 3) in uvec4 bone_indices;
layout(location = 4) in vec4 weights;

out vec3 v_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_viewpos;

@include_block vs_uniforms

#define MAX_BONES 32
layout(binding=1) uniform u_skeleton {
    mat4 bones[MAX_BONES];
};

//SOKOL_GLES3 does not seem to do the trick(?)
#ifdef SOKOL_GLSL
mat4 get_bone(int index) {
    for (int i = 0; i < MAX_BONES; i++) {
        if (i == index) return bones[i];
    }
    return mat4(1.0);
}
#endif

void main() {
#ifdef SOKOL_GLSL
    mat4 skin_mat = weights.x * get_bone(int(bone_indices.x)) +
                    weights.y * get_bone(int(bone_indices.y)) +
                    weights.z * get_bone(int(bone_indices.z)) +
                    weights.w * get_bone(int(bone_indices.w));
#else
    uvec4 idx = bone_indices;
    mat4 skin_mat = weights.x * bones[idx.x] +
                    weights.y * bones[idx.y] +
                    weights.z * bones[idx.z] +
                    weights.w * bones[idx.w];
#endif

    vec4 skinned_pos = skin_mat * vec4(position, 1.0);
    vec3 skinned_nrm = mat3(skin_mat) * normal;

    gl_Position = proj * view * model * skinned_pos;

    mat4 viewproj = proj * view;
    v_pos = vec3(model * skinned_pos);
    v_normal = normalize(mat3(model) * skinned_nrm);
    v_uv = uv;
    v_viewpos = vec4(viewproj * vec4(v_pos, 1.0)).xyz;
}
@end

@fs tex_lit_fs
in vec3 v_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec3 v_viewpos;

out vec4 FragColor;

layout(binding=0) uniform sampler col_smp;
layout(binding=0) uniform texture2D col_tex;

@include_block dirlight
@include_block lambert
@include_block dither

void main() {
    vec4 tex = texture(sampler2D(col_tex, col_smp), v_uv);
    vec3 lit = lambert_light(v_pos, v_normal, v_viewpos, tex.rgb, tex.rgb);
    vec3 res = dither(lit, gl_FragCoord.xy, 16.0);
    FragColor = vec4(res, 1.0);
}
@end

@program tex_lit tex_lit_vs tex_lit_fs
@program tex_lit_skinned tex_lit_skinned_vs tex_lit_fs
