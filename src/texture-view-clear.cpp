#include "command-buffer.h"
#include "device.h"
#include "pipeline.h"
#include "rhi-shared.h"
#include <slang-rhi/shader-cursor.h>
#include <cmath>

namespace rhi {

Result Device::getTextureViewClearPipeline(
    Format format,
    uint32_t samples,
    RenderTargetWriteMask mask,
    bool clearDepth,
    bool clearStencil,
    IRenderPipeline** outPipeline
)
{
    std::lock_guard<std::mutex> lock(m_textureViewClearMutex);
    std::vector<uint32_t> key =
        {uint32_t(format), samples, uint32_t(mask), uint32_t(clearDepth), uint32_t(clearStencil)};
    auto found = m_textureViewClearPipelines.find(key);
    if (found != m_textureViewClearPipelines.end())
    {
        *outPipeline = found->second.get();
        (*outPipeline)->addRef();
        return SLANG_OK;
    }
    const auto& info = getFormatInfo(format);
    bool depthStencil = info.kind == FormatKind::DepthStencil;
    const char* scalar = info.kind == FormatKind::Integer ? (info.isSigned ? "int" : "uint") : "float";
    std::string name = depthStencil ? "rhi_attachment_clear_depth" : std::string("rhi_attachment_clear_") + scalar;
    std::string source = std::string("uniform ") + scalar +
                         "4 clearColor; uniform float clearDepth;\n"
                         "[shader(\"vertex\")] float4 vertexMain(uint id : SV_VertexID) : SV_Position {\n"
                         "float2 p = float2((id << 1) & 2, id & 2); return float4(p * 2 - 1, clearDepth, 1); }\n";
    source += depthStencil ? "[shader(\"fragment\")] void fragmentMain() {}\n"
                           : std::string("[shader(\"fragment\")] ") + scalar +
                                 "4 fragmentMain() : SV_Target0 { return clearColor; }\n";
    ComPtr<slang::ISession> session;
    SLANG_RETURN_ON_FAIL(getSlangSession(session.writeRef()));
    ComPtr<slang::IBlob> diagnostics;
    auto module =
        session->loadModuleFromSourceString(name.c_str(), name.c_str(), source.c_str(), diagnostics.writeRef());
    if (!module)
        return SLANG_FAIL;
    ComPtr<slang::IEntryPoint> vertex, fragment;
    SLANG_RETURN_ON_FAIL(module->findEntryPointByName("vertexMain", vertex.writeRef()));
    SLANG_RETURN_ON_FAIL(module->findEntryPointByName("fragmentMain", fragment.writeRef()));
    slang::IComponentType* components[] = {module, vertex.get(), fragment.get()};
    ComPtr<slang::IComponentType> composite, linked;
    SLANG_RETURN_ON_FAIL(
        session->createCompositeComponentType(components, 3, composite.writeRef(), diagnostics.writeRef())
    );
    SLANG_RETURN_ON_FAIL(composite->link(linked.writeRef(), diagnostics.writeRef()));
    ShaderProgramDesc programDesc = {};
    programDesc.slangGlobalScope = linked;
    ComPtr<IShaderProgram> program;
    SLANG_RETURN_ON_FAIL(createShaderProgram(programDesc, program.writeRef(), diagnostics.writeRef()));
    ColorTargetDesc target = {};
    target.format = format;
    target.writeMask = mask;
    RenderPipelineDesc pipelineDesc = {};
    pipelineDesc.program = program;
    pipelineDesc.compilationPolicy = PipelineCompilationPolicy::Immediate;
    pipelineDesc.rasterizer.scissorEnable = true;
    pipelineDesc.rasterizer.multisampleEnable = samples > 1;
    pipelineDesc.multisample.sampleCount = samples;
    if (depthStencil)
    {
        pipelineDesc.depthStencil.format = format;
        pipelineDesc.depthStencil.depthTestEnable = clearDepth;
        pipelineDesc.depthStencil.depthWriteEnable = clearDepth;
        pipelineDesc.depthStencil.depthFunc = ComparisonFunc::Always;
        pipelineDesc.depthStencil.stencilEnable = clearStencil;
        pipelineDesc.depthStencil.stencilWriteMask = 0xff;
        pipelineDesc.depthStencil.frontFace.stencilPassOp = StencilOp::Replace;
        pipelineDesc.depthStencil.backFace.stencilPassOp = StencilOp::Replace;
    }
    else
    {
        pipelineDesc.targets = &target;
        pipelineDesc.targetCount = 1;
        pipelineDesc.depthStencil.depthWriteEnable = false;
    }
    ComPtr<IRenderPipeline> pipeline;
    SLANG_RETURN_ON_FAIL(createRenderPipeline(pipelineDesc, pipeline.writeRef()));
    m_textureViewClearPipelines.emplace(std::move(key), checked_cast<RenderPipeline*>(pipeline.get()));
    *outPipeline = pipeline.detach();
    return SLANG_OK;
}

Result CommandEncoder::clearTextureView(const TextureViewClearDesc& desc)
{
    if (!getDevice()->hasFeature(Feature::TextureViewClear))
        return SLANG_E_NOT_AVAILABLE;
    if (!m_commandList || m_renderPassEncoder.m_commandList || m_computePassEncoder.m_commandList ||
        m_rayTracingPassEncoder.m_commandList)
        return SLANG_FAIL;
    if (!desc.view || (desc.rectangleCount && !desc.rectangles))
        return SLANG_E_INVALID_ARG;
    auto view = checked_cast<TextureView*>(desc.view);
    auto texture = checked_cast<Texture*>(view->getTexture());
    if (texture->getDevice() != getDevice())
        return SLANG_E_INVALID_ARG;
    const auto& td = texture->getDesc();
    const auto& vd = view->getDesc();
    if ((getTextureDimension(td.type) != TextureDimension::Texture2D) || vd.subresourceRange.mip >= td.mipCount ||
        vd.subresourceRange.layer >= td.getLayerCount())
        return SLANG_E_INVALID_ARG;
    auto range = texture->resolveSubresourceRange(vd.subresourceRange);
    if (range.mipCount != 1 || range.layerCount != 1)
        return SLANG_E_INVALID_ARG;
    Format format = vd.format == Format::Undefined ? td.format : vd.format;
    const auto& info = getFormatInfo(format);
    bool depthStencil = info.kind == FormatKind::DepthStencil;
    if (info.isCompressed || (uint32_t(desc.colorWriteMask) & ~15u))
        return SLANG_E_INVALID_ARG;
    if (depthStencil)
    {
        if (!is_set(td.usage, TextureUsage::DepthStencil) ||
            (desc.clearDepth &&
             (vd.aspect == TextureAspect::StencilOnly || !info.hasDepth || !std::isfinite(desc.depthStencil.depth) ||
              desc.depthStencil.depth < 0.f || desc.depthStencil.depth > 1.f)) ||
            (desc.clearStencil &&
             (vd.aspect == TextureAspect::DepthOnly || !info.hasStencil || desc.depthStencil.stencil > 255)))
            return SLANG_E_INVALID_ARG;
    }
    else if (!is_set(td.usage, TextureUsage::RenderTarget) || desc.clearDepth || desc.clearStencil)
        return SLANG_E_INVALID_ARG;
    uint32_t width = max(1u, uint32_t(td.size.width) >> range.mip);
    uint32_t height = max(1u, uint32_t(td.size.height) >> range.mip);
    std::vector<ScissorRect> rectangles;
    if (!desc.rectangleCount)
        rectangles.push_back(ScissorRect::fromSize(width, height));
    for (uint32_t i = 0; i < desc.rectangleCount; ++i)
    {
        auto r = desc.rectangles[i];
        if (r.minX > r.maxX || r.minY > r.maxY)
            return SLANG_E_INVALID_ARG;
        r.minX = min(r.minX, width);
        r.maxX = min(r.maxX, width);
        r.minY = min(r.minY, height);
        r.maxY = min(r.maxY, height);
        if (r.minX < r.maxX && r.minY < r.maxY)
            rectangles.push_back(r);
    }
    if (rectangles.empty() ||
        (depthStencil ? !desc.clearDepth && !desc.clearStencil : desc.colorWriteMask == RenderTargetWriteMask::None))
        return SLANG_OK;
    // Whole-view normalized/float and depth/stencil clears use native pass load
    // clears. Draw fallback is needed only for rectangles, channel masks, or
    // integer values that cannot be represented exactly by float load values.
    bool wholeView = rectangles.size() == 1 && rectangles[0].minX == 0 && rectangles[0].minY == 0 &&
                     rectangles[0].maxX == width && rectangles[0].maxY == height;
    if (wholeView &&
        (depthStencil || (desc.colorWriteMask == RenderTargetWriteMask::All && info.kind != FormatKind::Integer)))
    {
        RenderPassColorAttachment color = {};
        color.view = desc.view;
        std::memcpy(color.clearValue, desc.color.floatValues, sizeof(color.clearValue));
        RenderPassDepthStencilAttachment ds = {};
        ds.view = desc.view;
        ds.depthLoadOp = desc.clearDepth ? LoadOp::Clear : LoadOp::Load;
        ds.stencilLoadOp = desc.clearStencil ? LoadOp::Clear : LoadOp::Load;
        ds.depthClearValue = desc.depthStencil.depth;
        ds.stencilClearValue = uint8_t(desc.depthStencil.stencil);
        commands::BeginRenderPass begin = {};
        if (depthStencil)
            begin.desc.depthStencilAttachment = &ds;
        else
        {
            begin.desc.colorAttachments = &color;
            begin.desc.colorAttachmentCount = 1;
        }
        m_commandList->write(std::move(begin));
        m_commandList->write(commands::EndRenderPass{});
        return SLANG_OK;
    }
    ComPtr<IRenderPipeline> pipeline;
    SLANG_RETURN_ON_FAIL(
        getDevice()->getTextureViewClearPipeline(
            format,
            td.sampleCount,
            depthStencil ? RenderTargetWriteMask::None : desc.colorWriteMask,
            desc.clearDepth,
            desc.clearStencil,
            pipeline.writeRef()
        )
    );
    // Build binding data before opening a pass; errors must leave encoder scope intact.
    RefPtr<RootShaderObject> root;
    SLANG_RETURN_ON_FAIL(
        getDevice()->createRootShaderObject(checked_cast<ShaderProgram*>(pipeline->getProgram()), root.writeRef())
    );
    if (!depthStencil)
        SLANG_RETURN_ON_FAIL(ShaderCursor(root.get())["clearColor"].setData(&desc.color, sizeof(desc.color)));
    float depth = desc.clearDepth ? desc.depthStencil.depth : 0.f;
    SLANG_RETURN_ON_FAIL(ShaderCursor(root.get())["clearDepth"].setData(&depth, sizeof(depth)));
    BindingData* bindings = nullptr;
    SLANG_RETURN_ON_FAIL(getBindingData(root, bindings));
    RenderPassColorAttachment color = {};
    color.view = desc.view;
    color.loadOp = LoadOp::Load;
    RenderPassDepthStencilAttachment ds = {};
    ds.view = desc.view;
    ds.depthLoadOp = ds.stencilLoadOp = LoadOp::Load;
    RenderPassDesc passDesc = {};
    if (depthStencil)
        passDesc.depthStencilAttachment = &ds;
    else
    {
        passDesc.colorAttachments = &color;
        passDesc.colorAttachmentCount = 1;
    }
    // Record directly rather than mutating the application's pass encoder state.
    commands::BeginRenderPass begin;
    begin.desc = passDesc;
    m_commandList->write(std::move(begin));
    for (auto r : rectangles)
    {
        commands::SetRenderState state = {};
        state.pipeline = pipeline;
        getPipelineSpecializationArgs(pipeline, root, state.specializationArgs);
        state.bindingData = bindings;
        state.state.viewportCount = state.state.scissorRectCount = 1;
        state.state.viewports[0] = Viewport::fromSize(float(width), float(height));
        state.state.scissorRects[0] = r;
        state.state.stencilRef = desc.depthStencil.stencil;
        m_commandList->write(std::move(state));
        commands::Draw draw = {};
        draw.args.vertexCount = 3;
        m_commandList->write(std::move(draw));
    }
    m_commandList->write(commands::EndRenderPass{});
    return SLANG_OK;
}

} // namespace rhi
