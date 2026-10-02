#include "testing.h"
#include "metal4/metal4-buffer.h"
#include "metal4/metal4-shader-object.h"

using namespace rhi;

TEST_CASE("metal4-binding-capacity")
{
    ArenaAllocator allocator(1024);
    metal4::BindingDataImpl data = {};
    data.bufferCapacity = data.textureCapacity = 1;
    data.buffers = allocator.allocate<MTL::Buffer*>(1);
    data.bufferOffsets = allocator.allocate<NS::UInteger>(1);
    data.textures = allocator.allocate<MTL::Texture*>(1);
    data.buffers[0] = nullptr;
    data.bufferOffsets[0] = 0;
    data.textures[0] = nullptr;
    metal4::BindingDataBuilder builder = {};
    builder.m_allocator = &allocator;
    builder.m_bindingData = &data;
    // Opaque tokens are only stored/copied by these helpers, never dereferenced.
    auto buffer = reinterpret_cast<MTL::Buffer*>(uintptr_t(0x1000));
    auto texture = reinterpret_cast<MTL::Texture*>(uintptr_t(0x2000));
    REQUIRE_CALL(builder.setBuffer(0, buffer, 64));
    REQUIRE_CALL(builder.setTexture(0, texture));
    auto firstBuffers = data.buffers;
    auto firstTextures = data.textures;
    REQUIRE_CALL(builder.setBuffer(17, buffer, 128));
    REQUIRE_CALL(builder.setTexture(31, texture));
    CHECK(data.buffers[0] == buffer);
    CHECK(data.bufferOffsets[0] == 64);
    CHECK(data.bufferOffsets[17] == 128);
    CHECK(data.textures[0] == texture);
    for (uint32_t i = 1; i < 17; ++i)
    {
        CHECK(data.buffers[i] == nullptr);
        CHECK(data.bufferOffsets[i] == 0);
    }
    for (uint32_t i = 1; i < 31; ++i) CHECK(data.textures[i] == nullptr);
    REQUIRE_CALL(builder.setBuffer(255, buffer, 256));
    REQUIRE_CALL(builder.setTexture(255, texture));
    CHECK(data.bufferCount == 256);
    CHECK(data.textureCount == 256);
    CHECK(data.buffers[17] == buffer);
    CHECK(data.textures[31] == texture);
    CHECK(firstBuffers[0] == buffer);
    CHECK(firstTextures[0] == texture);
    CHECK(builder.setBuffer(256, buffer) == SLANG_FAIL);
    CHECK(builder.setTexture(256, texture) == SLANG_FAIL);
}
