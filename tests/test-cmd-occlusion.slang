float queryDepth;

[shader("vertex")]
float4 vertexMain(uint id : SV_VertexID) : SV_Position
{
    float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    return float4(positions[id], queryDepth, 1);
}

[shader("fragment")]
float4 fragmentMain() : SV_Target
{
    return float4(1, 1, 1, 1);
}
