#include <iostream>
#include <map>
#include <chrono>
#include <random>
#include <vector>
#include <atomic>
#include <cassert>
#include <iomanip>
#include <set>

#define HSF_DEBUG
#include "hsf/frequency.h"
#include "hsf/prediction.h"
#include "zipf.h"

using namespace hsf;
using namespace std::chrono;

size_t forest_comparisons{0};
size_t rb_comparisons{0};

struct CountingCmpForest {
    bool operator()(int a, int b) const {
        ++forest_comparisons;
        return a < b;
    }
};

struct CountingCmpRB {
    bool operator()(int a, int b) const {
        ++rb_comparisons;
        return a < b;
    }
};

using Key = int;
template<typename Compare>
using MapWithCompare = std::map<Key, int, Compare>;

template<typename Compare>
using ForestWithCompare = learned_frequency_forest<capacity, std::map, Key, Compare>;

constexpr size_t NUM_KEYS = 1'000'000;
constexpr size_t NUM_ACCESSES = 1'000'000;

auto generate_queries(std::mt19937& rng) {
    zipfian_int_distribution<int> zipf(0, NUM_KEYS - 1, 0.5);
    std::default_random_engine gen;
    
    std::vector<size_t> frequency(NUM_KEYS);
    std::vector<Key> queries;
    queries.reserve(NUM_ACCESSES);
    
    for (size_t i = 0; i < NUM_ACCESSES; i++) {
        auto query = zipf(gen);
        queries.push_back(query);
        frequency[query]++;
    }

    return std::make_pair(queries, frequency);
}

auto compute_ranks(const std::vector<size_t>& freq) {
    size_t n = freq.size();
    std::vector<size_t> rank(n);
    std::vector<std::pair<size_t, size_t>> indexed_freq;

    for (size_t i = 0; i < n; ++i) {
        indexed_freq.emplace_back(i, freq[i]);
    }

    std::sort(indexed_freq.begin(), indexed_freq.end(), [](auto& a, auto& b) {
        return a.second > b.second;
    });

    size_t current_rank = 1;
    for (size_t i = 0; i < n; ++i) {
        if (i > 0 && indexed_freq[i].second != indexed_freq[i - 1].second) {
            current_rank = i + 1;
        }
        rank[indexed_freq[i].first] = current_rank;
    }

    return rank;
}

int main() {
    std::mt19937 rng(1234);
    auto [queries, frequency] = generate_queries(rng);
    auto rank = compute_ranks(frequency);

    ForestWithCompare<CountingCmpForest> forest(capacity(1.0), capacity(1.1));
    MapWithCompare<CountingCmpRB> rb_tree;

    // Insert
    for (Key i = 0; i < NUM_KEYS; ++i) {
        forest.insert(i, rank[i]);
        rb_tree[i] = i;
    }

    // Reset counters
    forest_comparisons = 0;
    rb_comparisons = 0;

    // Lookup benchmark
    for (Key k : queries) {
        auto it1 = forest.find(k, prediction_to_level(rank[k], capacity(1.0)));
        assert(it1 != forest.end() && it1->first == k);

        auto it2 = rb_tree.find(k);
        assert(it2 != rb_tree.end() && it2->first == k);
    }

    std::cout << "\n================== COMPARISON COUNT ==================\n";
    std::cout << "Total lookups: " << NUM_ACCESSES << "\n";
    std::cout << "Comparisons (frequency_forest): " << forest_comparisons << "\n";
    std::cout << "Comparisons (std::map):         " << rb_comparisons << "\n";

    double avg_forest = double(forest_comparisons) / NUM_ACCESSES;
    double avg_rb     = double(rb_comparisons) / NUM_ACCESSES;
    std::cout << "Avg comparisons per access:\n";
    std::cout << "  frequency_forest: " << std::fixed << std::setprecision(2) << avg_forest << "\n";
    std::cout << "  std::map:         " << avg_rb << "\n";

    std::cout << "\n================== FOREST STATS ==================\n";
    std::cout << "Total compactions: " << forest.compactions_ << "\n";
    std::cout << "Total promotions:  " << forest.promotions_ << "\n";
    std::cout << "Total mispredictions: " << forest.mispredictions_ << "\n";
    std::cout << "Levels:            " << forest.levels() << "\n";

    // for (size_t i = 0; i < forest.levels(); ++i) {
    //     auto [min_cap, max_cap] = forest.capacity(i);
    //     std::cout << "  Level " << i
    //               << ": size = " << forest.size(i)
    //               << ", min = " << min_cap
    //               << ", max = " << max_cap << "\n";
    // }

    return 0;
}

