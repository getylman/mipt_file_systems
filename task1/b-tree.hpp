#pragma once

#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace btree {

template <typename Key, typename Value, size_t N>
struct Node {
  // if values is nullptr it means it is leaf
  std::unique_ptr<std::deque<std::shared_ptr<Node<Key, Value, N>>>> children{
      nullptr};
  std::unique_ptr<std::deque<Key>> keys{nullptr};
  std::unique_ptr<std::deque<Value>> values{nullptr};

  std::shared_ptr<Node<Key, Value, N>> right{nullptr};
  std::mutex mtx;
};

template <typename Key, typename Value, size_t N>
class BLinkTree {
  using BNode = Node<Key, Value, N>;
  using BNodePtr = std::shared_ptr<BNode>;

 public:
  BLinkTree() = default;
  ~BLinkTree() = default;
  template <typename OStream>
  void serialize(OStream& out);

  template <typename IStream>
  static BLinkTree deserialize(const IStream& in);

  template <typename... Args>
  void emplace(Args&&... args);
  void insert(const std::pair<Key, Value>& value);
  void insert(std::pair<Key, Value>&& value);

  void erase(const Key& key);

  Value& at(const Key& key);
  const Value& at(const Key& key) const;
  bool contains(const Key& key) const;
  bool empty() const { return size_ == 0; }
  size_t size() const { return size_; }

  // needed for debug
  void GetValuesRec(const BNodePtr& node, std::vector<Key>& vec) const {
    if (node == nullptr) {
      return;
    }
    for (size_t i = 0; i < node->keys->size(); ++i) {
      if (node->children != nullptr && !node->children->empty()) {
        GetValuesRec((*(node->children))[i], vec);
      } else {
        vec.emplace_back((*(node->keys))[i]);
      }
    }
  }

  std::vector<Key> GetValues() const {
    std::vector<Key> v;
    GetValuesRec(root_, v);
    return v;
  }

 private:
  BNodePtr root_{nullptr};
  size_t size_{0};
};

template <typename Key, typename Value, size_t N>
BLinkTree<Key, Value, N> merge(BLinkTree<Key, Value, N>& left,
                               BLinkTree<Key, Value, N>& right);

}  // namespace btree
