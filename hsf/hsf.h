#ifndef HSF_HSF_H
#define HSF_HSF_H

#include <algorithm>
#include <vector>

namespace hsf {

template <typename Derived>
struct forest_traits;

template <typename Derived>
class search_forest {
public:
    using level_type = typename forest_traits<Derived>::level_type;
    using capacity_type = typename forest_traits<Derived>::capacity_type;
    using key_type = typename level_type::key_type;
    using value_type = typename level_type::value_type;
    using size_type = typename level_type::size_type;
    using level_iterator = typename level_type::iterator;

    struct iterator {
    public:
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

        size_type level() const {
            return level_;
        }

    private:
        friend class search_forest;
        level_iterator iter_;
        size_type level_;

        explicit iterator(level_iterator iter, size_type level)
            : iter_(iter), level_(level) {}
    };

    explicit search_forest(capacity_type min_capacity, capacity_type max_capacity)
        : min_capacity_(min_capacity), max_capacity_(max_capacity), total_size_(0) {
        levels_.emplace_back();
    }

    size_type size() const {
        return total_size_;
    }

    size_type size(size_type level) const {
        if (level >= levels()) {
            return 0;
        }
        return levels_[level].size();
    }

    std::pair<size_type, size_type> capacity(size_type level) const {
        return std::make_pair(min_capacity_(level), max_capacity_(level));
    }
    
    size_type levels() const {
        return levels_.size();
    }

    iterator find(const key_type& key, size_type hint) {
        for (size_type i = hint; i < levels_.size(); i++) {
            auto it = levels_[i].find(key);
            if (it != levels_[i].end()) {
#ifdef HSF_DEBUG
                if (i != hint) {
                    mispredictions_++;
                }
#endif
                return iterator(it, i);
            }
        }
        return end();
    }

    iterator insert(const value_type& value, size_type level) {
        while (level >= levels_.size()) {
            levels_.emplace_back();
        }
        
        auto it = levels_[level].insert(value).first;
#ifdef HSF_DEBUG
        if (levels_[level].size() > max_capacity_(level)) {
            compactions_++;
        }
#endif
        total_size_++;
        return iterator(it, level);
    }

    void erase(iterator it) {
        levels_[it.level_].erase(it.iter_);
#ifdef HSF_DEBUG
        if (levels_[it.level_].size() < min_capacity_(it.level_)) {
            promotions_++;
        }
#endif
        total_size_--;
    }

    iterator begin() const {
        for (size_type i = 0; i < levels(); i++) {
            if (!levels_[i].empty()) {
                return iterator(levels_[i].begin(), i);
            }
        }
        return end();
    }

    iterator end() const {
        return iterator({}, size_type(-1));
    }

#ifdef HSF_DEBUG
    mutable size_type compactions_ = 0;
    mutable size_type promotions_ = 0;
    mutable size_type mispredictions_ = 0;
#endif

protected:
    [[no_unique_address]] capacity_type min_capacity_;
    [[no_unique_address]] capacity_type max_capacity_;
    std::vector<level_type> levels_;
    size_type total_size_;
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