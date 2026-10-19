#pragma once

#include <string>

enum class TransactionType { StockIn, StockOut };

// A single recorded stock movement. Quantity is always stored as a
// non-negative magnitude; the sign of its effect on stock is determined by
// `type`, matching the persistence file format's Type + Quantity columns.
struct Transaction {
    int id = 0;
    int productId = 0;
    TransactionType type = TransactionType::StockIn;
    int quantity = 0;
    std::string timestamp;
    std::string note;
};

// The signed effect this transaction has on a product's quantity:
// +quantity for a stock-in, -quantity for a stock-out.
inline int signedDelta(const Transaction& t) {
    return t.type == TransactionType::StockIn ? t.quantity : -t.quantity;
}
