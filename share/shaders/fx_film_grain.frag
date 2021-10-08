#include "flat_pipeline_interface.glsl"
#include "noise.glsl"
#include "time_uniform.glsl"

layout(bindless_sampler) uniform sampler2D u_input;
layout(location=0) out vec3 out_color;

void main()
{
    float grain = noise(fpi.texCoord * time_seconds())*0.5 + 1.0;
    out_color = texture(u_input, fpi.texCoord).rgb * grain;
}