#include "reports.h"
#include <algorithm>

namespace Reports {

std::vector<LowStockEntry> lowStockReport(const std::vector<Product>& products) {
    std::vector<LowStockEntry> entries;
    for (const auto& p : products) {
        if (p.isBelowThreshold()) {
            entries.push_back(LowStockEntry{
                p.id(), p.name(), p.quantity(), p.reorderThreshold(),
                p.reorderThreshold() - p.quantity()
            });
        }
    }
    std::sort(entries.begin(), entries.end(),
              [](const LowStockEntry& a, const LowStockEntry& b) {
                  return a.shortfall > b.shortfall; // most urgent first
              });
    return entries;
}

std::vector<Transaction> transactionHistoryReport(const TransactionLog& log) {
    std::vector<Transaction> result;
    result.reserve(log.size());
    log.forEach([&result](const Transaction& t) { result.push_back(t); });
    return result;
}

} // namespace Reports
