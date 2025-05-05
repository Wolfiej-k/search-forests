#include <cassert>
#include <cstddef>
#include <deque>
#include <iomanip>
#include <iostream>
#include <set>
#include <unordered_set>
#include <vector>

#define HSF_DEBUG
#include "hsf/frequency.h"
#include "hsf/recency.h"

#include "benchmark/treap.h"
#include "benchmark/skiplist.h"
#include "benchmark/zipf.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

template <size_t* Counter>
struct counting_comparator {
    bool operator()(int left, int right) const {
        ++(*Counter);
        return left < right;
    }
};

using key_t = int;

static size_t rbtree_comparisons = 0;
using rbtree_comparator = counting_comparator<&rbtree_comparisons>;
using rbtree = std::set<key_t, rbtree_comparator>;

static size_t fforest_comparisons = 0;
using fforest_comparator = counting_comparator<&fforest_comparisons>;
using fforest = hsf::frequency_forest<hsf::capacity, std::map, key_t, fforest_comparator>;

static size_t learned_fforest_comparisons = 0;
using learned_fforest_comparator = counting_comparator<&learned_fforest_comparisons>;
using learned_fforest = hsf::learned_frequency_forest<hsf::capacity, std::map, key_t, learned_fforest_comparator>;

static size_t rforest_comparisons = 0;
using rforest_comparator = counting_comparator<&rforest_comparisons>;
using rforest = hsf::recency_forest<hsf::capacity, std::map, key_t, rforest_comparator>;

static size_t learned_rforest_comparisons = 0;
using learned_rforest_comparator = counting_comparator<&learned_rforest_comparisons>;
using learned_rforest = hsf::learned_recency_forest<hsf::capacity, std::map, key_t, learned_rforest_comparator>;

static size_t learned_treap_comparisons = 0;
using learned_treap_comparator = counting_comparator<&learned_treap_comparisons>;
using learned_treap = hsf::bench::treap<key_t, learned_treap_comparator>;

static size_t robustsl_comparisons = 0;
using robustsl_comparator = counting_comparator<&robustsl_comparisons>;
using robustsl = hsf::bench::skiplist<key_t, robustsl_comparator>;

// template <typename Gen>
auto generate_zipf_queries(size_t num_keys, size_t num_queries, double alpha, std::default_random_engine& gen) {
    zipfian_int_distribution<key_t> zipf(0, num_keys - 1, alpha);
    
    std::vector<size_t> perm(num_keys);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), gen);

    std::vector<key_t> queries(num_queries);
    for (size_t i = 0; i < num_queries; i++) {
        queries[i] = perm[zipf(gen)];
    }

    return queries;
}

auto compute_accesses(const std::vector<key_t>& queries, size_t num_keys) {
    std::vector<std::deque<size_t>> accesses(num_keys);
    
    size_t distinct_accesses = 0;
    size_t prev_query = 0;
    for (const auto& key : queries) {
        auto& queue = accesses[key];
        size_t prev_access = queue.empty() ? 0 : queue.back();
        queue.push_back(distinct_accesses - prev_access);

        if (key != prev_query) {
            distinct_accesses++;
        }
        prev_query = key;
    }

    return accesses;
}

auto compute_ranks(const std::vector<std::deque<size_t>>& accesses) {
    std::vector<size_t> ranks(accesses.size());
    std::vector<std::pair<size_t, size_t>> indexed_frequencies(accesses.size());
    for (size_t i = 0; i < accesses.size(); i++) {
        indexed_frequencies[i] = std::make_pair(i, accesses[i].size());
    }

    std::sort(indexed_frequencies.begin(), indexed_frequencies.end(), [](auto& left, auto& right) {
        if (left.second != right.second) {
            return left.second > right.second;
        }
        return left.first < right.first;
    });

    for (size_t i = 0; i < accesses.size(); ++i) {
        ranks[indexed_frequencies[i].first] = i;
    }

    return ranks;
}

void reset_comparisons() {
    fforest_comparisons = 0;
    learned_fforest_comparisons = 0;
    rforest_comparisons = 0;
    learned_rforest_comparisons = 0;
    rbtree_comparisons = 0;
    robustsl_comparisons = 0;
    learned_treap_comparisons = 0;
}

