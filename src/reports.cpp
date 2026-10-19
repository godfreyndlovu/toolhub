#include "reports.h"

#include <algorithm>
#include <unordered_map>

namespace Reports {

std::vector<LowStockRow> lowStockReport(const std::vector<Product>& products) {
    std::vector<LowStockRow> rows;
    for (const auto& p : products) {
        if (p.quantity() <= p.reorderThreshold()) {
            rows.push_back(LowStockRow{
                p.id(), p.name(), p.quantity(), p.reorderThreshold(),
                p.reorderThreshold() - p.quantity()});
        }
    }
    std::sort(rows.begin(), rows.end(), [](const LowStockRow& a, const LowStockRow& b) {
        return a.shortfall > b.shortfall;
    });
    return rows;
}

std::vector<TransactionRow> transactionHistoryReport(
    const std::vector<Product>& products,
    const std::vector<Transaction>& transactions) {
    std::unordered_map<int, std::string> nameById;
    nameById.reserve(products.size());
    for (const auto& p : products) {
        nameById[p.id()] = p.name();
    }

    std::vector<TransactionRow> rows;
    rows.reserve(transactions.size());
    for (const auto& t : transactions) {
        auto it = nameById.find(t.productId);
        std::string name = (it != nameById.end())
            ? it->second
            : ("(deleted product #" + std::to_string(t.productId) + ")");
        rows.push_back(TransactionRow{t.id, t.productId, name, t.type, t.quantity,
                                        t.timestamp, t.note});
    }
    return rows;
}

} // namespace Reports
