#ifndef LAB2_CONCURRENT_QUEUE_H
#define LAB2_CONCURRENT_QUEUE_H

#include "helper.h"
#include <vector>
#include <memory>

template <typename T>
class PriorityQueue {
    using queue_implementation = std::vector<std::shared_ptr<T>>;
    typedef bool (*TComparator)(const std::shared_ptr<T> &, const std::shared_ptr<T> &);

public:

    PriorityQueue(TComparator comparator) {
        this->comparator = comparator;
        std::make_heap(this->queue_base.begin(), this->queue_base.end(), this->comparator);
    };

    inline ~PriorityQueue() { clear(); }

    bool empty() const;

    size_t size() const;

public:
    void clear();

    bool pop(std::shared_ptr<T> &out_value);

    void push(T &value);
    void push(T &&value);
    void push(std::shared_ptr<T> &value);

    std::shared_ptr<T> &at(uint32_t index);

public:
    PriorityQueue(PriorityQueue const &other) = delete;

    PriorityQueue &operator=(PriorityQueue const &rhs) = delete;

private:
    mutable rw_lock read_write_lock;

    queue_implementation queue_base;

    TComparator comparator;
};

template <typename T>
bool PriorityQueue<T>::empty() const {
    read_lock _(this->read_write_lock);
    return this->queue_base.empty();
}

template <typename T>
size_t PriorityQueue<T>::size() const {
    read_lock _(this->read_write_lock);
    return this->queue_base.size();
}

template <typename T>
void PriorityQueue<T>::clear() {
    write_lock _(this->read_write_lock);
    while (!this->queue_base.empty()) {
        this->queue_base.pop_back();
    }
}

template <typename T>
bool PriorityQueue<T>::pop(std::shared_ptr<T> &out_value) {
    write_lock _(this->read_write_lock);

    if (this->queue_base.empty()) return false;

    std::pop_heap(this->queue_base.begin(), this->queue_base.end(), this->comparator);
    out_value = this->queue_base.back();

    this->queue_base.pop_back();

    return true;
}

template <typename T>
void PriorityQueue<T>::push(T &value) {
    write_lock _(this->read_write_lock);

    this->queue_base.push_back(std::make_shared<T>(value));
    std::push_heap(this->queue_base.begin(), this->queue_base.end(), this->comparator);
}

template <typename T>
void PriorityQueue<T>::push(T &&value) {
    write_lock _(this->read_write_lock);

    this->queue_base.push_back(std::make_shared<T>(value));
    std::push_heap(this->queue_base.begin(), this->queue_base.end(), this->comparator);
}

template <typename T>
void PriorityQueue<T>::push(std::shared_ptr<T> &value) {
    write_lock _(this->read_write_lock);

    this->queue_base.push_back(value);
    std::push_heap(this->queue_base.begin(), this->queue_base.end(), this->comparator);
}

template <typename T>
std::shared_ptr<T> &PriorityQueue<T>::at(uint32_t index) {
    read_lock _(this->read_write_lock);
    return this->queue_base[index];
}

#endif //LAB2_CONCURRENT_QUEUE_H