void print_comparisons(size_t num_ops) {
    double fforest_avg = double(fforest_comparisons) / num_ops;
    double learned_fforest_avg = double(learned_fforest_comparisons) / num_ops;
    double rforest_avg = double(rforest_comparisons) / num_ops;
    double learned_rforest_avg = double(learned_rforest_comparisons) / num_ops;
    double rbtree_avg = double(rbtree_comparisons) / num_ops;
    double robustsl_avg = double(robustsl_comparisons) / num_ops;
    double learned_treap_avg = double(learned_treap_comparisons) / num_ops;

    std::cout << "  red-black tree:     " << std::fixed << std::setprecision(2) << rbtree_avg << "\n";
    std::cout << "  f-forest:           " << std::fixed << std::setprecision(2) << fforest_avg << "\n";
    std::cout << "  learned f-forest:   " << std::fixed << std::setprecision(2) << learned_fforest_avg << "\n";
    std::cout << "  r-forest:           " << std::fixed << std::setprecision(2) << rforest_avg << "\n";
    std::cout << "  learned r-forest:   " << std::fixed << std::setprecision(2) << learned_rforest_avg << "\n";
    std::cout << "  robust-sl:          " << std::fixed << std::setprecision(2) << robustsl_avg << "\n";
    std::cout << "  learned treap:      " << std::fixed << std::setprecision(2) << learned_treap_avg << "\n";
}

py::dict benchmark(const std::vector<key_t>& queries, size_t num_keys, std::default_random_engine& gen) {
    auto accesses = compute_accesses(queries, num_keys);
    auto ranks = compute_ranks(accesses);
    auto levels = hsf::bench::skiplist_levels(accesses, num_keys, queries.size(), gen);

    fforest ff(hsf::capacity(1.0, 2.0), hsf::capacity(1.0, 2.0));
    learned_fforest lff(hsf::capacity(1.0, 1.1), hsf::capacity(1.1, 1.1));
    rforest rf(hsf::capacity(1.0, 2.0), hsf::capacity(1.0, 2.0));
    learned_rforest lrf(hsf::capacity(1.0, 1.1), hsf::capacity(1.1, 1.1));
    rbtree rb;
    robustsl sl;
    learned_treap treap;

    reset_comparisons();
    for (key_t key = 0; key < num_keys; key++) {
        ff.insert(key);
        lff.insert(key, ranks[key]);
        rf.insert(key);
        lrf.insert(key, accesses[key].empty() ? -1 : accesses[key].front());
        rb.insert(key);
        sl.insert(key, levels[key]);
        treap.insert(key, ranks[key]);
    }

    std::cout << "\n================== INSERTS ==================\n";
    std::cout << "Total: " << num_keys << "\n";
    std::cout << "Comparisons: \n";
    print_comparisons(num_keys);

    py::dict insert_stats;
    py::dict insert_stats_comparisons;
    py::dict insert_stats_compactions;
    py::dict insert_stats_mispredictions;
    py::dict insert_stats_promotions;
    
    insert_stats_comparisons["rbtree"] = double(rbtree_comparisons) / num_keys;
    insert_stats_comparisons["f-forest"] = double(fforest_comparisons) / num_keys;
    insert_stats_comparisons["learned-f-forest"] = double(learned_fforest_comparisons) / num_keys;
    insert_stats_comparisons["r-forest"] = double(rforest_comparisons) / num_keys;
    insert_stats_comparisons["learned-r-forest"] = double(learned_rforest_comparisons) / num_keys;
    insert_stats_comparisons["robust-sl"] = double(robustsl_comparisons) / num_keys;
    insert_stats_comparisons["learned-treap"] = double(learned_treap_comparisons) / num_keys;
    
    insert_stats_compactions["f-forest"] = ff.compactions_;
    insert_stats_compactions["learned-f-forest"] = lff.compactions_;
    insert_stats_compactions["r-forest"] = rf.compactions_;
    insert_stats_compactions["learned-r-forest"] = lrf.compactions_;

    insert_stats_mispredictions["f-forest"] = ff.mispredictions_;
    insert_stats_mispredictions["learned-f-forest"] = lff.mispredictions_;
    insert_stats_mispredictions["r-forest"] = rf.mispredictions_;
    insert_stats_mispredictions["learned-r-forest"] = lrf.mispredictions_;

    insert_stats_promotions["f-forest"] = ff.promotions_;
    insert_stats_promotions["learned-f-forest"] = lff.promotions_;
    insert_stats_promotions["r-forest"] = rf.promotions_;
    insert_stats_promotions["learned-r-forest"] = lrf.promotions_;

    insert_stats["comparisons"] = insert_stats_comparisons;
    insert_stats["compactions"] = insert_stats_compactions;
    insert_stats["mispredictions"] = insert_stats_mispredictions;
    insert_stats["promotions"] = insert_stats_promotions;
    
    reset_comparisons();
    for (const auto& key : queries) {
        auto it1 = ff.find(key);
        assert(it1 != ff.end() && it1->first == key);

        auto it2 = lff.find(key, ranks[key]);
        assert(it2 != lff.end() && it2->first == key);

        auto it3 = rf.find(key);
        assert(it3 != rf.end() && it3->first == key);

        size_t prev_access = accesses[key].front();
        accesses[key].pop_front();
        size_t next_access = accesses[key].empty() ? -1 : accesses[key].front();
        auto it4 = lrf.find(key, prev_access, next_access);

        auto it5 = rb.find(key);
        assert(it5 != rb.end() && *it5 == key);

        auto it6 = sl.find(key);
        assert(it6 != sl.end());

        auto it7 = treap.find(key);
        assert(it7 != nullptr && it7->key == key);
    }

    std::cout << "\n================== QUERIES ==================\n";
    std::cout << "Total: " << num_keys << "\n";
    std::cout << "Comparisons: \n";
    print_comparisons(queries.size());

    py::dict query_stats;
    py::dict query_stats_comparisons;
    py::dict query_stats_compactions;
    py::dict query_stats_mispredictions;
    py::dict query_stats_promotions;

    query_stats_comparisons["rbtree"] = double(rbtree_comparisons) / queries.size();
    query_stats_comparisons["f-forest"] = double(fforest_comparisons) / queries.size();
    query_stats_comparisons["learned-f-forest"] = double(learned_fforest_comparisons) / queries.size();
    query_stats_comparisons["r-forest"] = double(rforest_comparisons) / queries.size();
    query_stats_comparisons["learned-r-forest"] = double(learned_rforest_comparisons) / queries.size();
    query_stats_comparisons["robust-sl"] = double(robustsl_comparisons) / queries.size();
    query_stats_comparisons["learned-treap"] = double(learned_treap_comparisons) / queries.size();

    query_stats_compactions["f-forest"] = ff.compactions_;
    query_stats_compactions["learned-f-forest"] = lff.compactions_;
    query_stats_compactions["r-forest"] = rf.compactions_;
    query_stats_compactions["learned-r-forest"] = lrf.compactions_;

    query_stats_mispredictions["f-forest"] = ff.mispredictions_;
    query_stats_mispredictions["learned-f-forest"] = lff.mispredictions_;
    query_stats_mispredictions["r-forest"] = rf.mispredictions_;
    query_stats_mispredictions["learned-r-forest"] = lrf.mispredictions_;

    query_stats_promotions["f-forest"] = ff.promotions_;
    query_stats_promotions["learned-f-forest"] = lff.promotions_;
    query_stats_promotions["r-forest"] = rf.promotions_;
    query_stats_promotions["learned-r-forest"] = lrf.promotions_;

    query_stats["comparisons"] = query_stats_comparisons;
    query_stats["compactions"] = query_stats_compactions;
    query_stats["mispredictions"] = query_stats_mispredictions;
    query_stats["promotions"] = query_stats_promotions;

    py::dict res;
    res["inserts"] = insert_stats;
    res["queries"] = query_stats;

    return res;
}

