#include "d3d11-query.h"
#include "d3d11-device.h"

#include "d3d/d3d-utils.h"

#include <thread>
#include <chrono>

namespace rhi::d3d11 {

QueryPoolImpl::QueryPoolImpl(Device* device, const QueryPoolDesc& desc)
    : QueryPool(device, desc)
{
}

Result QueryPoolImpl::init()
{
    if (m_desc.count == 0)
        return SLANG_E_INVALID_ARG;
    m_queryDesc.MiscFlags = 0;
    switch (m_desc.type)
    {
    case QueryType::Occlusion:
        m_queryDesc.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
        break;
    case QueryType::OcclusionPrecise:
        m_queryDesc.Query = D3D11_QUERY_OCCLUSION;
        break;
    case QueryType::Timestamp:
        m_queryDesc.Query = D3D11_QUERY_TIMESTAMP;
        break;
    default:
        return SLANG_E_INVALID_ARG;
    }
    m_queries.resize(m_desc.count);
    return SLANG_OK;
}

ID3D11Query* QueryPoolImpl::getQuery(uint32_t index)
{
    DeviceImpl* device = getDevice<DeviceImpl>();
    if (!m_queries[index].timestampQuery)
        device->m_device->CreateQuery(&m_queryDesc, m_queries[index].timestampQuery.writeRef());
    return m_queries[index].timestampQuery.get();
}

void QueryPoolImpl::setDisjointQuery(uint32_t index, ID3D11Query* disjointQuery)
{
    SLANG_RHI_ASSERT(index < m_queries.size());
    m_queries[index].disjointQuery = disjointQuery;
}

Result QueryPoolImpl::getResultState(uint32_t queryIndex, uint32_t count, QueryResultState* outState)
{
    if (!outState || !isValidQueryRange(queryIndex, count))
    {
        return SLANG_E_INVALID_ARG;
    }

    QueryRangeInfo queryInfo = getQueryRangeInfo(queryIndex, count);
    if (queryInfo.state == QueryResultState::Reset)
    {
        *outState = QueryResultState::Reset;
        return SLANG_OK;
    }
    if (queryInfo.state == QueryResultState::Resolved)
    {
        *outState = QueryResultState::Resolved;
        return SLANG_OK;
    }

    DeviceImpl* device = getDevice<DeviceImpl>();
    for (uint32_t i = 0; i < count; i++)
    {
        const Query& query = m_queries[queryIndex + i];
        if (isOcclusionQueryType(m_desc.type))
        {
            if (!query.timestampQuery)
                return SLANG_FAIL;
            uint64_t value = 0;
            BOOL predicate = FALSE;
            void* data =
                m_desc.type == QueryType::Occlusion ? static_cast<void*>(&predicate) : static_cast<void*>(&value);
            UINT size = m_desc.type == QueryType::Occlusion ? sizeof(predicate) : sizeof(value);
            HRESULT hr =
                device->m_immediateContext->GetData(query.timestampQuery, data, size, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (hr == S_FALSE)
            {
                *outState = QueryResultState::Pending;
                return SLANG_OK;
            }
            SLANG_D3D_RETURN_ON_FAIL_REPORT(hr, device);
            continue;
        }
        if (!query.timestampQuery || !query.disjointQuery)
        {
            return SLANG_FAIL;
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData = {};
        HRESULT hr =
            device->m_immediateContext
                ->GetData(query.disjointQuery, &disjointData, sizeof(disjointData), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr == S_FALSE)
        {
            *outState = QueryResultState::Pending;
            return SLANG_OK;
        }
        SLANG_D3D_RETURN_ON_FAIL_REPORT(hr, device);
        if (disjointData.Disjoint || disjointData.Frequency == 0)
        {
            return SLANG_FAIL;
        }
        device->m_info.timestampFrequency = disjointData.Frequency;

        uint64_t value = 0;
        hr = device->m_immediateContext
                 ->GetData(query.timestampQuery, &value, sizeof(value), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr == S_FALSE)
        {
            *outState = QueryResultState::Pending;
            return SLANG_OK;
        }
        SLANG_D3D_RETURN_ON_FAIL_REPORT(hr, device);
    }

    markQueryRangeResolved(queryIndex, count, queryInfo.submissionID);
    *outState = QueryResultState::Resolved;

    return SLANG_OK;
}

Result QueryPoolImpl::getResult(uint32_t queryIndex, uint32_t count, uint64_t* outData)
{
    if (!outData || !isValidQueryRange(queryIndex, count))
    {
        return SLANG_E_INVALID_ARG;
    }

    QueryRangeInfo queryInfo = getQueryRangeInfo(queryIndex, count);
    if (queryInfo.state == QueryResultState::Reset)
    {
        return SLANG_FAIL;
    }
    if (count == 0)
    {
        return SLANG_OK;
    }

    DeviceImpl* device = getDevice<DeviceImpl>();
    for (uint32_t i = 0; i < count; i++)
    {
        const Query& query = m_queries[queryIndex + i];
        if (isOcclusionQueryType(m_desc.type))
        {
            if (!query.timestampQuery)
                return SLANG_FAIL;
            uint64_t value = 0;
            BOOL predicate = FALSE;
            void* data =
                m_desc.type == QueryType::Occlusion ? static_cast<void*>(&predicate) : static_cast<void*>(&value);
            UINT size = m_desc.type == QueryType::Occlusion ? sizeof(predicate) : sizeof(value);
            HRESULT hr = S_FALSE;
            while ((hr = device->m_immediateContext->GetData(query.timestampQuery, data, size, 0)) == S_FALSE)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            SLANG_D3D_RETURN_ON_FAIL_REPORT(hr, device);
            outData[i] = m_desc.type == QueryType::Occlusion ? uint64_t(predicate != FALSE) : value;
            continue;
        }
        if (!query.timestampQuery || !query.disjointQuery)
        {
            return SLANG_FAIL;
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData = {};
        HRESULT hr = S_FALSE;
        while (
            (hr = device->m_immediateContext
                      ->GetData(query.disjointQuery, &disjointData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0)) ==
            S_FALSE
        )
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        SLANG_D3D_RETURN_ON_FAIL_REPORT(hr, device);
        if (disjointData.Disjoint || disjointData.Frequency == 0)
        {
            return SLANG_FAIL;
        }
        device->m_info.timestampFrequency = disjointData.Frequency;

        while ((hr = device->m_immediateContext->GetData(query.timestampQuery, outData + i, sizeof(uint64_t), 0)) ==
               S_FALSE)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        SLANG_D3D_RETURN_ON_FAIL_REPORT(hr, device);
    }

    markQueryRangeResolved(queryIndex, count, queryInfo.submissionID);

    return SLANG_OK;
}

Result DeviceImpl::createQueryPool(const QueryPoolDesc& desc, IQueryPool** outPool)
{
    RefPtr<QueryPoolImpl> result = new QueryPoolImpl(this, desc);
    SLANG_RETURN_ON_FAIL(result->init());
    returnComPtr(outPool, result);
    return SLANG_OK;
}

} // namespace rhi::d3d11
