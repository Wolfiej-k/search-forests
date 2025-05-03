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
#include "btree/map.h"
#include "skiplist/skip_list.h"
#include "treap.h"
#include "zipf.h"

using namespace hsf;
using namespace std::chrono;

size_t forest_comparisons{0};
size_t lforest_comparisons{0};
size_t rb_comparisons{0};
size_t sl_comparisons{0};

struct CountingCmpForest {
    bool operator()(int a, int b) const {
        ++forest_comparisons;
        return a < b;
    }
};

struct CountingCmpLForest {
    bool operator()(int a, int b) const {
        ++lforest_comparisons;
        return a < b;
    }
};

struct CountingCmpRB {
    bool operator()(int a, int b) const {
        ++rb_comparisons;
        return a < b;
    }
};

struct CountingCmpSL {
    bool operator()(int a, int b) const {
        ++sl_comparisons;
        return a < b;
    }
};

struct Key {
    int value;
    Key(int value) : value(value) {}
    Key() : value(0) {}
    operator int() const {
        return value;
    }
};

template<class Key, class Value, class Compare = std::less<Key>, class Alloc = std::allocator<std::pair<const Key, Value>>>
using BTreeMap = btree::map<Key, Value, Compare, Alloc, 256>;

template<typename Compare>
using MapWithCompare = std::map<Key, int, Compare>;

template<typename Compare>
using ForestWithCompare = frequency_forest<capacity, std::map, Key, Compare>;

template<typename Compare>
using LearnedForestWithCompare = learned_frequency_forest<capacity, std::map, Key, Compare>;

template<typename Compare>
using RobustSLWithCompare = goodliffe::skip_list<Key, Compare>;

constexpr size_t NUM_KEYS = 1'000'000;
constexpr size_t NUM_ACCESSES = 1'000'000;
constexpr double ZIPF_THETA = 0.5;

auto generate_queries(std::default_random_engine& gen) {
    zipfian_int_distribution<int> zipf(0, NUM_KEYS - 1, ZIPF_THETA);

    std::vector<size_t> frequency(NUM_KEYS);
    std::vector<Key> queries;
    queries.reserve(NUM_ACCESSES);

    std::vector<int> perm(NUM_KEYS);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), gen);
    
    for (size_t i = 0; i < NUM_ACCESSES; i++) {
        auto query = perm[zipf(gen)];;
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
        if (a.second != b.second)
            return a.second > b.second;
        return a.first < b.first;
    });

    for (size_t i = 0; i < n; ++i) {
        rank[indexed_freq[i].first] = i;
    }

    return rank;
}

auto compute_sl_levels(const std::vector<size_t>& freq, std::default_random_engine& gen) {
    double p = 0.368;
    double theta = 0.05;
    double K = 1 + std::ceil(log2(log2(NUM_KEYS)) - log2(2 * log2(theta) / log2(p)));
    
    std::vector<int> D(K+1, 0);
    std::geometric_distribution<> geom(1-p);
    
    D[0] = std::ceil(log2(theta) / log2(p));
    for (int i = 1; i <= K; i++) {
        D[i] = D[i-1] + std::ceil(log2(theta) / log2(p) * (1 << i));
    }

    std::vector<size_t> levels(NUM_KEYS);
    for (int i = 0; i < NUM_KEYS; i++) {
        double f = double(freq[i]) / double(NUM_ACCESSES);
        int c;
        if (f >= theta) {
            c = 0;
        } else if (f == 0) {
            c = K;
        } else {
            c = std::ceil(log2(-std::min(-log2(f), -log2(p) * log2(NUM_KEYS) / 2) / log2(theta)));
        }

        levels[i] = D[K] - D[c] + geom(gen) + 1;
    }

    return levels;
}

int main() {
    std::default_random_engine gen;
    auto [queries, frequency] = generate_queries(gen);
    auto rank = compute_ranks(frequency);
    auto levels = compute_sl_levels(frequency, gen);

    ForestWithCompare<CountingCmpForest> forest(capacity(1.0, 2.0), capacity(1.1, 2.0));
    LearnedForestWithCompare<CountingCmpLForest> learned_forest(capacity(1.0, 1.1), capacity(1.1, 1.1));
    MapWithCompare<CountingCmpRB> rb_tree;
    RobustSLWithCompare<CountingCmpSL> robust_sl;
    learned_treap treap;

    for (int i = 0; i < NUM_KEYS; ++i) {
        forest.insert(i);
        learned_forest.insert(i, rank[i]);
        rb_tree[i] = i;
        robust_sl.insert(i, levels[i]);
        treap.insert(i, rank[i]);
    }

    forest_comparisons = 0;
    lforest_comparisons = 0;
    rb_comparisons = 0;
    sl_comparisons = 0;
    treap.ncomparisons = 0;

    for (Key k : queries) {
        auto it1 = forest.find(k);
        assert(it1 != forest.end() && it1->first == k);

        auto it2 = learned_forest.find(k, prediction_to_level(rank[k], capacity(1.0)));
        assert(it2 != learned_forest.end() && it2->first == k);

        auto it3 = rb_tree.find(k);
        assert(it3 != rb_tree.end() && it3->first == k);

        auto it4 = robust_sl.find(k);
        assert(it4 != robust_sl.end());

        auto it5 = treap.find(k);
        assert(it5 != nullptr);
    }

    std::cout << "\n================== COMPARISON COUNT ==================\n";
    std::cout << "Total lookups: " << NUM_ACCESSES << "\n";
    std::cout << "Comparisons (forest):         " << forest_comparisons << "\n";
    std::cout << "Comparisons (learned_forest): " << lforest_comparisons << "\n";
    std::cout << "Comparisons (learned_treap):  " << treap.ncomparisons << "\n";
    std::cout << "Comparisons (robust_sl):      " << sl_comparisons << "\n";
    std::cout << "Comparisons (std::map):       " << rb_comparisons << "\n";

    double avg_forest = double(forest_comparisons) / NUM_ACCESSES;
    double avg_lforest = double(lforest_comparisons) / NUM_ACCESSES;
    double avg_sl = double(sl_comparisons) / NUM_ACCESSES;
    double avg_treap = double(treap.ncomparisons) / NUM_ACCESSES;
    double avg_rb = double(rb_comparisons) / NUM_ACCESSES;
    std::cout << "Avg comparisons per access:\n";
    std::cout << "  forest:         " << std::fixed << std::setprecision(2) << avg_forest << "\n";
    std::cout << "  learned_forest: " << std::fixed << std::setprecision(2) << avg_lforest << "\n";
    std::cout << "  robust_sl:      " << std::fixed << std::setprecision(2) << avg_sl << "\n";
    std::cout << "  learned_treap:  " << std::fixed << std::setprecision(2) << avg_treap << "\n";
    std::cout << "  std::map:       " << avg_rb << "\n";

    std::cout << "\n================== FOREST STATS ==================\n";
    std::cout << "Total compactions: " << forest.compactions_ << "\n";
    std::cout << "Total promotions:  " << forest.promotions_ << "\n";
    std::cout << "Total mispredictions: " << forest.mispredictions_ << "\n";
    std::cout << "Levels:            " << forest.levels() << "\n";

    std::cout << "\n================== LEARNED FOREST STATS ==================\n";
    std::cout << "Total compactions: " << learned_forest.compactions_ << "\n";
    std::cout << "Total promotions:  " << learned_forest.promotions_ << "\n";
    std::cout << "Total mispredictions: " << learned_forest.mispredictions_ << "\n";
    std::cout << "Levels:            " << learned_forest.levels() << "\n";

    return 0;
}

