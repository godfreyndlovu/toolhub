#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "product.h"
#include "supplier.h"
#include "transaction.h"
#include "transaction_log.h"

// Owns all inventory state (products, suppliers, transaction history) and is
// the sole entry point through which the CLI (main.cpp) reads and mutates
// that state. Every public method here takes the internal mutex, so it is
// safe to call from the main thread while the background auto-save thread
// is concurrently reading the same state to persist it.
class InventoryManager {
public:
    InventoryManager() = default;
    ~InventoryManager();

    // Non-copyable: owns a background thread and a mutex.
    InventoryManager(const InventoryManager&) = delete;
    InventoryManager& operator=(const InventoryManager&) = delete;

    // --- Persistence ---------------------------------------------------
    // Loads from `path` (remembered for subsequent save()/auto-save calls).
    // A missing file is a normal first run, not a failure.
    bool load(const std::string& path);
    // Synchronous, explicit save to the path given to load(). Used by the
    // CLI's "Save & exit" and by the auto-save thread.
    bool save();
    int malformedLinesSkipped() const { return malformedLinesSkipped_; }

    // Starts a background thread that flushes to disk roughly every
    // `intervalMs` milliseconds, but only if a mutating operation has
    // happened since the last flush (a `dirty` flag, checked and cleared
    // atomically). Safe to call once after load(). Demonstrates
    // std::thread / std::mutex / std::atomic concurrency, per the
    // assignment's optional-extension list.
    void startAutoSave(int intervalMs = 4000);
    // Stops the background thread (joining it) and performs one final
    // synchronous save if any change is still unflushed. Also called
    // automatically by the destructor.
    void stopAutoSave();

    // --- Products --------------------------------------------------------
    int addProduct(const std::string& name, const std::string& category, int quantity,
                     double sellingPrice, double costPrice, int reorderThreshold,
                     int supplierId);
    // Updates every editable field except quantity (quantity changes only
    // via recordTransaction/undoLastTransaction, so every quantity change is
    // always accompanied by a Transaction record). Returns false if the
    // product id does not exist or supplierId does not refer to a real
    // supplier (0 is always allowed, meaning "no supplier").
    bool editProduct(int id, const std::string& name, const std::string& category,
                       double sellingPrice, double costPrice, int reorderThreshold,
                       int supplierId);
    bool removeProduct(int id);
    bool productNameExists(const std::string& name) const; // case-insensitive
    bool supplierExists(int supplierId) const; // 0 always counts as valid ("none")

    std::vector<Product> products() const;
    std::vector<Product> searchByName(const std::string& query) const; // case-insensitive substring
    std::vector<Product> sortedByName() const;
    std::vector<Product> sortedByQuantity() const;
    std::vector<Product> sortedByPrice() const;

    // --- Suppliers ---------------------------------------------------
    int addSupplier(const std::string& name, const std::string& contact);
    bool editSupplier(int id, const std::string& name, const std::string& contact);
    // Removing a supplier does not remove the products that reference it;
    // those products' supplierId is reset to 0 ("no supplier") so no
    // product is ever left pointing at a supplier id that no longer exists.
    bool removeSupplier(int id);
    std::vector<Supplier> suppliers() const;

    // --- Transactions ---------------------------------------------------
    // Records a stock movement and applies it to the product's quantity.
    // Fails (returns false, no state change) if the product does not exist,
    // quantity <= 0, or a stock-out would take quantity below zero.
    bool recordTransaction(int productId, TransactionType type, int quantity,
                             const std::string& note);
    // Reverses the single most recent transaction: removes it from the log
    // and, if the referenced product still exists, undoes its effect on
    // that product's quantity. This is a single-level undo (the last
    // transaction only, not a full undo stack) -- a deliberate scope
    // decision documented in CODEBOOK.md. Returns false if there is no
    // transaction to undo.
    bool undoLastTransaction(Transaction& undoneOut);
    std::vector<Transaction> transactions() const;

private:
    mutable std::mutex mutex_;
    std::vector<Product> products_;
    std::vector<Supplier> suppliers_;
    TransactionLog transactionLog_;

    std::string savePath_;
    int malformedLinesSkipped_ = 0;
    int nextProductId_ = 1;
    int nextSupplierId_ = 1;
    int nextTransactionId_ = 1;

    std::atomic<bool> dirty_{false};
    std::atomic<bool> autoSaveRunning_{false};
    std::thread autoSaveThread_;

    // Assumes mutex_ is already held by the caller.
    int findProductIndexLocked(int id) const;
    int findSupplierIndexLocked(int id) const;
    void markDirtyLocked();
    bool saveLocked();
    void autoSaveLoop(int intervalMs);
};
