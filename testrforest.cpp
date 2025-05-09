#include <map>
#include <iostream>

#define HSF_DEBUG
#include "hsf/recency.h"

using learned_r_forest = hsf::learned_recency_forest<hsf::capacity, std::map, int, std::less<int>>;

int main() {
    learned_r_forest lrf(hsf::capacity(1.0, 1.1), hsf::capacity(2.0, 1.1));
    for (int i = 0; i < 1000000; i++) {
        lrf.insert(i, i);
    }

    for (int i = 0; i < 1000000; i++) {
        auto it = lrf.find(i, i);
        assert(it != lrf.end());
    }

    std::cout << "compactions: " << lrf.compactions_ << std::endl;

    return 0;
}