#ifndef HSF_RECENCY_H
#define HSF_RECENCY_H

#include <algorithm>
#include <cassert>
#include <list>
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
class recency_forest : public search_forest<recency_forest<Capacity, Container, Key, Args...>> {
public:
    using parent_type = search_forest<recency_forest<Capacity, Container, Key, Args...>>; 
    using key_type = typename parent_type::key_type;
    using value_type = typename parent_type::value_type;
    using size_type = typename parent_type::size_type;
    using iterator = typename parent_type::iterator;

    template <typename... Params>
    explicit recency_forest(Params&&... params) 
        : parent_type(std::forward<Params>(params)...) {
        recencies_.emplace_back();
    }

    iterator find(const key_type& key, size_type hint = 0) {
        auto it = parent_type::find(key, hint);
        if (it == parent_type::end()) {
            return it;
        }

        size_type level = it.level();
        if (level > 0) {
            it = move_iterator(it, 0);
            compact_level(0);
            fill_level(level);
        }

        return it;
    }

    iterator insert(const key_type& key) {
        size_type level = parent_type::levels() - 1;
        recencies_[level].push_front(key);
        auto rec_it = recencies_[level].begin();
        auto it = parent_type::insert({key, rec_it}, level);
        compact_level(level);
        return it;
    }

private:
    std::vector<std::list<key_type>> recencies_;

    iterator move_key(const key_type& key, size_type from_level, size_type to_level) {
        auto from_it = parent_type::find(key, from_level);
        if (from_it == parent_type::end()) {
            return parent_type::end();
        }

        return move_iterator(from_it, to_level);
    }

    iterator move_iterator(iterator from_it, size_type to_level) {
        while (to_level >= recencies_.size()) {
            recencies_.emplace_back();
        }

        auto& from_list = recencies_[from_it.level()];
        auto& to_list = recencies_[to_level];
        to_list.splice(to_list.begin(), from_list, from_it->second);

        auto [key, rec_it] = *from_it;
        parent_type::erase(from_it);
        return parent_type::insert({key, rec_it}, to_level);
    }

    void compact_level(size_type level) {
        auto [min_cap, max_cap] = parent_type::capacity(level);
        size_type level_size = parent_type::size(level);

        if (level_size > max_cap) {
            while (level_size > min_cap) {
                assert(!recencies_[level].empty());
                auto min_key = *recencies_[level].rbegin();
                move_key(min_key, level, level + 1);
                level_size--;
            }
            
            compact_level(level + 1);
        }

        assert(recencies_[level].size() == parent_type::size(level));
    }

    void fill_level(size_type level) {
        auto [min_cap, _] = parent_type::capacity(level);
        size_type level_size = parent_type::size(level);

        if (level == 0 || level == parent_type::levels() - 1 || level_size >= min_cap) {
            return;
        }

        assert(!recencies_[level - 1].empty());
        
        auto min_key = *recencies_[level - 1].rbegin();
        move_key(min_key, level - 1, level);
        fill_level(level - 1);
    }
};

template <
    typename Capacity,
    template <typename, typename, typename...> class Container,
    typename Key,
    typename... Args
>
struct forest_traits<recency_forest<Capacity, Container, Key, Args...>> {
    using metadata_type = typename std::list<Key>::iterator;
    using level_type = Container<Key, metadata_type, Args...>;
    using capacity_type = Capacity;
};

template <
    typename Capacity,
    template <typename, typename, typename...> class Container,
    typename Key,
    typename... Args
>
class learned_recency_forest : public search_forest<learned_recency_forest<Capacity, Container, Key, Args...>> {
public:
    using parent_type = search_forest<learned_recency_forest<Capacity, Container, Key, Args...>>; 
    using key_type = typename parent_type::key_type;
    using value_type = typename parent_type::value_type;
    using size_type = typename parent_type::size_type;
    using iterator = typename parent_type::iterator;
    using parent_type::parent_type;

    iterator find(const key_type& key, size_type prev_access, size_type next_access = -1) {
        size_type prev_level = prediction_to_level(prev_access, parent_type::min_capacity_);
        auto it = parent_type::find(key, prev_level);
        if (it == parent_type::end()) {
            return it;
        }

        size_type level = it.level();
        size_type next_level = next_access == -1 
            ? parent_type::levels() - 1
            : prediction_to_level(next_access, parent_type::min_capacity_);

        it->second = next_access;
        if (level != next_level) {
            it = move_iterator(it, next_level);
            compact_level(next_level);
        }

        return it;
    }

    iterator insert(const key_type& key, size_type next_access = -1) {
        size_type level = next_access == -1 
            ? parent_type::levels() - 1
            : prediction_to_level(next_access, parent_type::min_capacity_);

        auto it = parent_type::insert({key, next_access}, level);
        compact_level(level);
        return it;
    }

private:
    struct heap_element {
        key_type key;
        uint32_t next_access;

        bool operator<(const heap_element& other) const {
            return next_access > other.next_access;
        }
    };

    iterator move_iterator(iterator from_it, size_type to_level) {
        auto [key, next_access] = *from_it;
        parent_type::erase(from_it);
        return parent_type::insert({key, next_access}, to_level);
    }

    void compact_level(size_type level) {
        auto [min_cap, max_cap] = parent_type::capacity(level);
        size_type level_size = parent_type::size(level);

        if (level_size > max_cap) {
            std::priority_queue<heap_element> max_accesses;
            for (const auto& [key, next_access] : parent_type::levels_[level]) {
                if (max_accesses.size() < level_size - min_cap) {
                    max_accesses.push({key, next_access});
                } else if (next_access > max_accesses.top().next_access) {
                    max_accesses.pop();
                    max_accesses.push({key, next_access});
                }
            }
            
            while (!max_accesses.empty()) {
                auto element = max_accesses.top();
                max_accesses.pop();

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
struct forest_traits<learned_recency_forest<Capacity, Container, Key, Args...>> {
    using metadata_type = uint32_t;
    using level_type = Container<Key, metadata_type, Args...>;
    using capacity_type = Capacity;
};

}

#endif