#pragma once

#include "core/tree/node.hpp"
#include "core/tree/persistent.hpp"

namespace n8v::detail::tree {

class Reconciler {
public:
  Reconciler();

  void reconcile(const Node &recordedRoot, TreeOps *ops);

  PNode &root() { return root_; }

  void clear(TreeOps *ops);

private:
  PNode root_;
};

Reconciler &reconciler();

void emitTree();

void setNodeDestroyedHook(void (*hook)(const PNode &node));

} // namespace n8v::detail::tree
