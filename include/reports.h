#pragma once

#include <string>
#include <vector>
#include "product.h"
#include "transaction.h"

// Reporting functions for stock levels and transaction history.
//
// Every function here is pure: it receives its data as parameters, has no
// side effects, and returns a result. None of them read or write files, and
// none of them mutate the Product/Transaction data they are given. This
// keeps reports independently testable (see tests/test_inventory.cpp) and
// decoupled from persistence and CLI concerns.
namespace Reports {

struct LowStockRow {
    int productId;
    std::string name;
    int quantity;
    int reorderThreshold;
    int shortfall; // reorderThreshold - quantity; always > 0 in the result
};

// Returns every product at or below its reorder threshold, ranked most
// urgent (largest shortfall) first.
std::vector<LowStockRow> lowStockReport(const std::vector<Product>& products);

struct TransactionRow {
    int transactionId;
    int productId;
    std::string productName; // "(deleted product #<id>)" if no longer present
    TransactionType type;
    int quantity;
    std::string timestamp;
    std::string note;
};

// Returns every transaction in insertion order, resolving product names
// where the product still exists.
std::vector<TransactionRow> transactionHistoryReport(
    const std::vector<Product>& products,
    const std::vector<Transaction>& transactions);

} // namespace Reports
