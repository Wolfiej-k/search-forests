#ifndef HSF_TREAP_H
#define HSF_TREAP_H

#include <algorithm>
#include <cstdint>
#include <deque>

namespace hsf {

namespace bench {

template <typename Key, typename Compare, typename Priority = uint32_t>
struct treap {
    struct treap_node {
        Key key;
        size_t size;
        Priority priority;
        treap_node* left_child;
        treap_node* right_child;

        treap_node(const Key& key, Priority p)
            : key(key), size(1), priority(p), left_child(nullptr), right_child(nullptr) {}

        void update() {
            size = 1;
            if (left_child)  size += left_child->size;
            if (right_child) size += right_child->size;
        }
    };
    
    std::deque<treap_node> nodes;
    treap_node* root = nullptr;
    [[no_unique_address]] Compare comp;
    
    std::pair<treap_node*, treap_node*> split(Key key, treap_node* cur) {
        if(cur == nullptr) {
            return {nullptr, nullptr};
        }
        
        std::pair<treap_node*, treap_node*> result;
        if (!comp(cur->key, key)) {
            auto ret = split(key, cur->left_child);
            cur->left_child = ret.second;
            result = {ret.first, cur};
        } else {
            auto ret = split(key, cur->right_child);
            cur->right_child = ret.first;
            result = {cur, ret.second};
        }
        cur->update();
        return result;
    }
    
    treap_node* merge(treap_node* left, treap_node* right) {
        if (!left)  return right;
        if (!right) return left;

        treap_node* top;
        if (left->priority < right->priority) {
            left->right_child = merge(left->right_child, right);
            top = left;
        } else {
            right->left_child = merge(left, right->left_child);
            top = right;
        }
        top->update();
        return top;
    }
    
    void insert(const Key& key, Priority priority) {
        nodes.emplace_back(key, priority);
        auto parts = split(key, root);
        root = merge(merge(parts.first, &nodes.back()), parts.second);
    }
    
    void erase(const Key& key) {
        auto [left, mid] = split(key, root);
        auto [_, right] = split(key, mid);
        root = merge(left, right);
    }

    treap_node* find(const Key& key) {
        treap_node* cur = root;
        while (cur) {
            if (comp(key, cur->key)) {
                cur = cur->left_child;
            } else if (!comp(cur->key, key)) {
                return cur;
            } else {
                cur = cur->right_child;
            }
        }
        return nullptr;
    }
};

}

}

#endif