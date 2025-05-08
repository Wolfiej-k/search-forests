#ifndef HSF_SKIPLIST_H
#define HSF_SKIPLIST_H

#include <cmath>
#include <deque>
#include <random>
#include <vector>

#include "skiplist/skip_list.h"

namespace hsf {

namespace bench {

template <typename Key, typename Compare>
using skiplist = goodliffe::skip_list<Key, Compare>;

template <typename Gen>
std::vector<size_t> skiplist_levels(const std::vector<size_t>& frequencies, size_t num_queries, Gen& gen) {
    constexpr double p = 0.368;
    constexpr double theta = 0.05;
    size_t num_keys = frequencies.size();
    size_t K = 1 + std::ceil(std::log2(std::log2(num_keys)) - std::log2(2 * std::log2(theta) / std::log2(p)));

    std::vector<int> D(K + 1, 0);
    std::geometric_distribution<int> geom(1 - p);

    D[0] = std::ceil(std::log2(theta) / log2(p));
    for (int i = 1; i <= K; i++) {
        D[i] = D[i-1] + std::ceil(std::log2(theta) / std::log2(p) * (1 << i));
    }

    std::vector<size_t> levels(num_keys);
    for (int i = 0; i < num_keys; i++) {
        double f = frequencies[i] / double(num_queries);
        size_t c;
        if (f >= theta) {
            c = 0;
        } else if (f == 0) {
            c = K;
        } else {
            c = std::ceil(std::log2(-std::min(-std::log2(f), -std::log2(p) * std::log2(num_keys) / 2) / std::log2(theta)));
        }

        levels[i] = D[K] - D[c] + geom(gen) + 1;
    }

    return levels;
}

}   

}

#endif