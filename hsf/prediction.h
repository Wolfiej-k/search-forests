#ifndef HSF_PREDICTIONS_H
#define HSF_PREDICTIONS_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <vector>

namespace hsf {

template <typename Key, typename Value = uint8_t, typename Hash = std::hash<Key>>
class prediction_sketch {
public:
    using key_type = Key;
    using value_type = Value;
    using hash_type = Hash;

    prediction_sketch(size_t keys, size_t hashes) 
        : table_(hashes, std::vector<value_type>(keys, empty_cell)), 
          collision_(hashes, std::vector<bool>(keys, false)),
          a_(hashes), b_(hashes) {
        std::mt19937_64 rng(2241);
        std::uniform_int_distribution<uint64_t> dist_a(1, prime - 1);
        std::uniform_int_distribution<uint64_t> dist_b(0, prime - 1);

        for (size_t i = 0; i < hashes; ++i) {
            a_[i] = dist_a(rng);
            b_[i] = dist_b(rng);
        }
    }

    void insert(const key_type& key, value_type value) {
        for (size_t i = 0; i < table_.size(); i++) {
            size_t idx = index(key, i);
            if (table_[i][idx] == empty_cell) {
                table_[i][idx] = value;
            } else {
                table_[i][idx] = std::min(value, table_[i][idx]);
                collision_[i][idx] = true;
            }
        }
    }

    void update(const key_type& key, value_type value) {
        for (size_t i = 0; i < table_.size(); i++) {
            size_t idx = index(key, i);
            if (!collision_[i][idx]) {
                table_[i][idx] = value;
            } else {
                table_[i][idx] = std::min(value, table_[i][idx]);
            }
        }
    }

    value_type get(const key_type& key) const {
        value_type result = 0;
        for (size_t i = 0; i < table_.size(); i++) {
            const auto& cell = table_[i][index(key, i)];
            result = std::max(result, cell);
        }
        return result;
    }

private:
    static constexpr value_type empty_cell = std::numeric_limits<value_type>::max();
    static constexpr uint64_t prime = INT_MAX;
    
    [[no_unique_address]] hash_type hasher_;
    std::vector<std::vector<value_type>> table_;
    std::vector<std::vector<bool>> collision_;
    std::vector<uint64_t> a_;
    std::vector<uint64_t> b_;

    size_t index(const key_type& key, size_t i) const {
        uint64_t x = static_cast<uint64_t>(hasher_(key));
        return ((a_[i] * x + b_[i]) % prime) % table_[i].size();
    }
};

template <typename Capacity>
size_t prediction_to_level(size_t prediction, const Capacity& capacity) {
    size_t level = 0;
    size_t offset = 0;
    while (true) {
        offset += capacity(level);
        if (prediction < offset) {
            return level;
        }
        level++;
    }
}

}

#endif