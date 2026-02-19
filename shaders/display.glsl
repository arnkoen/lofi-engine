// display-pass shader
@ctype vec2 HMM_Vec2


@module display

@vs vs

layout(binding=0) uniform vs_params {
    vec2 resolution;
    vec2 offscreen_size;
};


in vec2 position;
in vec2 uv;
out vec2 v_uv;


void main() {
    // Calculate the aspect ratios
    float display_aspect = resolution.x / resolution.y;
    float offscreen_aspect = offscreen_size.x / offscreen_size.y;

    // Initialize scale factors
    vec2 scale = vec2(1.0, 1.0);

    // Adjust scaling to maintain the offscreen aspect ratio
    if (display_aspect > offscreen_aspect) {
        // Wider display: scale width
        scale.x = offscreen_aspect / display_aspect;
    } else {
        // Taller display: scale height
        scale.y = display_aspect / offscreen_aspect;
    }

    // Apply scaling to the position (maintaining aspect ratio)
    vec2 scaledPosition = position * scale;

    // Pass the texture coordinates to the fragment shader
    v_uv = uv;

    // Output the final position in clip space
    gl_Position = vec4(scaledPosition, 0.0, 1.0);
}
@end

@fs fs
layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler smp;

in vec2 v_uv;
out vec4 frag_color;

void main() {
    vec2 uv = v_uv;
#if SOKOL_HLSL || SOKOL_WGSL
    uv.y = 1.0 - uv.y;
#endif
    vec4 col = texture(sampler2D(tex, smp), uv);
    frag_color = col;
}
@end

@program shd vs fs
