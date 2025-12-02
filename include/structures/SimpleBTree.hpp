#pragma once
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace structures {

// B+树节点定义
template <typename KeyType, typename ValueType, int ORDER>
struct BTreeNode {
    bool is_leaf;
    int count; // 当前节点包含的关键字数量
    KeyType keys[ORDER]; // 关键字数组
    
    // 联合体：内部节点存子节点指针，叶子节点存具体的值
    union {
        BTreeNode* children[ORDER + 1];
        ValueType values[ORDER];
    };

    //初始化构建节点
    BTreeNode(bool leaf) : is_leaf(leaf), count(0) {
        std::memset(children, 0, sizeof(children));
    }
};

// B+ 树类
template <typename KeyType, typename ValueType, int ORDER = 32>
class SimpleBTree {
public:
    using Node = BTreeNode<KeyType, ValueType, ORDER>;

    Node* root;

    SimpleBTree() : root(nullptr) {} //创建一棵空树

    // 递归删除防止内存泄漏
    ~SimpleBTree() {
        if (root) delete_tree(root);
    } 

    //insert
    void insert(KeyType key, ValueType value) {\
        // 如果树是空的，创建根节点
        if (!root) {
            root = new Node(true);
            root->keys[0] = key;
            root->values[0] = value;
            root->count = 1;
            return;
        }

        // 如果根节点满了，需要向上分裂
        if (root->count == ORDER) {
            Node* new_root = new Node(false);
            new_root->children[0] = root;
            split_child(new_root, 0, root);
            root = new_root;
            insert_non_full(root, key, value);
        } 
        // 根节点未满，直接插入
        else {
            insert_non_full(root, key, value);
        }
    }

    // lookup
    // 返回 true 表示找到，val 写入结果
    bool search(KeyType key, ValueType& val) {
        if (!root) return false;
        return search_internal(root, key, val);
    }

private:
    // 递归删除辅助函数
    void delete_tree(Node* node) {
        if (!node->is_leaf) {
            // 递归删除子节点
            for (int i = 0; i <= node->count; ++i) {
                if (node->children[i]) delete_tree(node->children[i]);
            }
        }
        delete node;
    }

    // 内部查询逻辑
    bool search_internal(Node* node, KeyType key, ValueType& val) {
        // 在当前节点内进行二分查找
        // std::lower_bound 返回第一个 >= key 的位置
        int idx = std::lower_bound(node->keys, node->keys + node->count, key) - node->keys;

        if (node->is_leaf) {
            // 叶子节点：检查 key 是否相等
            if (idx < node->count && node->keys[idx] == key) {
                val = node->values[idx]; // 找到，返回值
                return true;
            }
            return false;
        } else {
            // 内部节点：决定去哪个子节点
            if (idx < node->count && node->keys[idx] == key) {
                idx++;
            }
            // 递归搜索子节点
            return search_internal(node->children[idx], key, val);
        }
    }

    // 分裂子节点
    void split_child(Node* parent, int index, Node* child) {
        Node* new_child = new Node(child->is_leaf);// 创建分裂出的新节点
        int mid = ORDER / 2; // 中间位置
        new_child->count = child->count - mid - 1; // 新节点关键字数量
        
        // 1. 搬运 Keys 到新节点
        for (int j = 0; j < mid; j++) {
            new_child->keys[j] = child->keys[j + mid + 1];
        }
        
        // 2. 搬运 Children/Values
        if (!child->is_leaf) {
            for (int j = 0; j <= mid; j++) {
                new_child->children[j] = child->children[j + mid + 1];
            }
        } else {
             for (int j = 0; j <= mid; j++) {
                new_child->values[j] = child->values[j + mid + 1]; // 叶子节点全搬
            }
        }
        
        child->count = mid; // 调整旧节点大小

        // 3. 调整父节点 children
        for (int j = parent->count; j >= index + 1; j--) {
            parent->children[j + 1] = parent->children[j];
        }
        parent->children[index + 1] = new_child;

        // 4. 调整父节点 keys
        for (int j = parent->count - 1; j >= index; j--) {
            parent->keys[j + 1] = parent->keys[j];
        } // 腾出一个位置
        parent->keys[index] = child->keys[mid];
        parent->count++;
    }

    void insert_non_full(Node* node, KeyType key, ValueType value) {
        int i = node->count - 1;
        if (node->is_leaf) {
            // 把比key大的都后移
            while (i >= 0 && key < node->keys[i]) {
                node->keys[i + 1] = node->keys[i];
                node->values[i + 1] = node->values[i];
                i--;
            }
            node->keys[i + 1] = key;
            node->values[i + 1] = value;
            node->count++;
        } else {
            // 找子节点
            while (i >= 0 && key < node->keys[i]) i--; // 找到第一个小于等于 key 的位置
            i++;
            
            // 检查子节点是否已满
            if (node->children[i]->count == ORDER) {
                split_child(node, i, node->children[i]);
                if (key > node->keys[i]) i++; 
            }
            insert_non_full(node->children[i], key, value);
        }
    }
};

} // namespace structures