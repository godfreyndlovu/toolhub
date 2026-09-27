#ifndef SHOPTRACK_TRANSACTION_LOG_H
#define SHOPTRACK_TRANSACTION_LOG_H

#include "transaction.h"
#include <functional>

// A hand-built singly linked list of Transactions, using raw pointers.
// Chosen over std::vector because the access pattern is append-only,
// sequential-read (never random-indexed) -- this is the module's
// deliberate demonstration of manual pointer/memory management.
//
// TransactionLog owns every node it allocates. The destructor walks
// the chain and frees each node; copy construction/assignment perform
// a deep copy, since a shallow copy of raw pointers would cause a
// double-free when both copies are destroyed (rule of three).
class TransactionLog {
public:
    TransactionLog() = default;
    ~TransactionLog();

    TransactionLog(const TransactionLog& other);
    TransactionLog& operator=(const TransactionLog& other);

    // Appends a copy of the transaction to the end of the list. O(1)
    // via the tail pointer.
    void append(const Transaction& t);

    // Calls fn(const Transaction&) for every entry, in insertion order.
    // Used by Reports functions to build read-only views without
    // exposing the internal node structure.
    void forEach(const std::function<void(const Transaction&)>& fn) const;

    std::size_t size() const { return size_; }
    bool empty() const { return head_ == nullptr; }

private:
    struct TransactionNode {
        Transaction data;
        TransactionNode* next = nullptr;
    };

    TransactionNode* head_ = nullptr;
    TransactionNode* tail_ = nullptr;
    std::size_t size_ = 0;

    void clear();
    void copyFrom(const TransactionLog& other);
};

#endif // SHOPTRACK_TRANSACTION_LOG_H
