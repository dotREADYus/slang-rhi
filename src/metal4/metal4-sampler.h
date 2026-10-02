#pragma once

#include "metal4-base.h"

namespace rhi::metal4 {

class SamplerImpl : public Sampler
{
public:
    SamplerImpl(Device* device, const SamplerDesc& desc);
    ~SamplerImpl();

    virtual void deleteThis() override;

    Result init();

    // IResource implementation
    virtual SLANG_NO_THROW Result SLANG_MCALL getNativeHandle(NativeHandle* outHandle) override;

public:
    NS::SharedPtr<MTL::SamplerState> m_samplerState;
};

} // namespace rhi::metal4
