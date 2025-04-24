#ifndef HSF_HSF_H
#define HSF_HSF_H

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace hsf {

using std::vector;

template <class Policy, class Level>
class search_forest {
public:
    using policy_type = Policy;
    using level_type = Level;
    using key_type = typename level_type::key_type;
    using value_type = typename level_type::value_type;
    using size_type = typename level_type::size_type;
    using level_iterator = typename level_type::iterator;
    using const_level_iterator = typename level_type::const_iterator;

    struct iterator {
        level_iterator iter_;
        size_type level_;

        value_type& operator*() { 
            return *iter_; 
        }
        
        value_type* operator->() { 
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
    

    search_forest(policy_type growth_policy, double fill_factor)
        : growth_policy_(growth_policy),
          fill_factor_(fill_factor),
          total_size_(0) {
        if (fill_factor_ < 0 || fill_factor_ >= 1) {
            throw std::invalid_argument("flush factor not in [0, 1)");
        }

        sizes_.emplace_back(growth_policy_(0));
        levels_.emplace_back();
    }

    size_type size() const {
        return total_size_;
    }
    
    size_type levels() const {
        return levels_.size();
    }

    void insert(const value_type& value, size_type level = 0) {
        while (level >= levels_.size()) {
            sizes_.emplace_back(growth_policy_(levels_.size()));
            levels_.emplace_back();
        }
        
        levels_[level].insert(value);
        total_size_++;
        
        size_type curr_levels = levels();
        for (size_type i = level; i < curr_levels; i++) {
            if (levels_[i].size() <= sizes_[i]) {
                return;
            }

            #ifdef HSF_DEBUG
            compactions_++;
            #endif

            if (i == curr_levels - 1) {
                sizes_.emplace_back(growth_policy_(curr_levels));
                levels_.emplace_back();
            }

            auto to_compact = std::min(size_type((1 - fill_factor_) * sizes_[i]), sizes_[i]);
            auto hint = levels_[i + 1].begin();
            for (size_t j = 0; j < to_compact; j++) {
                auto it = levels_[i].begin();
                hint = levels_[i + 1].insert(hint, *it);
                levels_[i].erase(it);
            }
        }
    }
    
    iterator find(const key_type& key, size_type hint = 0) {
        auto [it, level] = find_impl(levels_, key, hint);
        return iterator{it, level};
    }
    
    
    const_iterator find(const key_type& key, size_type hint = 0) const {
        auto [it, level] = find_impl(levels_, key, hint);
        return const_iterator{it, level};
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
    vector<level_type> levels_;
    vector<size_type> sizes_;
    [[no_unique_address]] policy_type growth_policy_;
    size_type total_size_;
    double fill_factor_;

    template <typename T>
    auto find_impl(T& levels, const key_type& key, size_type hint) const {
        using iterator_type = decltype(levels[0].find(key));
        ptrdiff_t i = hint, j = hint - 1;
        while (i < levels.size() || j >= 0) {
            if (i < levels.size()) {
                auto it = levels[i].find(key);
                if (it != levels[i].end()) {
                    #ifdef HSF_DEBUG
                    if (i != hint) mispredictions_++;
                    #endif
                    return std::make_pair(it, size_type(i));
                }
                i++;
            }

            if (j >= 0) {
                auto it = levels[j].find(key);
                if (it != levels[j].end()) {
                    #ifdef HSF_DEBUG
                    mispredictions_++;
                    #endif
                    return std::make_pair(it, size_type(j));
                }
                j--;
            }
        }

        #ifdef HSF_DEBUG
        mispredictions_++;
        #endif

        return std::make_pair(iterator_type{}, size_type(-1));
    }
};

}

#endif