#include "metal4-query.h"
#include "metal4-device.h"
#include "metal4-command.h"
#include "metal4-utils.h"

namespace rhi::metal4 {

QueryPoolImpl::QueryPoolImpl(Device* device, const QueryPoolDesc& desc)
    : QueryPool(device, desc)
{
}

QueryPoolImpl::~QueryPoolImpl() {}

Result QueryPoolImpl::init()
{
    DeviceImpl* device = getDevice<DeviceImpl>();

    if (isOcclusionQueryType(m_desc.type))
    {
        if (m_desc.count == 0 || m_desc.count > 8192)
            return SLANG_E_INVALID_ARG; // Conservative 64 KiB limit supported by every Metal GPU family.
        if (!device->hasFeature(
                m_desc.type == QueryType::OcclusionPrecise ? Feature::PreciseOcclusionQuery : Feature::OcclusionQuery
            ))
            return SLANG_E_NOT_AVAILABLE;
        m_visibilityBuffer = NS::TransferPtr(
            device->m_device->newBuffer(uint64_t(m_desc.count) * sizeof(uint64_t), MTL::ResourceStorageModeShared)
        );
        if (m_visibilityBuffer && m_desc.label)
            m_visibilityBuffer->setLabel(createString(m_desc.label).get());
        return m_visibilityBuffer ? SLANG_OK : SLANG_FAIL;
    }

    if (m_desc.type != QueryType::Timestamp)
        return SLANG_E_NOT_AVAILABLE;
    if (m_desc.count == 0 || m_desc.count > 8192)
        return SLANG_E_INVALID_ARG;
    m_timestampHeap = api::adopt(new api::CounterHeap(device->m_device.get(), m_desc.count));
    if (!m_timestampHeap->native)
        return SLANG_E_NOT_AVAILABLE;
    m_timestampReadback = NS::TransferPtr(
        device->m_device->newBuffer(uint64_t(m_desc.count) * sizeof(uint64_t), MTL::ResourceStorageModeShared)
    );
    return m_timestampReadback ? SLANG_OK : SLANG_FAIL;
}

Result QueryPoolImpl::reset()
{
    return reset(0, m_desc.count);
}
Result QueryPoolImpl::reset(uint32_t index, uint32_t count)
{
    if (!isValidQueryRange(index, count))
        return SLANG_E_INVALID_ARG;
    if (m_timestampHeap && count)
    {
        const auto info = getQueryRangeInfo(index, count);
        if (info.state == QueryResultState::Pending &&
            getDevice<DeviceImpl>()->m_queue->updateLastFinishedID() < info.submissionID)
            return SLANG_FAIL; // CPU invalidation cannot race a GPU writer.
        m_timestampHeap->invalidate(index, count);
    }
    return QueryPool::reset(index, count);
}
Result QueryPoolImpl::getResultState(uint32_t queryIndex, uint32_t count, QueryResultState* outState)
{
    if (!isOcclusionQueryType(m_desc.type) && m_desc.type != QueryType::Timestamp)
        return SLANG_E_NOT_AVAILABLE;
    if (!outState || !isValidQueryRange(queryIndex, count))
        return SLANG_E_INVALID_ARG;
    auto info = getQueryRangeInfo(queryIndex, count);
    *outState = info.state;
    if (info.state == QueryResultState::Pending &&
        getDevice<DeviceImpl>()->m_queue->updateLastFinishedID() >= info.submissionID)
    {
        markQueryRangeResolved(queryIndex, count, info.submissionID);
        *outState = QueryResultState::Resolved;
    }
    return SLANG_OK;
}

Result QueryPoolImpl::getResult(uint32_t queryIndex, uint32_t count, uint64_t* outData)
{
    if (!isOcclusionQueryType(m_desc.type) && m_desc.type != QueryType::Timestamp)
        return SLANG_E_NOT_AVAILABLE;
    if (!outData || !isValidQueryRange(queryIndex, count))
        return SLANG_E_INVALID_ARG;
    auto info = getQueryRangeInfo(queryIndex, count);
    if (info.state == QueryResultState::Reset)
        return SLANG_FAIL;
    if (count == 0)
        return SLANG_OK;
    auto queue = getDevice<DeviceImpl>()->m_queue;
    if (queue->updateLastFinishedID() < info.submissionID)
        SLANG_RETURN_ON_FAIL(queue->waitOnHost());
    if (m_desc.type == QueryType::Timestamp)
    {
        std::memcpy(
            outData,
            static_cast<uint64_t*>(m_timestampReadback->contents()) + queryIndex,
            count * sizeof(uint64_t)
        );
    }
    else
        std::memcpy(
            outData,
            static_cast<uint64_t*>(m_visibilityBuffer->contents()) + queryIndex,
            sizeof(uint64_t) * count
        );
    markQueryRangeResolved(queryIndex, count, info.submissionID);
    return SLANG_OK;
}

} // namespace rhi::metal4
