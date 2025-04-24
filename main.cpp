#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <set>

#define HSF_DEBUG
#include "hsf/hsf.h"
#include "btree/set.h"
#include "zipf.h"

struct capacity {
    double base;
    double fill_factor;
    size_t top_level_size;
    
    capacity(double fill_factor = 1.0)
        : base(1.1), fill_factor(fill_factor), top_level_size(256) {}
    
    size_t operator()(size_t level) const {
        return std::pow(base, std::pow(base, level)) * top_level_size * fill_factor;
    }
};

size_t comparison_count = 0;
struct counting_comparator {
    bool operator()(int left, int right) const {
        comparison_count++;
        return left < right;
    }
};

struct int_wrapper {
    int value;
    int_wrapper(int value) : value(value) {}
    int_wrapper() : value(0) {}
    operator int() const {
        return value;
    }
};

using rbtree = std::set<int, counting_comparator>;
using btree_linear = btree::set<int, counting_comparator>;
using btree_binary = btree::set<int_wrapper, counting_comparator>;
using search_forest = hsf::search_forest<capacity, rbtree>;

size_t index_to_level(size_t index, const capacity& max_capacity) {
    size_t level = 0;
    size_t offset = 0;

    while (true) {
        size_t capacity = max_capacity(level);
        if (index < offset + capacity) {
            return level;
        }
        offset += capacity;
        level++;
    }
}

int main() {
    constexpr size_t N = 1'000'000;
    zipfian_int_distribution<int> zipf(0, N-1, 0.5);
    std::normal_distribution<double> normal(10'000, 2'000);
    std::default_random_engine gen;
    
    constexpr size_t M = 1'000'000;
    std::vector<int> perm(N);
    std::vector<int> queries(M);
    std::vector<int> frequency(N, 0);
    std::vector<int> level(N, 0);
    std::vector<int> rank(N);

    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), gen);

    for (size_t i = 0; i < M; i++) {
        queries[i] = perm[zipf(gen)];
        frequency[queries[i]]++;
    }

    std::iota(rank.begin(), rank.end(), 0);
    std::sort(rank.begin(), rank.end(), [&](int a, int b) {
        return frequency[a] > frequency[b];
    });

    for (size_t i = 0; i < N; i++) {
        int r = i + normal(gen);
        level[rank[i]] = index_to_level(r, capacity());
    }

    perm.clear();
    rank.clear();

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    {
        rbtree baseline;

        comparison_count = 0;
        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; i++) {
            baseline.insert(i);
        }
        auto t2 = high_resolution_clock::now();
        auto elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "red-black tree build: " << elapsed << "ms, " << comparison_count / double(N) << " comps\n";

        comparison_count = 0;
        t1 = high_resolution_clock::now();
        for (int q : queries) {
            assert(baseline.find(q) != baseline.end());
        }
        t2 = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "red-black tree queries: " << elapsed << "ms, " << comparison_count / double(M) << " comps\n\n";
    }

    {
        btree_binary baseline;

        comparison_count = 0;
        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; i++) {
            baseline.insert(i);
        }
        auto t2 = high_resolution_clock::now();
        auto elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "btree build: " << elapsed << "ms, " << comparison_count / double(N) << " comps\n";

        comparison_count = 0;
        t1 = high_resolution_clock::now();
        for (int q : queries) {
            assert(baseline.find(q) != baseline.end());
        }
        t2 = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "btree queries: " << elapsed << "ms, " << comparison_count / double(M) << " comps\n\n";
    }

    {
        search_forest forest(capacity(0.5), capacity(1.0));

        comparison_count = 0;
        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; i++) {
            forest.insert(i);
        }
        auto t2 = high_resolution_clock::now();
        auto elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "sf build: " << elapsed << "ms, " << comparison_count / double(N) << " comps\n";

        comparison_count = 0;
        t1 = high_resolution_clock::now();
        for (int q : queries) {
            assert(forest.find(q) != forest.end());
        }
        t2 = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "sf queries: " << elapsed << "ms, " << comparison_count / double(M) << " comps\n";

        std::cout << "sf compactions: " << forest.compactions_ << "\n";
        std::cout << "sf mispredictions: " << forest.mispredictions_ << "\n\n";
    }

    {
        search_forest forest(capacity(0.95), capacity(1.05));

        comparison_count = 0;
        auto t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; i++) {
            forest.insert(i, level[i]);
        }
        auto t2 = high_resolution_clock::now();
        auto elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "fsf build: " << elapsed << "ms, " << comparison_count / double(N) << " comps\n";

        comparison_count = 0;
        t1 = high_resolution_clock::now();
        for (int q : queries) {
            assert(forest.find(q, level[q]) != forest.end());
        }
        t2 = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "fsf queries: " << elapsed << "ms, " << comparison_count / double(M) << " comps\n";

        std::cout << "fsf compactions: " << forest.compactions_ << "\n";
        std::cout << "fsf mispredictions: " << forest.mispredictions_ << "\n\n";
    }
}