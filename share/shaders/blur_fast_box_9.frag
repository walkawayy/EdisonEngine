#include "flat_pipeline_interface.glsl"

#if BLUR_DIM == 1
#define BLUR_TYPE float
#elif BLUR_DIM == 2
#define BLUR_TYPE vec2
#elif BLUR_DIM == 3
#define BLUR_TYPE vec3
#endif

layout(bindless_sampler) uniform sampler2D u_input;
layout(location=0) uniform int u_blurDir;
layout(location=0) out BLUR_TYPE out_tex;

void main()
{
    const vec2 texelSize = 1.0 / vec2(textureSize(u_input, 0));

    vec2 direction = u_blurDir == 1 ? vec2(1, 0) * texelSize : vec2(0, 1) * texelSize;
    vec2 off1 = vec2(1.5) * direction;
    vec2 off2 = vec2(3.5) * direction;
    out_tex = (
    BLUR_TYPE(texture(u_input, fpi.texCoord))
    + BLUR_TYPE(texture(u_input, fpi.texCoord + off1))
    + BLUR_TYPE(texture(u_input, fpi.texCoord - off1))
    + BLUR_TYPE(texture(u_input, fpi.texCoord + off2))
    + BLUR_TYPE(texture(u_input, fpi.texCoord - off2))
    ) * (1.0 / 5.0);
}
