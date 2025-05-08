#ifndef HSF_BENCHMARK_H
#define HSF_BENCHMARK_H

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <deque>
#include <vector>
#include <iostream>

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

template <typename Gen>
size_t scale_and_shift(size_t value, size_t max_value, size_t epsilon, size_t delta, Gen& gen) {
    if (epsilon < 1.0) {
        throw std::invalid_argument("invalid error");
    }
    
    std::uniform_real_distribution<double> scale(1.0, std::nextafter(epsilon, DBL_MAX));
    std::uniform_real_distribution<double> shift(-static_cast<double>(delta), std::nextafter(delta, DBL_MAX));
    std::bernoulli_distribution bern(0.5);

    double scale_value = epsilon == 1 ? 1 : scale(gen);
    double shift_value = delta == 0 ? 0 : shift(gen);
    double new_value;
    if (bern(gen)) {
        new_value = value * scale_value + shift_value;
    } else {
        new_value = value / scale_value + shift_value;
    }

    return std::clamp<size_t>(std::round<size_t>(new_value), static_cast<size_t>(0), max_value);
}

template <typename Key, typename Gen>
std::vector<size_t> generate_noisy_frequencies(const std::vector<Key>& queries, size_t num_keys, size_t epsilon, size_t delta, Gen& gen) {
    std::vector<size_t> frequencies(num_keys);
    for (const auto& query : queries) {
        frequencies[query]++;
    }
    
    for (size_t i = 0; i < frequencies.size(); i++) {
        frequencies[i] = scale_and_shift(frequencies[i], queries.size(), epsilon, delta, gen);
    }

    return frequencies;
}

template <typename Key, typename Gen>
std::vector<std::deque<size_t>> generate_noisy_accesses(const std::vector<Key>& queries, size_t num_keys, size_t epsilon, size_t delta, Gen& gen) {
    std::vector<std::deque<size_t>> accesses(num_keys);

    size_t distinct_accesses = 0;
    size_t prev_query = 0;
    for (const auto& query : queries) {
        auto& queue = accesses[query];
        size_t prev_access = queue.empty() ? 0 : queue.back();
        size_t next_access = distinct_accesses - prev_access;
        queue.push_back(scale_and_shift(next_access, num_keys, epsilon, delta, gen));

        if (query != prev_query) {
            distinct_accesses++;
        }
        prev_query = query;
    }

    return accesses;
}

template <typename Gen>
std::vector<size_t> generate_noisy_ranks(const std::vector<size_t>& frequencies, size_t epsilon, size_t delta, Gen& gen) {
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
        ranks[indices[i]] = scale_and_shift(i, frequencies.size(), epsilon, delta, gen);
    }
    
    return ranks;
}

}

}

#endif