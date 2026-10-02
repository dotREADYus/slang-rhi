#include "metal-query.h"
#include "metal-device.h"
#include "metal-command.h"
#include "metal-utils.h"

namespace rhi::metal {

QueryPoolImpl::QueryPoolImpl(Device* device, const QueryPoolDesc& desc)
    : QueryPool(device, desc)
{
}

QueryPoolImpl::~QueryPoolImpl() {}

static MTL::CounterSet* findCounterSet(MTL::Device* device, QueryType queryType)
{
    if (queryType != QueryType::Timestamp)
    {
        return nullptr;
    }

    for (int i = 0; i < device->counterSets()->count(); ++i)
    {
        MTL::CounterSet* counterSet = static_cast<MTL::CounterSet*>(device->counterSets()->object(i));
        if (!counterSet->name()->isEqualToString(MTL::CommonCounterSetTimestamp))
        {
            continue;
        }
        for (int j = 0; j < counterSet->counters()->count(); ++j)
        {
            MTL::Counter* counter = static_cast<MTL::Counter*>(counterSet->counters()->object(j));
            if (counter->name()->isEqualToString(MTL::CommonCounterTimestamp))
            {
                return counterSet;
            }
        }
    }
    return nullptr;
}

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

    // Recording timestamps is not implemented by the classic backend. Do not
    // create a pool that would silently return unwritten counter data.
    if (m_desc.type == QueryType::Timestamp && !device->hasFeature(Feature::TimestampQuery))
        return SLANG_E_NOT_AVAILABLE;

    MTL::CounterSet* counterSet = findCounterSet(device->m_device.get(), m_desc.type);
    if (!counterSet)
    {
        return SLANG_E_NOT_AVAILABLE;
    }

    NS::SharedPtr<MTL::CounterSampleBufferDescriptor> counterSampleBufferDesc =
        NS::TransferPtr(MTL::CounterSampleBufferDescriptor::alloc()->init());
    counterSampleBufferDesc->setStorageMode(MTL::StorageModeShared);
    counterSampleBufferDesc->setSampleCount(m_desc.count);
    counterSampleBufferDesc->setCounterSet(counterSet);
    if (m_desc.label)
    {
        counterSampleBufferDesc->setLabel(createString(m_desc.label).get());
    }

    NS::Error* error;
    m_counterSampleBuffer =
        NS::TransferPtr(device->m_device->newCounterSampleBuffer(counterSampleBufferDesc.get(), &error));

    return m_counterSampleBuffer ? SLANG_OK : SLANG_FAIL;
}

Result QueryPoolImpl::getResultState(uint32_t queryIndex, uint32_t count, QueryResultState* outState)
{
    if (!isOcclusionQueryType(m_desc.type))
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
    if (!isOcclusionQueryType(m_desc.type))
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
    std::memcpy(outData, static_cast<uint64_t*>(m_visibilityBuffer->contents()) + queryIndex, sizeof(uint64_t) * count);
    markQueryRangeResolved(queryIndex, count, info.submissionID);
    return SLANG_OK;
}

} // namespace rhi::metal
