#include "core/tree/reconcile.hpp"

#include "core/tree/props.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace n8v::detail::tree {
namespace {

std::vector<std::string> keysFor(const std::vector<Node *> &children) {
  std::vector<std::string> keys;
  keys.reserve(children.size());
  std::unordered_set<std::string> used;
  std::unordered_map<size_t, int> unkeyedOrdinal;
  for (const Node *child : children) {
    const size_t kind = child->props.index();
    const char *id = idOf(child->props);
    std::string key = std::to_string(kind);
    if (const auto *entry = std::get_if<n8v_entry_options>(&child->props); entry && entry->multiline) key += entry->match_entry_style ? "ms" : "m"; // the single-line style is a different widget tree on GTK
    if (id) {
      key += "#";
      key += id;
    } else {
      key += ":" + std::to_string(unkeyedOrdinal[kind]++);
    }
    for (int n = 1; !used.insert(key).second; ++n) key += "@" + std::to_string(n);
    keys.push_back(std::move(key));
  }
  return keys;
}

void (*destroyedHook)(const PNode &) = nullptr;
int lastOrdinal = 0;

void destroySubtree(PNode &node, TreeOps *ops) {
  for (auto &child : node.children) destroySubtree(*child, ops);
  if (destroyedHook) destroyedHook(node);
  if (ops) ops->destroy(node);
}

void reconcileChildren(PNode &parent, const std::vector<Node *> &recorded, TreeOps *ops);

void updateNode(PNode &node, const Node &recorded, TreeOps *ops) {
  OwnedProps next = adopt(recorded);
  unsigned flags = 0;
  if (next.encoded != node.current.encoded) flags |= UpdateProps;

  BoundValue bound = readBound(next.props);
  if (bound != node.bound) flags |= UpdateValue;
  node.bound = std::move(bound);

  if (flags & UpdateProps) {
    std::swap(node.current, next);
    if (ops) ops->update(node, next, flags);
  } else if (flags && ops) {
    ops->update(node, node.current, flags);
  }

  reconcileChildren(node, recorded.children, ops);
}

std::unique_ptr<PNode> buildNode(PNode &parent, const Node &recorded, std::string key, TreeOps *ops) {
  auto node = std::make_unique<PNode>();
  node->parent = &parent;
  node->key = std::move(key);
  node->ordinal = (lastOrdinal += kOrdinalStride);
  node->current = adopt(recorded);
  node->bound = readBound(node->current.props);
  if (ops) ops->create(*node);
  reconcileChildren(*node, recorded.children, ops);
  return node;
}

void reconcileChildren(PNode &parent, const std::vector<Node *> &recorded, TreeOps *ops) {
  const std::vector<std::string> keys = keysFor(recorded);

  std::unordered_map<std::string, size_t> oldIndexByKey;
  for (size_t i = 0; i < parent.children.size(); ++i) oldIndexByKey.emplace(parent.children[i]->key, i);

  std::vector<std::unique_ptr<PNode>> old = std::move(parent.children);
  parent.children.clear();
  std::vector<PNode *> matched(recorded.size(), nullptr);
  std::vector<bool> survives(old.size(), false);
  for (size_t i = 0; i < recorded.size(); ++i) {
    auto it = oldIndexByKey.find(keys[i]);
    if (it == oldIndexByKey.end() || survives[it->second]) continue;
    survives[it->second] = true;
    matched[i] = old[it->second].get();
  }

  std::vector<PNode *> live;
  for (size_t i = 0; i < old.size(); ++i) {
    if (survives[i]) {
      live.push_back(old[i].get());
    } else {
      if (ops) ops->remove(parent, *old[i]);
      destroySubtree(*old[i], ops);
    }
  }

  std::unordered_map<PNode *, std::unique_ptr<PNode>> owner;
  for (auto &node : old)
    if (node) owner.emplace(node.get(), std::move(node));

  std::vector<std::unique_ptr<PNode>> next;
  next.reserve(recorded.size());
  for (size_t i = 0; i < recorded.size(); ++i) {
    if (PNode *existing = matched[i]) {
      updateNode(*existing, *recorded[i], ops);
      if (i >= live.size() || live[i] != existing) {
        auto from = std::find(live.begin(), live.end(), existing);
        live.erase(from);
        live.insert(live.begin() + (ptrdiff_t)i, existing);
        if (ops) ops->move(parent, *existing, i);
      }
      next.push_back(std::move(owner.at(existing)));
    } else {
      std::unique_ptr<PNode> created = buildNode(parent, *recorded[i], keys[i], ops);
      live.insert(live.begin() + (ptrdiff_t)i, created.get());
      if (ops) ops->insert(parent, *created, i);
      next.push_back(std::move(created));
    }
  }
  parent.children = std::move(next);
}

} // namespace

Reconciler::Reconciler() { root_.current.props = std::monostate{}; }

void Reconciler::reconcile(const Node &recordedRoot, TreeOps *ops) { reconcileChildren(root_, recordedRoot.children, ops); }

void Reconciler::clear(TreeOps *ops) {
  for (auto &child : root_.children) {
    if (ops) ops->remove(root_, *child);
    destroySubtree(*child, ops);
  }
  root_.children.clear();
}

void setNodeDestroyedHook(void (*hook)(const PNode &node)) { destroyedHook = hook; }

Reconciler &reconciler() {
  static Reconciler instance;
  return instance;
}

} // namespace n8v::detail::tree
