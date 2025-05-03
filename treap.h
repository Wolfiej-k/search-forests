#include <deque>
#include <iomanip>

struct learned_treap {
    struct treap_node {
        int key;
        int size;
        unsigned int priority;
        
        treap_node* left_child;
        treap_node* right_child;
        
        treap_node(int new_key, int p) {
            key = new_key;
            size = 1;
            priority = p; 
            left_child = nullptr;
            right_child = nullptr;
        }
        void update() {
            size = 1;
            if(left_child) size += left_child->size;
            if(right_child) size += right_child->size;
        }
    };
    
    std::deque<treap_node> nodes;
    treap_node* root = nullptr;
    int ncomparisons = 0;
    
    std::pair<treap_node*, treap_node*> split(int key, treap_node* cur) {
        if(cur == nullptr) {
            return {nullptr, nullptr};
        }
        
        std::pair<treap_node*, treap_node*> result;
        if(key <= cur->key) {
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
        if(left == nullptr) return right;
        if(right == nullptr) return left;
        
        treap_node* top;
        if(left->priority < right->priority) {
            left->right_child = merge(left->right_child, right);
            top = left;
        } else {
            right->left_child = merge(left, right->left_child);
            top = right;
        }
        top->update();
        return top;
    }
    
    void insert(int key, int priority) {
        nodes.push_back(treap_node(key, priority));
        
        std::pair<treap_node*, treap_node*> ret = split(key, root);
        root = merge(merge(ret.first, &nodes.back()), ret.second);
    }
    
    void erase(int key) {
        treap_node *left, *mid, *right;
        
        std::tie(left, mid) = split(key, root);
        std::tie(mid, right) = split(key+1, mid);
        root = merge(left, right);
    }

    treap_node* find(int key) {
        treap_node* cur = root;
        while (cur) {
            ncomparisons++;
            if (cur->key == key) {
                return cur;
            } 
            else if (key < cur->key) {
                cur = cur->left_child;
            }
            else {
                cur = cur->right_child;
            }
        }
        return nullptr;
    }
};