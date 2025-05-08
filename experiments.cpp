#include <cassert>
#include <cstddef>
#include <random>
#include <set>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#define HSF_DEBUG
#include "hsf/frequency.h"
#include "hsf/recency.h"

#include "benchmark/treap.h"
#include "benchmark/skiplist.h"
#include "benchmark/benchmark.h"

namespace py = pybind11;

template <size_t* Counter>
struct counting_comparator {
    bool operator()(int left, int right) const {
        ++(*Counter);
        return left < right;
    }
};

static size_t f_forest_comparisons = 0;
using f_forest_comparator = counting_comparator<&f_forest_comparisons>;
using f_forest = hsf::frequency_forest<hsf::capacity, std::map, int, f_forest_comparator>;

static size_t learned_f_forest_comparisons = 0;
using learned_f_forest_comparator = counting_comparator<&learned_f_forest_comparisons>;
using learned_f_forest = hsf::learned_frequency_forest<hsf::capacity, std::map, int, learned_f_forest_comparator>;

static size_t r_forest_comparisons = 0;
using r_forest_comparator = counting_comparator<&r_forest_comparisons>;
using r_forest = hsf::recency_forest<hsf::capacity, std::map, int, r_forest_comparator>;

static size_t learned_r_forest_comparisons = 0;
using learned_r_forest_comparator = counting_comparator<&learned_r_forest_comparisons>;
using learned_r_forest = hsf::learned_recency_forest<hsf::capacity, std::map, int, learned_r_forest_comparator>;

static size_t learned_treap_comparisons = 0;
using learned_treap_comparator = counting_comparator<&learned_treap_comparisons>;
using learned_treap = hsf::bench::treap<int, learned_treap_comparator>;

static size_t robustsl_comparisons = 0;
using robustsl_comparator = counting_comparator<&robustsl_comparisons>;
using robustsl = hsf::bench::skiplist<int, robustsl_comparator>;

static size_t rb_tree_comparisons = 0;
using rb_tree_comparator = counting_comparator<&rb_tree_comparisons>;
using rb_tree = std::set<int, rb_tree_comparator>;

void reset_comparisons() {
    rb_tree_comparisons = 0;
    f_forest_comparisons = 0;
    learned_f_forest_comparisons = 0;
    r_forest_comparisons = 0;
    learned_r_forest_comparisons = 0;
    learned_treap_comparisons = 0;
    robustsl_comparisons = 0;
}

