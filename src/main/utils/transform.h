#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__

#include <vector>
#include <unordered_map>

namespace utils {
    // K and V must be copiable.
    template<typename K, typename V, typename T>
    std::unordered_map<K, V> to_map(const std::vector<V> & values, T transformer) {
        std::unordered_map<K, V> result;
        result.reserve(values.size());

        for (const V & value : values) {
            result.emplace(transformer(value), value);
        }

        return result;
    }
}

#endif