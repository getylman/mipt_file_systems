#include "b-tree.hpp"

#include <algorithm>
#include <optional>
#include <stack>
#include <stdexcept>

namespace btree {

namespace {

template <typename Key>
typename std::deque<Key>::iterator KeyBinSearchInNode(const Key& key,
                                                      std::deque<Key>& scope) {
  return std::lower_bound(
      scope.begin(), scope.end(), key,
      [](const Key& x, const Key& y) { return std::less<Key>{}(x, y); });
}

template <typename Key>
size_t GetIndex(const Key& key, std::deque<Key>& scope) {
  return std::distance(scope.begin(), KeyBinSearchInNode(key, scope));
}

template <typename Key, typename Value, size_t N>
bool IsLeaf(const std::shared_ptr<Node<Key, Value, N>>& node) {
  return node->children == nullptr;
}

template <typename Key, typename Value, size_t N>
typename std::deque<Value>::iterator GetValueIter(
    const typename std::deque<Key>::iterator& iter,
    std::shared_ptr<Node<Key, Value, N>>& node) {
  const auto delta = std::distance(node->keys->begin(), iter);
  auto value_iter = node->values->begin();
  std::advance(value_iter, delta);
  return value_iter;
}

template <typename Key, typename Value, size_t N>
typename std::deque<std::shared_ptr<Node<Key, Value, N>>>::iterator
GetChildIter(const typename std::deque<Key>::iterator& iter,
             std::shared_ptr<Node<Key, Value, N>>& node) {
  const auto delta = std::distance(node->keys->begin(), iter);
  auto child_iter = node->children->begin();
  std::advance(child_iter, delta);
  return child_iter;
}

// dive to leaf by key
template <typename Key, typename Value, size_t N>
std::shared_ptr<Node<Key, Value, N>> GoToLeaf(
    const Key& key, const std::shared_ptr<Node<Key, Value, N>>& node) {
  auto cur_node = node;
  while (!IsLeaf(cur_node)) {
    auto iter = KeyBinSearchInNode(key, *(cur_node->keys));
    while (iter == cur_node->keys->end() && cur_node->right != nullptr) {
      cur_node = cur_node->right;
      iter = KeyBinSearchInNode(key, *(cur_node->keys));
    }
    const auto idx = std::distance(cur_node->keys->begin(), iter);
    if (iter == cur_node->keys->end() && cur_node->right == nullptr) {
      cur_node = (*(cur_node->children))[idx - 1];
    } else {
      cur_node = (*(cur_node->children))[idx];
    }
  }
  return cur_node;
}

// dive to leaf by key
template <typename Key, typename Value, size_t N>
std::shared_ptr<Node<Key, Value, N>> GoToLeafAndRemember(
    const Key& key, const std::shared_ptr<Node<Key, Value, N>>& node,
    std::stack<std::shared_ptr<Node<Key, Value, N>>>& stck) {
  auto cur_node = node;
  while (!IsLeaf(cur_node)) {
    const auto& tmp_cur_node_keys = *(cur_node->keys);
    auto iter = KeyBinSearchInNode(key, *(cur_node->keys));
    while (iter == cur_node->keys->end() && cur_node->right != nullptr) {
      cur_node = cur_node->right;
      iter = KeyBinSearchInNode(key, *(cur_node->keys));
    }
    const auto idx = std::distance(cur_node->keys->begin(), iter);
    stck.push(cur_node);
    if (iter == cur_node->keys->end() && cur_node->right == nullptr) {
      cur_node = (*(cur_node->children))[idx - 1];
    } else {
      cur_node = (*(cur_node->children))[idx];
    }
  }
  return cur_node;
}

template <typename Key, typename Value, size_t N>
bool IsSaveNodeForDeletion(const std::shared_ptr<Node<Key, Value, N>>& node) {
  return node->keys->size() > (N / 2);
}

template <size_t N>
size_t GetSaveDeletionBound() {
  return N / 2;
}

template <typename Key, typename Value, size_t N>
Value* Search(const Key& key,
              const std::shared_ptr<Node<Key, Value, N>>& node) {
  if (node == nullptr) {
    return nullptr;
  }
  // go to leaf
  auto cur_node = GoToLeaf(key, node);
  // search in leaf
  while (cur_node != nullptr) {
    const auto& tmp_cur_keys = *(cur_node->keys);
    const auto iter = KeyBinSearchInNode(key, *(cur_node->keys));
    if (iter != cur_node->keys->end()) {
      const auto& tmp_cur_values = *(cur_node->values);
      const auto idx = std::distance(cur_node->keys->begin(), iter);
      if (*iter == key) {
        return &((*(cur_node->values))[idx]);
      }
      return nullptr;
    }
    cur_node = cur_node->right;
  }
  return nullptr;
}

// calls in insertion
template <typename Key, typename Value, size_t N>
std::shared_ptr<Node<Key, Value, N>> MoveRight(
    const Key& key, std::shared_ptr<Node<Key, Value, N>>& node) {
  auto cur_node = node;
  if (cur_node->keys->empty()) {
    return cur_node;
  }
  while (cur_node->keys->back() < key && cur_node->right != nullptr) {
    cur_node->mtx.unlock();
    cur_node = cur_node->right;
    if (cur_node == nullptr) {
      return nullptr;
    }
    cur_node->mtx.lock();
  }
  return cur_node;
}

template <typename Key, typename Value, size_t N>
std::shared_ptr<Node<Key, Value, N>> GetNodeForInsertion(
    const Key& key, std::shared_ptr<Node<Key, Value, N>>& node,
    std::stack<std::shared_ptr<Node<Key, Value, N>>>& stck) {
  auto cur_node = GoToLeafAndRemember(key, node, stck);
  cur_node->mtx.lock();
  // const auto aaa = cur_node->mtx.try_lock();
  return cur_node;
}

template <typename Key, typename Value, size_t N>
std::shared_ptr<Node<Key, Value, N>> SplitBigNode(
    std::shared_ptr<Node<Key, Value, N>>& node) {
  auto new_node = std::make_shared<Node<Key, Value, N>>();
  new_node->keys = std::move(std::make_unique<std::deque<Key>>());
  for (size_t i = 0; i < N; ++i) {
    new_node->keys->emplace_front(std::move(node->keys->back()));
    node->keys->pop_back();
  }
  if (IsLeaf(node)) {
    new_node->values = std::move(std::make_unique<std::deque<Value>>());
    for (size_t i = 0; i < N; ++i) {
      new_node->values->emplace_front(std::move(node->values->back()));
      node->values->pop_back();
    }
  } else {
    new_node->children = std::move(
        std::make_unique<std::deque<std::shared_ptr<Node<Key, Value, N>>>>());
    for (size_t i = 0; i < N; ++i) {
      auto& a = node->children->back();
      new_node->children->emplace_front(std::move(node->children->back()));
      auto& b = new_node->children->front();
      node->children->pop_back();
    }
  }
  new_node->right = std::move(node->right);
  node->right = new_node;
  return new_node;
}

template <typename Key, typename Value, size_t N>
void PutNewNodeIntoParent(size_t cur_node_idx,
                          std::shared_ptr<Node<Key, Value, N>>& parent,
                          const bool is_max_key) {
  if (is_max_key) {
    --cur_node_idx;
  }
  auto& cur_node = (*(parent->children))[cur_node_idx];
  auto& new_node = cur_node->right;
  if (is_max_key) {
    (*(parent->keys))[cur_node_idx] = new_node->keys->back();
  }
  auto cur_node_key_iter = parent->keys->begin();
  std::advance(cur_node_key_iter, cur_node_idx);
  auto cur_node_iter = parent->children->begin();
  std::advance(cur_node_iter, cur_node_idx + 1);
  parent->keys->emplace(cur_node_key_iter, cur_node->keys->back());
  parent->children->emplace(cur_node_iter, new_node);
  (*(parent->children))[cur_node_idx]->mtx.unlock();
}

template <typename Key, typename Value, size_t N>
std::shared_ptr<Node<Key, Value, N>> MakeRoot(
    std::shared_ptr<Node<Key, Value, N>>& left,
    std::shared_ptr<Node<Key, Value, N>>& right) {
  auto parent = std::make_shared<Node<Key, Value, N>>();
  parent->keys = std::move(std::make_unique<std::deque<Key>>());
  parent->keys->emplace_back(left->keys->back());
  parent->keys->emplace_back(right->keys->back());
  parent->children = std::move(
      std::make_unique<std::deque<std::shared_ptr<Node<Key, Value, N>>>>());
  parent->children->emplace_back(left);
  parent->children->emplace_back(right);
  return parent;
}

template <typename Key, typename Value, size_t N>
bool IsNodeOverflowed(const std::shared_ptr<Node<Key, Value, N>>& node) {
  return node->keys->size() > (2 * N);
}

// go by stack and set max key to each of them
template <typename Key, typename Value, size_t N>
void RefreshParentsAfterInsertionOfMax(
    const Key& key, std::stack<std::shared_ptr<Node<Key, Value, N>>>& stck) {
  if (stck.empty()) {
    return;
  }
  while (stck.size() > 1) {
    auto& last = stck.top();
    last->mtx.lock();
    last->keys->back() = key;
    last->mtx.unlock();
    stck.pop();
  }
  auto& last = stck.top();
  last->keys->back() = key;
  stck.pop();
}

template <typename Key, typename Value, size_t N>
void DoInsertionIntoNode(size_t cur_node_idx,
                         std::shared_ptr<Node<Key, Value, N>>& parent,
                         std::shared_ptr<Node<Key, Value, N>>& root,
                         std::stack<std::shared_ptr<Node<Key, Value, N>>>& stck,
                         const bool is_max_key) {
  const auto& cur_node = (*(parent->children))[cur_node_idx];  // tmp
  PutNewNodeIntoParent(cur_node_idx, parent, is_max_key);
  const auto& parent_tmp_keys = *(parent->keys);
  const auto& cur_node1 = (*(parent->children))[cur_node_idx];  // tmp

  if (!IsNodeOverflowed(parent)) {
    if (!is_max_key || !stck.empty()) {
      parent->mtx.unlock();
    }
    if (is_max_key) {
      const auto& key = (*(parent->keys))[cur_node_idx];
      RefreshParentsAfterInsertionOfMax(key, stck);
    }
    return;
  }

  auto new_node = SplitBigNode(parent);
  if (stck.empty()) {
    auto pre_parent = MakeRoot(parent, new_node);
    const auto& tmp_pre_par_keys = *(pre_parent->keys);
    root = pre_parent;
    const auto& tmp_root_keys = *(root->keys);
    if (is_max_key) {
      const auto& max_key = (*(parent->keys))[cur_node_idx];
      root->keys->back() = max_key;
      const auto& tmp_root_keys1 = *(root->keys);
      parent->mtx.unlock();
      return;
    }
    parent->mtx.unlock();
    return;
  }
  const auto& key = parent->keys->back();
  auto pre_parent = MoveRight(key, stck.top());
  if (!is_max_key || stck.size() > 1) {
    pre_parent->mtx.lock();
  }
  stck.pop();

  const auto& pre_par_tmp_keys = *(pre_parent->keys);
  const auto idx = GetIndex(key, *(pre_parent->keys));
  DoInsertionIntoNode(idx, pre_parent, root, stck, is_max_key);
}

template <typename Key, typename Value, size_t N>
void DoInsertion(std::pair<Key, Value>& item,
                 const typename std::deque<Key>::iterator& iter,
                 std::shared_ptr<Node<Key, Value, N>>& node,
                 std::shared_ptr<Node<Key, Value, N>>& root,
                 std::stack<std::shared_ptr<Node<Key, Value, N>>>& stck) {
  const bool is_max_key = (iter == node->keys->end());
  auto key = item.first;
  const auto value_iter = GetValueIter(iter, node);
  node->keys->emplace(iter, std::move(item.first));
  node->values->emplace(value_iter, std::move(item.second));
  if (!IsNodeOverflowed(node)) {
    node->mtx.unlock();
    if (is_max_key) {
      RefreshParentsAfterInsertionOfMax(key, stck);
    }
    return;
  }
  auto new_node = SplitBigNode(node);
  if (stck.empty()) {
    // if leaf is root
    auto parent = MakeRoot(node, new_node);
    root = parent;
    if (is_max_key) {
      root->keys->back() = key;
    }
    node->mtx.unlock();
    return;
  }
  auto parent = MoveRight(key, stck.top());
  if (parent != root) {
    parent->mtx.lock();
  }
  stck.pop();
  const auto idx = GetIndex(key, *(parent->keys));
  DoInsertionIntoNode(idx, parent, root, stck, is_max_key);
}

template <typename Key, typename Value, size_t N>
void EraseFromSaveLeaf(const typename std::deque<Key>::iterator& iter,
                       std::shared_ptr<Node<Key, Value, N>>& leaf) {
  const auto v_iter = GetValueIter(iter, leaf);
  leaf->keys->erase(iter);
  leaf->values->erase(v_iter);
  leaf->mtx.unlock();
}

// function calls to refresh parents after transfer values between neightbours
template <typename Key, typename Value, size_t N>
void RefreshParentsAfterTransfer(
    std::shared_ptr<Node<Key, Value, N>>& left,
    std::vector<std::shared_ptr<Node<Key, Value, N>>>& parents) {
  const auto& key = left->keys->back();
  const auto cur_key_idx = GetSaveDeletionBound<N>() - 1;
  const auto& cur_key = (*(left->keys))[cur_key_idx];
  size_t p_idx = parents.size() - 1;
  while (true) {
    // if (p_idx > 0) {
    //     parents[p_idx]->mtx.lock();
    // }
    auto& p_keys = *(parents[p_idx]->keys);
    const auto idx = GetIndex(cur_key, p_keys);
    p_keys[idx] = key;
    // if (p_idx > 0) {
    //     parents[p_idx]->mtx.unlock();
    // }
    if (p_idx == 0 || idx < p_keys.size() - 1) {
      break;
    } else if (p_idx == 1) {
      // root is locked yet
      auto& root_keys = *(parents.front()->keys);
      const auto r_idx = GetIndex(cur_key, root_keys);
      root_keys[r_idx] = key;
      return;
    }
    --p_idx;
  }
}

template <typename Key, typename Value, size_t N>
void TransferL2R(std::shared_ptr<Node<Key, Value, N>>& left,
                 std::shared_ptr<Node<Key, Value, N>>& right,
                 std::vector<std::shared_ptr<Node<Key, Value, N>>>& parents) {
  const auto delta = ((left->keys->size() + right->keys->size() + 1) / 2) -
                     right->keys->size();
  // transfer node values to neighbour
  for (size_t i = 0; i < delta; ++i) {
    right->keys->emplace_front(std::move(left->keys->back()));
    left->keys->pop_back();
  }
  if (IsLeaf(right)) {
    for (size_t i = 0; i < delta; ++i) {
      right->values->emplace_front(std::move(left->values->back()));
      left->values->pop_back();
    }
  } else {
    for (size_t i = 0; i < delta; ++i) {
      right->children->emplace_front(std::move(left->children->back()));
      left->children->pop_back();
    }
  }
  RefreshParentsAfterTransfer(left, parents);
}

template <typename Key, typename Value, size_t N>
void TransferR2L(std::shared_ptr<Node<Key, Value, N>>& left,
                 std::shared_ptr<Node<Key, Value, N>>& right,
                 std::vector<std::shared_ptr<Node<Key, Value, N>>>& parents) {
  const auto delta =
      ((left->keys->size() + right->keys->size() + 1) / 2) - left->keys->size();
  // transfer node values to neighbour
  for (size_t i = 0; i < delta; ++i) {
    left->keys->emplace_back(std::move(right->keys->front()));
    right->keys->pop_front();
  }
  if (IsLeaf(right)) {
    for (size_t i = 0; i < delta; ++i) {
      left->values->emplace_back(std::move(right->values->front()));
      right->values->pop_front();
    }
  } else {
    for (size_t i = 0; i < delta; ++i) {
      left->children->emplace_back(std::move(right->children->front()));
      right->children->pop_front();
    }
  }
  RefreshParentsAfterTransfer(left, parents);
}

template <typename Key, typename Value, size_t N>
void RefreshParentsAfterMerge(
    std::shared_ptr<Node<Key, Value, N>>& left,
    std::vector<std::shared_ptr<Node<Key, Value, N>>>& parents) {
  const auto& cur_key = left->keys->back();
  // if (parents.size() > 1) {
  //     parents.back()->mtx.lock();
  // }
  const auto& n_key = (*(left->keys))[GetSaveDeletionBound<N>() - 1];
  const auto n_iter = KeyBinSearchInNode(n_key, *(parents.back()->keys));

  const auto iter = KeyBinSearchInNode(cur_key, *(parents.back()->keys));
  const auto idx = std::distance(parents.back()->keys->begin(), iter);
  parents.back()->keys->erase(n_iter);
  auto child_iter = GetChildIter(iter, parents.back());
  std::advance(child_iter, 1);
  parents.back()->children->erase(child_iter);
  if (idx < parents.back()->keys->size()) {
    // if (parents.size() > 1) {
    //     parents.back()->mtx.unlock();
    // }
    return;
  }
  if (parents.size() > 2) {
    for (size_t i = parents.size() - 2; i > 0; --i) {
      auto& cur_parent = parents[i];
      cur_parent->mtx.lock();
      const auto cur_idx = GetIndex(cur_key, *(cur_parent->keys));
      (*(cur_parent->keys))[cur_idx] = cur_key;
      cur_parent->mtx.unlock();
      if (cur_idx < cur_parent->keys->size() - 1) {
        return;
      }
    }
  }

  auto& cur_parent_keys = *(parents[0]->keys);
  const auto cur_idx = GetIndex(cur_key, cur_parent_keys);
  cur_parent_keys[cur_idx] = cur_key;
}

template <typename Key, typename Value, size_t N>
void MergeLRNodes(std::shared_ptr<Node<Key, Value, N>>& left,
                  std::shared_ptr<Node<Key, Value, N>>& right,
                  std::vector<std::shared_ptr<Node<Key, Value, N>>>& parents) {
  for (auto& key : *(right->keys)) {
    left->keys->emplace_back(std::move(key));
  }
  right->keys->clear();
  if (IsLeaf(right)) {
    for (auto& value : *(right->values)) {
      left->values->emplace_back(std::move(value));
    }
    right->values->clear();
  } else {
    for (auto& kid : *(right->children)) {
      left->children->emplace_back(std::move(kid));
    }
    right->children->clear();
  }
  left->right = std::move(right->right);
  RefreshParentsAfterMerge(left, parents);
}

// make subtree save for deletion a element
template <typename Key, typename Value, size_t N>
std::shared_ptr<Node<Key, Value, N>> MakeSave(
    const Key& key, std::shared_ptr<Node<Key, Value, N>>& node) {
  auto cur_node = node;
  std::vector<std::shared_ptr<Node<Key, Value, N>>> parents;
  while (!IsLeaf(cur_node)) {
    parents.emplace_back(cur_node);
    auto& cur_parent = parents.back();
    // bamboo situation
    if (cur_parent->keys->size() == 1) {
      while (cur_parent->children != nullptr &&
             cur_parent->children->front()->keys->size() == 1 &&
             !IsLeaf(cur_parent->children->front())) {
        cur_parent->children->front() =
            std::move(cur_parent->children->front()->children->front());
      }
      if (IsLeaf(cur_parent->children->front())) {
        auto& kid = cur_parent->children->front();
        cur_parent->keys = std::move(kid->keys);
        cur_parent->values = std::move(kid->values);
        cur_parent->children = std::move(kid->children);
        return cur_parent;
      }
      cur_node = cur_parent;
    }
    auto iter = KeyBinSearchInNode(key, *(cur_node->keys));
    const auto idx = std::distance(cur_node->keys->begin(), iter);
    cur_node = (*(cur_node->children))[idx];

    if (IsSaveNodeForDeletion(cur_node)) {
      continue;
    }
    // cur_node->mtx.lock();
    if (idx != 0) {
      auto& left = (*(cur_parent->children))[idx - 1];
      if (IsSaveNodeForDeletion(left)) {
        left->mtx.lock();
        TransferL2R(left, cur_node, parents);
        left->mtx.unlock();
      } else if (idx < cur_parent->children->size() - 1 &&
                 IsSaveNodeForDeletion((*(cur_parent->children))[idx + 1])) {
        auto& right = (*(cur_parent->children))[idx + 1];
        right->mtx.lock();
        TransferR2L(cur_node, right, parents);
        right->mtx.unlock();
      } else {
        left->mtx.lock();
        MergeLRNodes(left, cur_node, parents);
        left->mtx.unlock();
        cur_node = left;
      }
    } else {
      // bamboo situation
      auto& right = (*(cur_parent->children))[idx + 1];
      right->mtx.lock();
      if (IsSaveNodeForDeletion(right)) {
        TransferR2L(cur_node, right, parents);
      } else {
        MergeLRNodes(cur_node, right, parents);
      }
      right->mtx.unlock();
    }
    // cur_node->mtx.unlock();
  }
  return cur_node;
}

template <typename Key, typename Value, size_t N>
void MakeSaveAndErase(const Key& key,
                      std::shared_ptr<Node<Key, Value, N>>& root) {
  root->mtx.lock();
  auto leaf = MakeSave(key, root);
  if (leaf != root) {
    leaf->mtx.lock();
  }
  const auto iter = KeyBinSearchInNode(key, *(leaf->keys));
  EraseFromSaveLeaf(iter, leaf);
  if (leaf != root) {
    root->mtx.unlock();
  }
}

}  // namespace

template <typename Key, typename Value, size_t N>
template <typename... Args>
void BLinkTree<Key, Value, N>::emplace(Args&&... args) {
  auto item =
      std::make_unique<std::pair<Key, Value>>(std::forward<Args>(args)...);
  if (root_ == nullptr) {
    root_ = std::make_shared<Node<Key, Value, N>>();
    root_->keys = std::make_unique<std::deque<Key>>();
    root_->values = std::make_unique<std::deque<Value>>();
    root_->keys->emplace_back(std::move(item->first));
    root_->values->emplace_back(std::move(item->second));
    ++size_;
    return;
  }
  auto& key = item->first;
  std::stack<std::shared_ptr<Node<Key, Value, N>>> stck;
  auto cur_node = GetNodeForInsertion(key, root_, stck);
  if (cur_node == nullptr) {
    /// TODO: smtg bad and unexpected
    return;
  }
  const auto iter = KeyBinSearchInNode(key, *(cur_node->keys));
  if (*iter == key) {
    // already contains this element
    cur_node->mtx.unlock();
    return;
  }
  if (item->first > root_->keys->back()) {
    if (cur_node != root_) {
      root_->mtx.lock();
    }
    DoInsertion(*item, iter, cur_node, root_, stck);
    root_->mtx.unlock();
  } else {
    DoInsertion(*item, iter, cur_node, root_, stck);
  }
  ++size_;
}

template <typename Key, typename Value, size_t N>
void BLinkTree<Key, Value, N>::insert(const std::pair<Key, Value>& value) {
  emplace(value);
}

template <typename Key, typename Value, size_t N>
void BLinkTree<Key, Value, N>::insert(std::pair<Key, Value>&& value) {
  emplace(std::move(value));
}

template <typename Key, typename Value, size_t N>
void BLinkTree<Key, Value, N>::erase(const Key& key) {
  if (root_ == nullptr) {
    throw std::out_of_range("put into empty tree");
  }
  auto leaf = GoToLeaf(key, root_);
  if (leaf == nullptr) {
    // smth wrong
    return;
  }
  leaf->mtx.lock();
  const auto iter = KeyBinSearchInNode(key, *(leaf->keys));
  if (*iter != key) {
    // does not exist
    leaf->mtx.unlock();
    return;
  }
  if (!IsSaveNodeForDeletion(leaf) && leaf != root_) {
    leaf->mtx.unlock();
    MakeSaveAndErase(key, root_);
  } else {
    EraseFromSaveLeaf(iter, leaf);
  }
  --size_;
}

template <typename Key, typename Value, size_t N>
Value& BLinkTree<Key, Value, N>::at(const Key& key) {
  auto value_ptr = Search(key, root_);

  if (value_ptr == nullptr) {
    throw std::out_of_range("");
  }
  return *value_ptr;
}

template <typename Key, typename Value, size_t N>
const Value& BLinkTree<Key, Value, N>::at(const Key& key) const {
  const auto value_ptr = Search(key, root_);

  if (value_ptr == nullptr) {
    throw std::out_of_range("");
  }
  return *value_ptr;
}

template <typename Key, typename Value, size_t N>
bool BLinkTree<Key, Value, N>::contains(const Key& key) const {
  const auto value_ptr = Search(key, root_);
  return value_ptr != nullptr;
}

template class BLinkTree<int, int, 2>;

}  // namespace btree