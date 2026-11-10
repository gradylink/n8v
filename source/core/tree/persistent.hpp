#pragma once

#include "core/tree/node.hpp"

#include <deque>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace n8v::detail::tree {

using BoundValue = std::variant<std::monostate, bool, int, float, std::string>;

struct OwnedProps {
  Node::Props props;
  std::string label;
  std::deque<std::string> strings;
  std::vector<const char *> items;
  std::string encoded;
};

constexpr int kOrdinalStride = 4;

struct PNode {
  PNode *parent = nullptr;
  std::string key;
  int ordinal = 0;
  OwnedProps current;
  BoundValue bound;
  std::vector<std::unique_ptr<PNode>> children;
  void *handle = nullptr;
};

enum UpdateFlags : unsigned {
  UpdateProps = 1u << 0,
  UpdateValue = 1u << 1,
};

struct TreeOps {
  virtual ~TreeOps() = default;

  virtual void create(PNode &node) = 0;
  virtual void update(PNode &node, const OwnedProps &previous, unsigned flags) = 0;
  virtual void insert(PNode &parent, PNode &child, size_t index) = 0;
  virtual void move(PNode &parent, PNode &child, size_t index) = 0;
  virtual void remove(PNode &parent, PNode &child) = 0;
  virtual void destroy(PNode &node) = 0;
};

} // namespace n8v::detail::tree
