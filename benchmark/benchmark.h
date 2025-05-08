#ifndef HSF_BENCHMARK_H
#define HSF_BENCHMARK_H

#include <algorithm>
#include <deque>
#include <vector>

#include "zipf.h"

namespace hsf {

namespace bench {

template <typename Key, typename Gen>
std::vector<Key> generate_zipf_queries(size_t num_keys, size_t num_queries, double alpha, Gen& gen) {
    std::vector<size_t> perm(num_keys);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), gen);

    zipfian_int_distribution<Key> zipf(0, num_keys - 1, alpha);
    std::vector<Key> queries(num_queries);
    for (size_t i = 0; i < num_queries; i++) {
        queries[i] = perm[zipf(gen)];
    }

    return queries;
}

template <typename Key, typename Gen>
std::vector<size_t> generate_noisy_frequencies(const std::vector<Key>& queries, size_t num_keys, size_t delta, Gen& gen) {
    std::vector<size_t> frequencies(num_keys);
    for (const auto& query : queries) {
        frequencies[query]++;
    }

    std::uniform_int_distribution<Key> unif(1, delta);
    std::bernoulli_distribution bern(0.5);
    
    for (auto& freq : frequencies) {
        size_t scale = unif(gen);
        if (bern(gen)) {
            freq *= scale;
        } else {
            freq /= scale;
        }
    }

    return frequencies;
}

template <typename Key, typename Gen>
std::vector<std::deque<size_t>> generate_noisy_accesses(const std::vector<Key>& queries, size_t num_keys, size_t delta, Gen& gen) {
    std::vector<std::deque<size_t>> accesses(num_keys);

    std::uniform_int_distribution<Key> unif(1, delta);
    std::bernoulli_distribution bern(0.5);
    size_t distinct_accesses = 0;
    size_t prev_query = 0;
    
    for (const auto& query : queries) {
        auto& queue = accesses[query];
        size_t prev_access = queue.empty() ? 0 : queue.back();
        size_t next_access = distinct_accesses - prev_access;
        
        // size_t scale = unif(gen);
        // if (bern(gen)) {
        //     next_access *= scale;
        // } else {
        //     next_access /= scale;
        // }
        // next_access = std::min(next_access, queries.size() - 1);
        
        queue.push_back(next_access);

        if (query != prev_query) {
            distinct_accesses++;
        }
        prev_query = query;
    }

    return accesses;
}

std::vector<size_t> compute_ranks(const std::vector<size_t>& frequencies) {
    std::vector<size_t> indices(frequencies.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t left, size_t right) {
        if (frequencies[left] != frequencies[right]) {
            return frequencies[left] > frequencies[right];
        }
        return left < right;
    });
    
    std::vector<size_t> ranks(frequencies.size());
    for (size_t i = 0; i < frequencies.size(); i++) {
        ranks[indices[i]] = i;
    }
    
    return ranks;
}

}

}

#endif