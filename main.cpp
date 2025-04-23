#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <set>

#include "hsf/hsf.h"
#include "zipf.h"

struct growth_policy {
    constexpr static double alpha = 2;
    constexpr static size_t top_level_size = 256;
    size_t operator()(size_t level) const {
        return std::pow(alpha, std::pow(alpha, level)) * top_level_size;
    }
};

size_t comparison_count = 0;
struct counting_comparator {
    bool operator()(int left, int right) const {
        comparison_count++;
        return left < right;
    }
};

size_t index_to_level(size_t index, const growth_policy& policy) {
    size_t level = 0;
    size_t offset = 0;

    while (true) {
        size_t capacity = policy(level);
        if (index < offset + capacity) {
            return level;
        }
        offset += capacity;
        level++;
    }
}

int main() {
    constexpr size_t N = 10'000'000;
    zipfian_int_distribution<int> zipf(0, N-1, 0.999);
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
        size_t j = rank[i];
        level[j] = index_to_level(i, growth_policy());
    }

    perm.clear();
    rank.clear();

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    {
       std::set<int, counting_comparator> baseline;

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
        std::cout << "red-black tree queries: " << elapsed << "ms, " << comparison_count / double(M) << " comps\n";

        comparison_count = 0;
        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; i++) {
            assert(baseline.erase(i));
        }
        t2 = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "red-black tree deletes: " << elapsed << "ms, " << comparison_count / double(N) << " comps\n\n";
    }

    {
        using search_forest = hsf::search_forest<growth_policy, std::set<int, counting_comparator>>;
        search_forest forest(growth_policy(), 0.5);

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

        comparison_count = 0;
        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; i++) {
            assert(forest.erase(i));
        }
        t2 = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "sf deletes: " << elapsed << "ms, " << comparison_count / double(N) << " comps\n\n";
    }

    {
        using search_forest = hsf::search_forest<growth_policy, std::set<int, counting_comparator>>;
        search_forest forest(growth_policy(), 0.5);

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

        comparison_count = 0;
        t1 = high_resolution_clock::now();
        for (size_t i = 0; i < N; i++) {
            assert(forest.erase(i, level[i]));
        }
        t2 = high_resolution_clock::now();
        elapsed = duration_cast<milliseconds>(t2 - t1).count();
        std::cout << "fsf deletes: " << elapsed << "ms, " << comparison_count / double(N) << " comps\n\n";
    }
}