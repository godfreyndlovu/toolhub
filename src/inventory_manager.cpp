#include "inventory_manager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "file_handler.h"

namespace {

std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

InventoryManager::~InventoryManager() { stopAutoSave(); }

int InventoryManager::findProductIndexLocked(int id) const {
    for (std::size_t i = 0; i < products_.size(); ++i) {
        if (products_[i].id() == id) return static_cast<int>(i);
    }
    return -1;
}

int InventoryManager::findSupplierIndexLocked(int id) const {
    for (std::size_t i = 0; i < suppliers_.size(); ++i) {
        if (suppliers_[i].id() == id) return static_cast<int>(i);
    }
    return -1;
}

void InventoryManager::markDirtyLocked() { dirty_.store(true); }

bool InventoryManager::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    savePath_ = path;

    FileHandler::LoadResult result;
    if (!FileHandler::loadFromFile(path, result, malformedLinesSkipped_)) {
        return false;
    }
    products_ = std::move(result.products);
    suppliers_ = std::move(result.suppliers);
    transactionLog_ = TransactionLog{};
    for (const auto& t : result.transactions) {
        transactionLog_.append(t);
    }

    nextProductId_ = 1;
    for (const auto& p : products_) nextProductId_ = std::max(nextProductId_, p.id() + 1);
    nextSupplierId_ = 1;
    for (const auto& s : suppliers_) nextSupplierId_ = std::max(nextSupplierId_, s.id() + 1);
    nextTransactionId_ = 1;
    transactionLog_.forEach([this](const Transaction& t) {
        nextTransactionId_ = std::max(nextTransactionId_, t.id + 1);
    });

    dirty_.store(false);
    return true;
}

bool InventoryManager::saveLocked() {
    std::vector<Transaction> txns;
    txns.reserve(transactionLog_.size());
    transactionLog_.forEach([&txns](const Transaction& t) { txns.push_back(t); });
    bool ok = FileHandler::saveToFile(savePath_, products_, suppliers_, txns);
    if (ok) dirty_.store(false);
    return ok;
}

bool InventoryManager::save() {
    std::lock_guard<std::mutex> lock(mutex_);
    return saveLocked();
}

void InventoryManager::autoSaveLoop(int intervalMs) {
    while (autoSaveRunning_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        if (!autoSaveRunning_.load()) break;
        if (dirty_.load()) {
            std::lock_guard<std::mutex> lock(mutex_);
            saveLocked();
        }
    }
}

void InventoryManager::startAutoSave(int intervalMs) {
    if (autoSaveRunning_.exchange(true)) return; // already running
    autoSaveThread_ = std::thread(&InventoryManager::autoSaveLoop, this, intervalMs);
}

void InventoryManager::stopAutoSave() {
    if (!autoSaveRunning_.exchange(false)) {
        return; // was not running
    }
    if (autoSaveThread_.joinable()) {
        autoSaveThread_.join();
    }
    if (dirty_.load()) {
        std::lock_guard<std::mutex> lock(mutex_);
        saveLocked();
    }
}

int InventoryManager::addProduct(const std::string& name, const std::string& category,
                                    int quantity, double sellingPrice, double costPrice,
                                    int reorderThreshold, int supplierId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (findSupplierIndexLocked(supplierId) == -1 && supplierId != 0) return -1;

    int id = nextProductId_++;
    products_.emplace_back(id, name, category, quantity, sellingPrice, costPrice,
                             reorderThreshold, supplierId);
    markDirtyLocked();
    return id;
}

bool InventoryManager::editProduct(int id, const std::string& name,
                                      const std::string& category, double sellingPrice,
                                      double costPrice, int reorderThreshold,
                                      int supplierId) {
    std::lock_guard<std::mutex> lock(mutex_);
    int idx = findProductIndexLocked(id);
    if (idx == -1) return false;
    if (supplierId != 0 && findSupplierIndexLocked(supplierId) == -1) return false;

    Product& p = products_[idx];
    p.setName(name);
    p.setCategory(category);
    p.setSellingPrice(sellingPrice);
    p.setCostPrice(costPrice);
    p.setReorderThreshold(reorderThreshold);
    p.setSupplierId(supplierId);
    markDirtyLocked();
    return true;
}

