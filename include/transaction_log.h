#pragma once

#include <functional>
#include <cstddef>
#include "transaction.h"

// Hand-built singly linked list of Transactions, using raw pointers.
//
// This is a deliberate C/C++ competency demonstration: transaction history is
// append-only and read sequentially, a pattern std::vector would also serve
// well. The linked list is chosen specifically to exercise dynamic memory
// allocation, pointer traversal, ownership, and manual destruction -- the
// module's core memory-model competency -- rather than because it is
// technically necessary here.
//
// TransactionLog owns every node it allocates via `new`. The destructor walks
// the chain and `delete`s each node. Copy construction and copy assignment
// perform a deep copy of the chain (rule of three) so that two independently
// destroyed logs never double-free the same node.
class TransactionLog {
public:
    TransactionLog() = default;
    ~TransactionLog();

    TransactionLog(const TransactionLog& other);
    TransactionLog& operator=(const TransactionLog& other);

    // Appends a copy of t to the end of the chain. O(1) via a tracked tail
    // pointer.
    void append(const Transaction& t);

    // Removes the last transaction in the chain (if any) and copies its data
    // into `out`. O(n) -- must walk to find the node before the tail, since
    // this is a singly linked list. Used only for the single-level "undo
    // last transaction" feature, which is not performance-sensitive.
    bool removeLast(Transaction& out);

    // Calls fn(t) for every transaction, in insertion order. O(n).
    void forEach(const std::function<void(const Transaction&)>& fn) const;

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

private:
    struct Node {
        Transaction data;
        Node* next = nullptr;
    };

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;

    void copyFrom(const TransactionLog& other);
    void clear();
};
