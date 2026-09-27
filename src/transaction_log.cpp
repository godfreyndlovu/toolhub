#include "transaction_log.h"

TransactionLog::~TransactionLog() {
    clear();
}

TransactionLog::TransactionLog(const TransactionLog& other) {
    copyFrom(other);
}

TransactionLog& TransactionLog::operator=(const TransactionLog& other) {
    if (this != &other) {
        clear();
        copyFrom(other);
    }
    return *this;
}

void TransactionLog::append(const Transaction& t) {
    TransactionNode* node = new TransactionNode{t, nullptr};
    if (tail_ == nullptr) {
        head_ = node;
        tail_ = node;
    } else {
        tail_->next = node;
        tail_ = node;
    }
    ++size_;
}

void TransactionLog::forEach(const std::function<void(const Transaction&)>& fn) const {
    for (TransactionNode* cur = head_; cur != nullptr; cur = cur->next) {
        fn(cur->data);
    }
}

void TransactionLog::clear() {
    TransactionNode* cur = head_;
    while (cur != nullptr) {
        TransactionNode* next = cur->next;
        delete cur;
        cur = next;
    }
    head_ = nullptr;
    tail_ = nullptr;
    size_ = 0;
}

void TransactionLog::copyFrom(const TransactionLog& other) {
    for (TransactionNode* cur = other.head_; cur != nullptr; cur = cur->next) {
        append(cur->data);
    }
}
