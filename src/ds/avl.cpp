#include "avl.h"
#include <cassert>
#include <algorithm>
#include <cstdint>

using namespace std;

void AVLNode::avl_update() {
  this->height = 1 + max(avl_height(this->left), avl_height(this->right));
  this->cnt = 1 + avl_cnt(this->left) + avl_cnt(this->right);
}

AVLNode *AVLNode::rot_left() {
  auto par = this->parent;
  auto new_node = this->right;
  auto inner = new_node->left;

  this->right = inner;
  if (inner) {
    inner->parent = this;
  }

  new_node->parent = par;
  new_node->left = this;
  this->parent = new_node;

  avl_update();
  new_node->avl_update();
  return new_node;
}

AVLNode *AVLNode::rot_right() {
  auto par = this->parent;
  auto new_node = this->left;
  auto inner = new_node->right;

  this->left = inner;
  if (inner) {
    inner->parent = this;
  }

  new_node->parent = par;
  new_node->right = this;
  this->parent = new_node;

  avl_update();
  new_node->avl_update();
  return new_node;
}

AVLNode *AVLNode::avl_fix_left() {
  if (avl_height(this->left->left) < avl_height(this->left->right)) {
    this->left = this->left->rot_left();
  }
  return this->rot_right();
}

AVLNode *AVLNode::avl_fix_right() {
  if (avl_height(this->right->right) < avl_height(this->right->left)) {
    this->right = this->right->rot_right();
  }
  return this->rot_left();
}

AVLNode *AVLNode::avl_fix() {
  auto t = this;
  while (true) {
    
    AVLNode **from = &t;
    AVLNode *parent = t->parent;
    if (parent) {
      from = parent->left == t ? &parent->left : &parent->right;
    }

    t->avl_update();

    uint32_t l = avl_height(t->left);
    uint32_t r = avl_height(t->right);
    if (l == r + 2) {
      *from = t->avl_fix_left();
    } else if (l  + 2 == r) {
      *from = t->avl_fix_right();
    }

    if (!parent) {
      return *from;
    }
    t = parent;
  }
}

AVLNode *AVLNode::avl_del_easy() {
  assert(!this->left || !this->right);
  AVLNode *child = this->left ? this->left : this->right;
  AVLNode *parent = this->parent;

  if (child) {
    child->parent = parent;
  }

  if (!parent) {
    return child;
  }

  AVLNode **from = parent->left == this ? &parent->left : &parent->right;
  *from = child;
  return parent->avl_fix(); 
}

AVLNode *AVLNode::avl_del() {
  if (!this->left || !this->right) {
    return this->avl_del_easy();
  }
  
  AVLNode *victim = this->right;
  while (victim->left) {
    victim = victim->left;
  }

  AVLNode *root = victim->avl_del_easy();

  *victim = *this;

  if (victim->left) {
    victim->left->parent = victim;
  }

  if (victim->right) {
    victim->right->parent = victim;
  }

  AVLNode **from = &root;
  AVLNode *parent = this->parent;
  if (parent) {
    from = parent->left == this ? &parent->left : &parent->right;
  }
  *from = victim;
  return root;
}

AVLNode *AVLNode::avl_offset(int64_t offset) {
  AVLNode *node = this;
  int64_t pos = 0;
  while (offset != pos) {
    if (pos < offset && pos + (int64_t)avl_cnt(node->right) >= offset) {
      node = node->right;
      pos += 1 + (int64_t)avl_cnt(node->left);
    } else if (pos > offset && pos - (int64_t)avl_cnt(node->left) <= offset) {
      node = node->left;
      pos -= 1 + (int64_t)avl_cnt(node->right);
    } else {
      AVLNode *parent = node->parent;
      if (!parent) {
        return nullptr;
      }
      if (parent->right == node) {
        pos -= 1 + (int64_t)avl_cnt(node->left);
      } else {
        pos += 1 + (int64_t)avl_cnt(node->right);
      }
      node = parent;
    }
  }
  return node;
}
