struct Parameters
{
    uint index;
    float bias;
};
ConstantBuffer<Parameters> parameters;
Texture2D<float4> inputTexture;
SamplerState inputSampler;
RWStructuredBuffer<float4> output;
float4 sampledValue()
{
    return inputTexture.SampleLevel(inputSampler, float2(1.25, 0.5), 0) + float4(parameters.bias, 0, 0, 0);
}
[shader("compute")][numthreads(1, 1, 1)] void computeMain(uint3 tid : SV_DispatchThreadID)
{
    output[parameters.index] = sampledValue();
}[shader("vertex")] float4 vertexMain(uint id : SV_VertexID)
    : SV_Position
{
    float2 p[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
    return float4(p[id], parameters.bias * 0.001, 1);
}
[shader("fragment")] float4 fragmentMain()
    : SV_Target
{
    return sampledValue();
}