template <typename Gen>
py::dict benchmark(const std::vector<int>& queries, const std::vector<size_t>& frequencies, std::vector<std::deque<size_t>>& accesses, Gen& gen) {
    auto ranks = hsf::bench::compute_ranks(frequencies);
    auto levels = hsf::bench::skiplist_levels(frequencies, queries.size(), gen);
    size_t num_keys = frequencies.size();
    size_t num_queries = queries.size();

    f_forest ff(hsf::capacity(1.0, 2.0), hsf::capacity(1.0, 2.0));
    learned_f_forest lff(hsf::capacity(1.0, 1.1), hsf::capacity(1.1, 1.1));
    r_forest rf(hsf::capacity(1.0, 2.0), hsf::capacity(1.0, 2.0));
    learned_r_forest lrf(hsf::capacity(1.0, 1.1), hsf::capacity(1.1, 1.1));
    learned_treap lt;
    robustsl rsl;
    rb_tree rb;

    reset_comparisons();
    for (int key = 0; key < num_keys; key++) {
        ff.insert(key);
        lff.insert(key, ranks[key]);
        rf.insert(key);
        lrf.insert(key, accesses[key].empty() ? -1 : accesses[key].front());
        lt.insert(key, ranks[key]);
        rsl.insert(key, levels[key]);
        rb.insert(key);
    }

    py::dict insert_stats;
    py::dict insert_stats_comparisons;
    py::dict insert_stats_compactions;
    py::dict insert_stats_mispredictions;
    py::dict insert_stats_promotions;

    insert_stats_comparisons["f_forest"] = double(f_forest_comparisons) / num_keys;
    insert_stats_comparisons["learned_f_forest"] = double(learned_f_forest_comparisons) / num_keys;
    insert_stats_comparisons["r_forest"] = double(r_forest_comparisons) / num_keys;
    insert_stats_comparisons["learned_r_forest"] = double(learned_r_forest_comparisons) / num_keys;
    insert_stats_comparisons["learned_treap"] = double(learned_treap_comparisons) / num_keys;
    insert_stats_comparisons["robustsl"] = double(robustsl_comparisons) / num_keys;
    insert_stats_comparisons["rb_tree"] = double(rb_tree_comparisons) / num_keys;

    insert_stats_compactions["f_forest"] = ff.compactions_;
    insert_stats_compactions["learned_f_forest"] = lff.compactions_;
    insert_stats_compactions["r_forest"] = rf.compactions_;
    insert_stats_compactions["learned_r_forest"] = lrf.compactions_;

    insert_stats_mispredictions["f_forest"] = ff.mispredictions_;
    insert_stats_mispredictions["learned_f_forest"] = lff.mispredictions_;
    insert_stats_mispredictions["r_forest"] = rf.mispredictions_;
    insert_stats_mispredictions["learned_r_forest"] = lrf.mispredictions_;

    insert_stats_promotions["f_forest"] = ff.promotions_;
    insert_stats_promotions["learned_f_forest"] = lff.promotions_;
    insert_stats_promotions["r_forest"] = rf.promotions_;
    insert_stats_promotions["learned_r_forest"] = lrf.promotions_;

    insert_stats["comparisons"] = insert_stats_comparisons;
    insert_stats["compactions"] = insert_stats_compactions;
    insert_stats["mispredictions"] = insert_stats_mispredictions;
    insert_stats["promotions"] = insert_stats_promotions;

    reset_comparisons();
    for (const auto& query : queries) {
        auto it1 = ff.find(query);
        assert(it1 != ff.end() && it1->first == query);

        auto it2 = lff.find(query, ranks[query]);
        assert(it2 != lff.end() && it2->first == query);

        auto it3 = rf.find(query);
        assert(it3 != rf.end() && it3->first == query);

        size_t prev_access = accesses[query].front();
        accesses[query].pop_front();
        size_t next_access = accesses[query].empty() ? -1 : accesses[query].front();
        auto it4 = lrf.find(query, prev_access, next_access);

        auto it5 = lt.find(query);
        assert(it5 != nullptr && it5->key == query);

        auto it6 = rsl.find(query);
        assert(it6 != rsl.end());

        auto it7 = rb.find(query);
        assert(it7 != rb.end() && *it7 == query);
    }

    py::dict query_stats;
    py::dict query_stats_comparisons;
    py::dict query_stats_compactions;
    py::dict query_stats_mispredictions;
    py::dict query_stats_promotions;

    query_stats_comparisons["f_forest"] = double(f_forest_comparisons) / num_queries;
    query_stats_comparisons["learned_f_forest"] = double(learned_f_forest_comparisons) / num_queries;
    query_stats_comparisons["r_forest"] = double(r_forest_comparisons) / num_queries;
    query_stats_comparisons["learned_r_forest"] = double(learned_r_forest_comparisons) / num_queries;
    query_stats_comparisons["learned_treap"] = double(learned_treap_comparisons) / num_queries;
    query_stats_comparisons["robustsl"] = double(robustsl_comparisons) / num_queries;
    query_stats_comparisons["rb_tree"] = double(rb_tree_comparisons) / num_queries;

    query_stats_compactions["f_forest"] = ff.compactions_;
    query_stats_compactions["learned_f_forest"] = lff.compactions_;
    query_stats_compactions["r_forest"] = rf.compactions_;
    query_stats_compactions["learned_r_forest"] = lrf.compactions_;

    query_stats_mispredictions["f_forest"] = ff.mispredictions_;
    query_stats_mispredictions["learned_f_forest"] = lff.mispredictions_;
    query_stats_mispredictions["r_forest"] = rf.mispredictions_;
    query_stats_mispredictions["learned_r_forest"] = lrf.mispredictions_;

    query_stats_promotions["f_forest"] = ff.promotions_;
    query_stats_promotions["learned_f_forest"] = lff.promotions_;
    query_stats_promotions["r_forest"] = rf.promotions_;
    query_stats_promotions["learned_r_forest"] = lrf.promotions_;

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
    m.doc() = "Benchmarking module for search forests";

    py::class_<std::mt19937>(m, "RandomEngine")
        .def(py::init<unsigned long>(), py::arg("seed"))
        .def("__call__", [](std::mt19937& gen) {
            return gen();
        });

    m.def("generate_zipf_queries",
          &hsf::bench::generate_zipf_queries<int, std::mt19937>,
          "generate_zipf_queries(num_keys: int, num_queries: int, alpha: float, gen: RandomEngine) -> List[int]",
          py::arg("num_keys"), py::arg("num_queries"), py::arg("alpha"), py::arg("gen"));
    
    m.def("generate_noisy_frequencies",
          &hsf::bench::generate_noisy_frequencies<int, std::mt19937>,
          "generate_noisy_frequencies(queries: List[int], num_keys: int, delta: int, gen: RandomEngine) -> List[int]",
          py::arg("queries"), py::arg("num_keys"), py::arg("delta"), py::arg("gen"));
    
    m.def("generate_noisy_accesses",
          &hsf::bench::generate_noisy_accesses<int, std::mt19937>,
          "generate_noisy_accesses(queries: List[int], num_keys: int, delta: int, gen: RandomEngine) -> List[List[int]]",
          py::arg("queries"), py::arg("num_keys"), py::arg("delta"), py::arg("gen"));

    m.def("benchmark",
          &benchmark<std::mt19937>,
          "benchmark(queries: List[int], frequencies: List[int], accesses: List[List[int]], gen: RandomEngine) -> Dict[str, Dict[str, float]]",
          py::arg("queries"), py::arg("frequencies"), py::arg("accesses"), py::arg("gen"));
}