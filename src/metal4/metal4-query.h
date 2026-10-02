#pragma once

#include "metal4-base.h"

namespace rhi::metal4 {

class QueryPoolImpl : public QueryPool
{
public:
    api::Ptr<api::CounterHeap> m_timestampHeap;
    NS::SharedPtr<MTL::Buffer> m_timestampReadback;
    NS::SharedPtr<MTL::Buffer> m_visibilityBuffer;

    QueryPoolImpl(Device* device, const QueryPoolDesc& desc);
    ~QueryPoolImpl();

    Result init();
    virtual SLANG_NO_THROW Result SLANG_MCALL reset() override;
    virtual SLANG_NO_THROW Result SLANG_MCALL reset(uint32_t, uint32_t) override;

    virtual SLANG_NO_THROW Result SLANG_MCALL getResultState(
        uint32_t queryIndex,
        uint32_t count,
        QueryResultState* outState
    ) override;
    virtual SLANG_NO_THROW Result SLANG_MCALL getResult(
        uint32_t queryIndex,
        uint32_t count,
        uint64_t* outData
    ) override;
};

} // namespace rhi::metal4
