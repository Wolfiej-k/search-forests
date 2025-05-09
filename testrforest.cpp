#include <map>
#include <random>
#include <iostream>

#define HSF_DEBUG
#include "hsf/recency.h"
#include "benchmark/benchmark.h"

using learned_r_forest = hsf::learned_recency_forest<hsf::capacity, std::map, int, std::less<int>>;

int main() {
    learned_r_forest lrf(hsf::capacity(1.0, 1.1), hsf::capacity(2.0, 1.1));

    static constexpr size_t num_keys = 100'000;
    static constexpr size_t num_queries = 100'000;
    
    std::mt19937 gen;
    auto queries = hsf::bench::generate_zipf_queries<int>(num_keys, num_queries, 1.0, gen);
    auto accesses = hsf::bench::generate_noisy_accesses(queries, num_keys, 1, 0, gen);
    
    for (int i = 0; i < num_keys; i++) {
        lrf.insert(i, accesses[i].empty() ? -1 : accesses[i].front() + (num_keys - i - 1));
    }

    for (const auto& query : queries) {
        assert(!accesses[query].empty());
        size_t prev_access = accesses[query].front();
        accesses[query].pop_front();
        size_t next_access = accesses[query].empty() ? -1 : accesses[query].front();
        auto it = lrf.find(query, prev_access, next_access);
        assert(it != lrf.end());
    }

    std::cout << "compactions: " << lrf.compactions_ << std::endl;

    return 0;
}