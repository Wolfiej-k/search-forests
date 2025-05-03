#ifndef HSF_HSF_H
#define HSF_HSF_H

#include <algorithm>
#include <vector>

namespace hsf {

template <class Level, class Capacity>
class search_forest {
public:
    using level_type = Level;
    using capacity_type = Capacity;
    using key_type = typename level_type::key_type;
    using value_type = typename level_type::value_type;
    using size_type = typename level_type::size_type;
    using level_iterator = typename level_type::iterator;
    using const_level_iterator = typename level_type::const_iterator;

    struct iterator {
        level_iterator iter_;
        size_type level_;

        const value_type& operator*() const { 
            return *iter_; 
        }
        
        const value_type* operator->() const { 
            return &(*iter_); 
        }

        bool operator==(const iterator& other) const {
            return level_ == other.level_ && iter_ == other.iter_;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    struct const_iterator {
        const_level_iterator iter_;
        size_type level_;

        const value_type& operator*() const { 
            return *iter_; 
        }
        
        const value_type* operator->() const { 
            return &(*iter_); 
        }

        bool operator==(const const_iterator& other) const {
            return level_ == other.level_ && iter_ == other.iter_;
        }

        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
    };
    

    search_forest(capacity_type min_capacity, capacity_type max_capacity)
        : min_capacity_(min_capacity), max_capacity_(max_capacity) {
        levels_.emplace_back();
    }

    size_type size() const {
        return total_size_;
    }

    size_type size(size_type level) const {
        return levels_[level].size();
    }

    std::pair<size_type, size_type> capacity(size_type level) const {
        return std::make_pair(min_capacity_(level), max_capacity_(level));
    }
    
    size_type levels() const {
        return levels_.size();
    }

    void insert(const value_type& value, size_type level = 0) {
        while (level >= levels_.size()) {
            levels_.emplace_back();
        }
        
        levels_[level].insert(value);
        total_size_++;
        
        size_type curr_levels = levels();
        for (size_type i = level; i < curr_levels; i++) {
            auto to_compact = levels_[i].size() - min_capacity_(i);
            if (levels_[i].size() <= max_capacity_(i) || to_compact <= 0) {
                return;
            }

            #ifdef HSF_DEBUG
            compactions_++;
            #endif

            if (i == curr_levels - 1) {
                levels_.emplace_back();
            }

            auto hint = levels_[i + 1].begin();
            for (size_t j = 0; j < to_compact; j++) {
                auto it = levels_[i].begin();
                hint = levels_[i + 1].insert(hint, value_type(*it));
                levels_[i].erase(it);
            }
        }
    }
    
    iterator find(const key_type& key, size_type hint = 0) {
        auto [it, level] = find_impl(levels_, key, hint);
        return { it, level };
    }
    
    const_iterator find(const key_type& key, size_type hint = 0) const {
        auto [it, level] = find_impl(levels_, key, hint);
        return { it, level };
    }

    iterator begin() {
        for (size_type i = 0; i < levels(); i++) {
            if (!levels_[i].empty()) {
                return { levels_[i].begin(), i };
            }
        }
        return end();
    }

    const_iterator begin() const {
        for (size_type i = 0; i < levels(); i++) {
            if (!levels_[i].empty()) {
                return { levels_[i].begin(), i };
            }
        }
        return end();
    }

    iterator end() {
        return { {}, size_type(-1) };
    }

    const_iterator end() const {
        return { {}, size_type(-1) };
    }

    bool erase(const key_type& key, size_type hint = 0) {
        auto it = find(key, hint);
        return erase(it);
    }

    bool erase(const iterator& it) {
        if (it != end()) {
            levels_[it.level_].erase(it.iter_);
            total_size_--;
            return true;
        }
        return false;
    }

    #ifdef HSF_DEBUG
    mutable size_type compactions_ = 0;
    mutable size_type mispredictions_ = 0;
    #endif

private:
    [[no_unique_address]] capacity_type min_capacity_;
    [[no_unique_address]] capacity_type max_capacity_;
    std::vector<level_type> levels_;
    size_type total_size_;

    template <typename T>
    auto find_impl(T& levels, const key_type& key, size_type hint) const {
        using iterator_type = decltype(levels[0].find(key));
        for (size_t i = hint; i < levels.size(); i++) {
            auto it = levels[i].find(key);
            if (it != levels[i].end()) {
                #ifdef HSF_DEBUG
                if (i != hint) mispredictions_++;
                #endif
                return std::make_pair(it, size_type(i));
            }
        }

        #ifdef HSF_DEBUG
        mispredictions_++;
        #endif

        return std::make_pair(iterator_type{}, size_type(-1));
    }
};

struct capacity {
    double base;
    double scale;

    constexpr capacity(double fill_factor, double base = 1.1, size_t top_size = 256) 
        : base(base), scale(top_size * fill_factor / base) {}

    constexpr size_t operator()(size_t level) const {
        return std::pow(base, std::pow(base, level)) * scale;
    }
};

}

#endif