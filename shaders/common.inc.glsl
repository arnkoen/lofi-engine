
@block vs_uniforms
layout(binding = 0) uniform u_vs_params {
    mat4 view;
    mat4 proj;
    mat4 model;
};
@end

@block dirlight
layout(binding = 3) uniform u_dir_light {
    vec3 direction;
    vec4 ambient;
    vec4 diffuse;
} light;
@end

@block lambert
vec3 lambert_light(vec3 v_pos, vec3 v_normal, vec3 viewpos, vec3 material_ambient, vec3 material_diffuse) {
    vec3 norm = normalize(v_normal);
    vec3 view_dir = normalize(viewpos - v_pos);
    vec3 light_dir = normalize(-light.direction);

    float diff = max(dot(norm, light_dir), 0.0);

    vec3 ambient = light.ambient.rgb * material_ambient;
    vec3 diffuse = light.diffuse.rgb * diff * material_diffuse;

    return ambient + diffuse;
}
@end

@block dither
float bayer4x4(vec2 pos) {
    int x = int(mod(pos.x, 4.0));
    int y = int(mod(pos.y, 4.0));
    const float matrix[16] = float[16](
        0.0,  8.0,  2.0, 10.0,
        12.0, 4.0, 14.0,  6.0,
        3.0, 11.0,  1.0,  9.0,
        15.0, 7.0, 13.0,  5.0
    );
    return matrix[y * 4 + x] / 16.0;
}

vec3 dither(vec3 color, vec2 screenPos, float levels) {
    float threshold = bayer4x4(screenPos);
    return floor(color * levels + threshold) / levels;
}
@end
