#ifndef HSF_RECENCY_H
#define HSF_RECENCY_H

#include <list>
#include <vector>

#include "hsf.h"

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
        while (to_level >= frequencies_.size()) {
            frequencies_.emplace_back();
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

        if (level == 0 || level_size > min_cap) {
            return;
        }

        assert(!recencies_[level - 1].empty());
        
        auto min_key = *recencies_[level - 1].rbegin();
        move_key(min_key, level - 1, level);
        fill_level(level - 1);
    }
};

}

#endif