PYBIND11_MODULE(benchmark_module, m) {
    m.doc() = "Benchmarking module for search-forests";

    py::class_<std::default_random_engine>(m, "RandomEngine")
        .def(py::init<unsigned long>(), py::arg("seed"))
        .def("__call__", [](std::default_random_engine &eng) {
            return eng();
        });

    m.def("generate_zipf_queries",
          &generate_zipf_queries,
          "generate_zipf_queries(num_keys: int, num_queries: int, alpha: float, gen: PythonEngine) -> List[int]",
          py::arg("num_keys"), py::arg("num_queries"), py::arg("alpha"), py::arg("gen"));

    m.def("benchmark",
          &benchmark,
          "benchmark(queries: List[int], num_keys: int, gen: PythonEngine) -> Dict[str, Dict[str, float]]",
          py::arg("queries"), py::arg("num_keys"), py::arg("gen"));
}

// int main() {
//     constexpr size_t NUM_KEYS = 1'000'000;
//     constexpr size_t NUM_QUERIES = 1'000'000;
//     constexpr double ZIPF_ALPHA = 2.0;
    
//     std::default_random_engine gen(42);
//     auto queries = generate_zipf_queries(NUM_KEYS, NUM_QUERIES, ZIPF_ALPHA, gen);

//     benchmark(queries, NUM_KEYS, gen);
// }