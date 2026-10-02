#pragma once

#include "metal4-base.h"

#include <string>

namespace rhi::metal4 {

class ShaderProgramImpl : public ShaderProgram
{
public:
    RefPtr<RootShaderObjectLayoutImpl> m_rootObjectLayout;

    struct Module
    {
        SlangStage stage;
        std::string entryPointName;
        ComPtr<ISlangBlob> code;
        NS::SharedPtr<MTL::Library> library;
    };

    std::vector<Module> m_modules;

    ShaderProgramImpl(Device* device, const ShaderProgramDesc& desc);
    ~ShaderProgramImpl();

    virtual Result createShaderModule(const ShaderModuleDesc& desc, ComPtr<ISlangBlob> kernelCode) override;

    virtual ShaderObjectLayout* getRootShaderObjectLayout() override;
};

} // namespace rhi::metal4
