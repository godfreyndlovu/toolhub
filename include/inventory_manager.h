#ifndef SHOPTRACK_INVENTORY_MANAGER_H
#define SHOPTRACK_INVENTORY_MANAGER_H

#include "product.h"
#include "supplier.h"
#include "transaction_log.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

// Orchestrates the domain classes and persistence. Owns the data;
// Product/Supplier/TransactionLog stay focused on their own state.
class InventoryManager {
public:
    // --- Products (baseline) ---
    // Returns the new product's id, or -1 if the input is invalid
    // (empty name, negative quantity/price/threshold, or a supplierId
    // that is neither 0 (no supplier) nor an existing supplier's id).
    int addProduct(const std::string& name, const std::string& category,
                    int quantity, double sellingPrice, double costPrice,
                    int reorderThreshold, int supplierId);
    // Returns false if the id doesn't exist or the input is invalid,
    // including the same supplierId rule as addProduct.
    // Quantity is deliberately NOT editable here -- it only changes
    // through recordTransaction, so TransactionLog stays the single
    // source of truth for every stock change (see CODEBOOK.md).
    bool editProduct(int id, const std::string& name, const std::string& category,
                      double sellingPrice, double costPrice, int reorderThreshold,
                      int supplierId);
    bool removeProduct(int id); // historical transactions referencing this id are kept

    std::optional<int> findProductIndex(int id) const;
    bool productNameExists(const std::string& name) const; // case-insensitive
    bool supplierExists(int supplierId) const; // true for id 0 (meaning "none") or a known supplier
    const std::vector<Product>& products() const { return products_; }

    std::vector<Product> searchProductsByName(const std::string& query) const;
    std::vector<Product> productsSortedByName() const;
    std::vector<Product> productsSortedByQuantity() const;
    std::vector<Product> productsSortedByPrice() const;

    // --- Suppliers (data model is baseline; full management is optional --
    //     kept minimal here rather than a full CRUD surface) ---
    int addSupplier(const std::string& name, const std::string& contact);
    const std::vector<Supplier>& suppliers() const { return suppliers_; }

    // --- Transactions (baseline) ---
    // Returns false if productId doesn't exist, quantity is not
    // positive, or a StockOut would take quantity below zero.
    bool recordTransaction(int productId, TransactionType type, int quantity,
                            const std::string& timestamp, const std::string& note);
    const TransactionLog& transactionLog() const { return log_; }

    // --- Persistence ---
    // load() returns false only if the file exists but cannot be
    // opened/read at all; a missing file is a valid first run and
    // still returns true. Malformed individual lines are skipped, not
    // fatal -- check malformedLinesSkipped() afterwards to see how many.
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    int malformedLinesSkipped() const { return malformedLinesSkipped_; }

private:
    std::vector<Product> products_;
    std::vector<Supplier> suppliers_;
    std::unordered_map<int, std::size_t> productIndexById_;
    TransactionLog log_;

    int nextProductId_ = 1;
    int nextSupplierId_ = 1;
    int nextTransactionId_ = 1;
    int malformedLinesSkipped_ = 0;

    void rebuildIndex();
};

#endif // SHOPTRACK_INVENTORY_MANAGER_H
