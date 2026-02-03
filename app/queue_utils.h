#ifndef QUEUE_UTILS_H_
#define QUEUE_UTILS_H_

#include <queue>
#include <utility>

template <typename T>
void ClearQueue(std::queue<T> &q) {
  std::queue<T> empty;
  std::swap(empty, q);
}

#endif  // QUEUE_UTILS_H_
