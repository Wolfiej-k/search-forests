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

struct range_tree {
    size_t rangemax;
    std::vector<std::vector<size_t>> nodes;

    range_tree(std::vector<size_t>& data) {
        rangemax = data.size() - 1;
        nodes.resize(4 * data.size());
        build(1, 0, rangemax, data);
    }

    size_t query(size_t l, size_t r, size_t val) {
        if (l > rangemax) {
            return -1;
        }
        r = std::min(r, rangemax);
        return query(1, 0, rangemax, l, r, val);
    }

private:
    void build(size_t v, size_t l, size_t r, std::vector<size_t>& data) {
        if (l == r) {
            nodes[v].push_back(data[l]);
            assert(nodes[v].size() == 1);
        } else {
            int m = (l + r) / 2;
            build(2 * v, l, m, data);
            build(2 * v + 1, m + 1, r, data);
            nodes[v].resize(r - l + 1);
            std::merge(nodes[2 * v].begin(), nodes[2 * v].end(),
                        nodes[2 * v + 1].begin(), nodes[2 * v + 1].end(),
                        nodes[v].begin());
        }
    }

    size_t query(size_t v, size_t l, size_t r, size_t ql, size_t qr, size_t val) {
        if (ql > qr) {
            return 0;
        } else if (l == ql && r == qr) {
            auto it = std::upper_bound(nodes[v].begin(), nodes[v].end(), val);
            return nodes[v].end() - it;
        } else {
            size_t m = (l + r) / 2;
            size_t left = query(2 * v, l, m, ql, std::min(m, qr), val);
            size_t right = query(2 * v + 1, m + 1, r, std::max(ql, m + 1), qr, val);
            return left + right;
        }
    }
};

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
        throw std::invalid_argument("invalid epsilon");
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
    std::vector<size_t> next_access(queries.size());
    std::vector<size_t> last_seen(num_keys, -1);

    for (long i = queries.size() - 1; i >= 0; i--) {
        next_access[i] = last_seen[queries[i]];
        last_seen[queries[i]] = i;
    }

    std::vector<std::deque<size_t>> accesses(num_keys);
    range_tree tree(next_access);
    for (size_t i = 0; i < queries.size(); i++) {
        if (next_access[i] != -1) {
            size_t next = next_access[i] ? tree.query(i + 1, next_access[i] - 1, next_access[i]) : 0;
            scale_and_shift(next, num_keys, epsilon, delta, gen);
            accesses[queries[i]].push_back(next);
        }
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