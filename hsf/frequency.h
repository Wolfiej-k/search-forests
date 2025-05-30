#ifndef HSF_FREQUENCY_H
#define HSF_FREQUENCY_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <queue>
#include <vector>

#include "hsf.h"
#include "prediction.h"

namespace hsf {

template <
    typename Capacity,
    template <typename, typename, typename...> class Container,
    typename Key,
    typename... Args
>
class frequency_forest : public search_forest<frequency_forest<Capacity, Container, Key, Args...>> {
public:
    using parent_type = search_forest<frequency_forest<Capacity, Container, Key, Args...>>; 
    using key_type = typename parent_type::key_type;
    using value_type = typename parent_type::value_type;
    using size_type = typename parent_type::size_type;
    using iterator = typename parent_type::iterator;
    
    template <typename... Params>
    explicit frequency_forest(Params&&... params) 
        : parent_type(std::forward<Params>(params)...) {
        frequencies_.emplace_back();
    }

    iterator find(const key_type& key, size_type hint = 0) {
        auto it = parent_type::find(key, hint);
        if (it == parent_type::end()) {
            return it;
        }

        size_type level = it.level();
        auto node = frequencies_[level].extract(it->second);
        node.key() = node.key() + 1;
        it->second = frequencies_[level].insert(std::move(node));

        auto new_frequency = it->second->first;
        size_type new_level = level;
        while (new_level > 0 && new_frequency > frequencies_[new_level - 1].begin()->first) {
            new_level--;
        }

        if (new_level != level) {
            it = move_iterator(it, new_level, new_frequency);
            compact_level(new_level);
            fill_level(level);
        }

        return it;
    }

    iterator insert(const key_type& key, size_type frequency = 0) {
        size_type level = parent_type::levels() - 1;
        while (level > 0 && frequency > 0 && frequency >= frequencies_[level - 1].begin()->first) {
            level--;
        }

        auto freq_it = frequencies_[level].insert({frequency, key});
        auto it = parent_type::insert({key, freq_it}, level);
        compact_level(level);
        return it;
    }

private:
    std::vector<std::multimap<uint32_t, key_type>> frequencies_;

    iterator move_key(const key_type& key, size_type from_level, size_type to_level, uint32_t frequency) {
        auto from_it = parent_type::find(key, from_level);
        if (from_it == parent_type::end()) {
            return parent_type::end();
        }

        return move_iterator(from_it, to_level, frequency);
    }

    iterator move_iterator(iterator from_it, size_type to_level, uint32_t frequency) {
        while (to_level >= frequencies_.size()) {
            frequencies_.emplace_back();
        }

        auto node = frequencies_[from_it.level()].extract(from_it->second);
        node.key() = frequency;
        auto freq_it = frequencies_[to_level].insert(std::move(node));

        auto key = from_it->first;
        parent_type::erase(from_it);
        return parent_type::insert({key, freq_it}, to_level);
    }

    void compact_level(size_type level) {
        auto [min_cap, max_cap] = parent_type::capacity(level);
        size_type level_size = parent_type::size(level);

        if (level_size > max_cap) {
            while (level_size > min_cap) {
                assert(!frequencies_[level].empty());
                auto [min_freq, min_key] = *frequencies_[level].begin();
                move_key(min_key, level, level + 1, min_freq);
                level_size--;
            }
            
            compact_level(level + 1);
        }
        
        assert(frequencies_[level].size() == parent_type::size(level));
    }
    
    void fill_level(size_type level) {
        auto [min_cap, _] = parent_type::capacity(level);
        size_type level_size = parent_type::size(level);

        if (level == 0 || level == parent_type::levels() - 1 || level_size >= min_cap) {
            return;
        }

        assert(!frequencies_[level - 1].empty());
        assert(frequencies_[level - 1].begin()->first >= frequencies_[level].rbegin()->first);
        
        auto [min_freq, min_key] = *frequencies_[level - 1].begin();
        move_key(min_key, level - 1, level, min_freq);
        fill_level(level - 1);
    }
};

template <
    typename Capacity,
    template <typename, typename, typename...> class Container,
    typename Key,
    typename... Args
>
struct forest_traits<frequency_forest<Capacity, Container, Key, Args...>> {
    using metadata_type = typename std::multimap<uint32_t, Key>::iterator;
    using level_type = Container<Key, metadata_type, Args...>;
    using capacity_type = Capacity;
};

template <
    typename Capacity,
    template <typename, typename, typename...> class Container,
    typename Key,
    typename... Args
>
class learned_frequency_forest : public search_forest<learned_frequency_forest<Capacity, Container, Key, Args...>> {
public:
    using parent_type = search_forest<learned_frequency_forest<Capacity, Container, Key, Args...>>; 
    using key_type = typename parent_type::key_type;
    using value_type = typename parent_type::value_type;
    using size_type = typename parent_type::size_type;
    using iterator = typename parent_type::iterator;
    using parent_type::parent_type;

    iterator find(const key_type& key, size_type rank) {
        size_type level = prediction_to_level(rank, parent_type::min_capacity_);
        return parent_type::find(key, level);
    }

    iterator insert(const key_type& key, size_type rank) {
        size_type level = prediction_to_level(rank, parent_type::min_capacity_);
        auto it = parent_type::insert({key, rank}, level);
        compact_level(level);
        return it;
    }

private:
    struct heap_element {
        key_type key;
        uint32_t rank;

        bool operator<(const heap_element& other) const {
            return rank > other.rank;
        }
    };

    iterator move_iterator(iterator from_it, size_type to_level) {
        auto [key, rank] = *from_it;
        parent_type::erase(from_it);
        return parent_type::insert({key, rank}, to_level);
    }

    void compact_level(size_type level) {
        auto [min_cap, max_cap] = parent_type::capacity(level);
        size_type level_size = parent_type::size(level);

        if (level_size > max_cap && level == parent_type::levels() - 1) {
            std::priority_queue<heap_element> max_ranks;
            for (const auto& [key, rank] : parent_type::levels_[level]) {
                if (max_ranks.size() < level_size - min_cap) {
                    max_ranks.push({key, rank});
                } else if (rank > max_ranks.top().rank) {
                    max_ranks.pop();
                    max_ranks.push({key, rank});
                }
            }

            while (!max_ranks.empty()) {
                auto element = max_ranks.top();
                max_ranks.pop();

                auto it = parent_type::find(element.key, level);
                assert(it != parent_type::end());
                move_iterator(it, level + 1);
            }

            compact_level(level + 1);
        }
    }
};

template <
    typename Capacity,
    template <typename, typename, typename...> class Container,
    typename Key,
    typename... Args
>
struct forest_traits<learned_frequency_forest<Capacity, Container, Key, Args...>> {
    using metadata_type = uint32_t;
    using level_type = Container<Key, metadata_type, Args...>;
    using capacity_type = Capacity;
};

}

#endif