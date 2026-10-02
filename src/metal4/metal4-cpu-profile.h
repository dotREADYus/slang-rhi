#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace rhi::metal4::cpu_profile {
// Optional diagnostic totals on the recording thread. Nested scopes overlap;
// compare inclusive phases rather than summing them as exclusive CPU cost.
inline bool enabled()
{
    static const bool value = std::getenv("SLANG_RHI_CPU_DETAIL") != nullptr;
    return value;
}
inline thread_local double milliseconds[6]{};
inline thread_local unsigned long long calls[6]{};
struct Scope
{
    unsigned phase;
    std::chrono::steady_clock::time_point start{};
    explicit Scope(unsigned p)
        : phase(p)
    {
        if (enabled())
            start = std::chrono::steady_clock::now();
    }
    ~Scope()
    {
        if (enabled())
        {
            milliseconds[phase] +=
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            ++calls[phase];
        }
    }
};
inline void emit()
{
    if (!enabled())
        return;
    static auto* file = []
    {
        auto* f = std::fopen(std::getenv("SLANG_RHI_CPU_DETAIL"), "w");
        if (f)
            std::fprintf(
                f,
                "wall_s,track_ms,layout_ms,build_binding_ms,resolve_pipeline_ms,record_native_ms,argument_table_ms,"
                "track_calls,layout_calls,build_binding_calls,resolve_pipeline_calls,record_native_calls,argument_"
                "table_calls\n"
            );
        return f;
    }();
    static auto* mutex = new std::mutex;
    std::lock_guard<std::mutex> lock(*mutex);
    if (file)
    {
        std::fprintf(
            file,
            "%.9f",
            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()
        );
        for (double ms : milliseconds)
            std::fprintf(file, ",%.6f", ms);
        for (auto count : calls)
            std::fprintf(file, ",%llu", count);
        std::fprintf(file, "\n");
    }
    for (auto& ms : milliseconds)
        ms = 0.;
    for (auto& count : calls)
        count = 0;
}
} // namespace rhi::metal4::cpu_profile
