#ifndef SHOPTRACK_TRANSACTION_H
#define SHOPTRACK_TRANSACTION_H

#include <string>

enum class TransactionType { StockIn, StockOut };

// Quantity is always non-negative; type determines direction.
// (Matches the on-disk format: Type | Quantity, not a signed delta.)
struct Transaction {
    int id = 0;
    int productId = 0;
    TransactionType type = TransactionType::StockIn;
    int quantity = 0;
    std::string timestamp; // ISO-ish: YYYY-MM-DDTHH:MM
    std::string note;
};

inline int signedDelta(const Transaction& t) {
    return t.type == TransactionType::StockIn ? t.quantity : -t.quantity;
}

#endif // SHOPTRACK_TRANSACTION_H
