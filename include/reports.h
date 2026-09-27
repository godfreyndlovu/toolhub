#ifndef SHOPTRACK_REPORTS_H
#define SHOPTRACK_REPORTS_H

#include "product.h"
#include "transaction_log.h"
#include <vector>

// Reporting functions for stock levels and transaction history.
// Functions here take their inputs as parameters and return a result;
// they do not read or modify InventoryManager's stored state directly,
// and have no side effects (no printing, no file I/O). This keeps them
// independently testable and keeps InventoryManager as the only module
// that owns the domain data.
namespace Reports {

struct LowStockEntry {
    int productId;
    std::string name;
    int quantity;
    int reorderThreshold;
    int shortfall; // reorderThreshold - quantity; used to rank urgency
};

// Returns products at or below their reorder threshold, most urgent first.
std::vector<LowStockEntry> lowStockReport(const std::vector<Product>& products);

// Returns a chronological copy of all transactions (insertion order).
std::vector<Transaction> transactionHistoryReport(const TransactionLog& log);

} // namespace Reports

#endif // SHOPTRACK_REPORTS_H
