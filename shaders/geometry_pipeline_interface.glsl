IN_OUT GeometryPipelineInterface {
    vec2 texCoord;
    vec4 color;
    flat float texIndex;
    vec3 vertexPos;
    vec3 vertexPosLight[5];
    vec3 vertexPosWorld;
    vec3 normal;
    vec3 ssaoNormal;

    flat float isQuad;
    flat vec3 quadVerts[4];
    flat vec2 quadUvs[4];
} gpi;
