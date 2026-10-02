struct Parameters
{
    uint index;
    uint value;
    float padding[254];
};
ConstantBuffer<Parameters> parameters;
RWStructuredBuffer<uint> output;
[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    output[parameters.index] = parameters.value + uint(parameters.padding[253]);
}
