#pragma once

#include <cstdint>
#include <functional>

namespace PixelEngine {

    class UUID {
    public:
        UUID();
        UUID(uint64_t uuid);
        UUID(const UUID&) = default;

        operator uint64_t() const { return m_UUID; }
    private:
        uint64_t m_UUID;
    };

}

namespace std {
    template<>
    struct hash<PixelEngine::UUID> {
        std::size_t operator()(const PixelEngine::UUID& uuid) const {
            return (uint64_t)uuid;
        }
    };
}
