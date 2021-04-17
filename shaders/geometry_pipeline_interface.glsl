#include "csm_interface.glsl"

IN_OUT GeometryPipelineInterface {
    vec2 texCoord;
    vec4 color;
    flat float texIndex;
    vec3 vertexPosLight[CSMSplits];
    vec3 vertexPosWorld;
    vec3 normal;

    flat float isQuad;
    flat vec3 quadVerts[4];
    flat vec2 quadUvs[4];
} gpi;
