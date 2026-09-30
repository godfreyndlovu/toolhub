#include "transaction_log.h"

TransactionLog::~TransactionLog() { clear(); }

TransactionLog::TransactionLog(const TransactionLog& other) { copyFrom(other); }

TransactionLog& TransactionLog::operator=(const TransactionLog& other) {
    if (this == &other) return *this;
    clear();
    copyFrom(other);
    return *this;
}

void TransactionLog::append(const Transaction& t) {
    Node* node = new Node{t, nullptr};
    if (tail_ == nullptr) {
        head_ = tail_ = node;
    } else {
        tail_->next = node;
        tail_ = node;
    }
    ++size_;
}

bool TransactionLog::removeLast(Transaction& out) {
    if (head_ == nullptr) return false;

    if (head_ == tail_) {
        // Single node.
        out = head_->data;
        delete head_;
        head_ = tail_ = nullptr;
        size_ = 0;
        return true;
    }

    // Walk to the node just before tail_ (singly linked, so no shortcut).
    Node* prev = head_;
    while (prev->next != tail_) {
        prev = prev->next;
    }
    out = tail_->data;
    delete tail_;
    tail_ = prev;
    tail_->next = nullptr;
    --size_;
    return true;
}

void TransactionLog::forEach(const std::function<void(const Transaction&)>& fn) const {
    for (Node* cur = head_; cur != nullptr; cur = cur->next) {
        fn(cur->data);
    }
}

void TransactionLog::copyFrom(const TransactionLog& other) {
    head_ = tail_ = nullptr;
    size_ = 0;
    for (Node* cur = other.head_; cur != nullptr; cur = cur->next) {
        append(cur->data);
    }
}

void TransactionLog::clear() {
    Node* cur = head_;
    while (cur != nullptr) {
        Node* next = cur->next;
        delete cur;
        cur = next;
    }
    head_ = tail_ = nullptr;
    size_ = 0;
}