bool InventoryManager::removeProduct(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    int idx = findProductIndexLocked(id);
    if (idx == -1) return false;
    products_.erase(products_.begin() + idx);
    // Historical transactions referencing this product are kept as an audit
    // trail; Reports::transactionHistoryReport displays them with a
    // "(deleted product #id)" placeholder name.
    markDirtyLocked();
    return true;
}

bool InventoryManager::productNameExists(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string target = toLower(name);
    for (const auto& p : products_) {
        if (toLower(p.name()) == target) return true;
    }
    return false;
}

bool InventoryManager::supplierExists(int supplierId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (supplierId == 0) return true;
    return findSupplierIndexLocked(supplierId) != -1;
}

std::vector<Product> InventoryManager::products() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return products_;
}

std::vector<Product> InventoryManager::searchByName(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string target = toLower(query);
    std::vector<Product> out;
    for (const auto& p : products_) {
        if (toLower(p.name()).find(target) != std::string::npos) out.push_back(p);
    }
    return out;
}

std::vector<Product> InventoryManager::sortedByName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Product> out = products_;
    std::sort(out.begin(), out.end(), [](const Product& a, const Product& b) {
        return toLower(a.name()) < toLower(b.name());
    });
    return out;
}

std::vector<Product> InventoryManager::sortedByQuantity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Product> out = products_;
    std::sort(out.begin(), out.end(), [](const Product& a, const Product& b) {
        return a.quantity() < b.quantity();
    });
    return out;
}

std::vector<Product> InventoryManager::sortedByPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Product> out = products_;
    std::sort(out.begin(), out.end(), [](const Product& a, const Product& b) {
        return a.sellingPrice() < b.sellingPrice();
    });
    return out;
}

int InventoryManager::addSupplier(const std::string& name, const std::string& contact) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextSupplierId_++;
    suppliers_.emplace_back(id, name, contact);
    markDirtyLocked();
    return id;
}

bool InventoryManager::editSupplier(int id, const std::string& name, const std::string& contact) {
    std::lock_guard<std::mutex> lock(mutex_);
    int idx = findSupplierIndexLocked(id);
    if (idx == -1) return false;
    suppliers_[idx].setName(name);
    suppliers_[idx].setContact(contact);
    markDirtyLocked();
    return true;
}

bool InventoryManager::removeSupplier(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    int idx = findSupplierIndexLocked(id);
    if (idx == -1) return false;
    suppliers_.erase(suppliers_.begin() + idx);
    // Referential integrity: no product may keep pointing at a supplier id
    // that no longer exists, so every product referencing this supplier is
    // reset to "no supplier" (0) rather than being left dangling.
    for (auto& p : products_) {
        if (p.supplierId() == id) p.setSupplierId(0);
    }
    markDirtyLocked();
    return true;
}

std::vector<Supplier> InventoryManager::suppliers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return suppliers_;
}

bool InventoryManager::recordTransaction(int productId, TransactionType type, int quantity,
                                            const std::string& note) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (quantity <= 0) return false;
    int idx = findProductIndexLocked(productId);
    if (idx == -1) return false;

    int delta = (type == TransactionType::StockIn) ? quantity : -quantity;
    if (!products_[idx].adjustQuantity(delta)) return false; // would go negative

    Transaction t;
    t.id = nextTransactionId_++;
    t.productId = productId;
    t.type = type;
    t.quantity = quantity;
    t.timestamp = currentTimestamp();
    t.note = note;
    transactionLog_.append(t);
    markDirtyLocked();
    return true;
}

bool InventoryManager::undoLastTransaction(Transaction& undoneOut) {
    std::lock_guard<std::mutex> lock(mutex_);
    Transaction removed;
    if (!transactionLog_.removeLast(removed)) return false;

    // Reverse the quantity effect if the referenced product still exists.
    // If the product was since deleted, the transaction record is still
    // removed (undo always succeeds once there is a transaction to undo);
    // there is simply no quantity left to reverse.
    int idx = findProductIndexLocked(removed.productId);
    if (idx != -1) {
        products_[idx].adjustQuantity(-signedDelta(removed));
    }
    undoneOut = removed;
    markDirtyLocked();
    return true;
}

std::vector<Transaction> InventoryManager::transactions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Transaction> out;
    out.reserve(transactionLog_.size());
    transactionLog_.forEach([&out](const Transaction& t) { out.push_back(t); });
    return out;
}
