#pragma once

#include <stddef.h>
#include <stdint.h>


class AVLNode {
 public:
  AVLNode *parent;
  AVLNode *left;
  AVLNode *right;

  uint32_t height;
  uint32_t cnt;

  AVLNode() : parent(NULL), left(NULL), right(NULL), height(1), cnt(1) {}

  void avl_update();

  AVLNode *avl_del_easy();

  AVLNode *avl_fix_left();
  AVLNode *avl_fix_right();

  AVLNode *rot_left();
  AVLNode *rot_right();

  uint32_t get_height() { return this->height; }
  uint32_t get_cnt() { return this->cnt; }

  AVLNode *avl_fix();
  AVLNode *avl_del();
};

inline uint32_t avl_height(AVLNode *node) { return node ? node->height : 0; }
inline uint32_t avl_cnt(AVLNode *node) { return node ? node->cnt : 0; }
