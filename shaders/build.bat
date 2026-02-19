#/bin/bash 2>nul || goto :windows

# bash
echo hello Bash
ls

LANG="hlsl5:glsl430:wgsl"

sokol-shdc -i shaders.glsl -o shaders.glsl.h --slang $LANG

exit

:windows

set lang=hlsl5:glsl430:glsl300es:wgsl:spirv_vk

cmd /c "sokol-shdc -i shaders.glsl -o shaders.glsl.h --slang %lang%"
cmd /c "sokol-shdc -i display.glsl -o display.glsl.h --slang %lang%"

exit /b
