Texture2D<float4> inputTexture;
RWStructuredBuffer<float4> output;
[numthreads(1, 1, 1)] void computeMain(uint3 id : SV_DispatchThreadID)
{
    output[0] = inputTexture.Load(int3(0, 0, 1));
}
