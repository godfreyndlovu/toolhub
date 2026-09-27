#include "inventory_manager.h"
#include "file_handler.h"
#include <algorithm>
#include <cctype>

int InventoryManager::addProduct(const std::string& name, const std::string& category,
                                  int quantity, double sellingPrice, double costPrice,
                                  int reorderThreshold, int supplierId) {
    if (name.empty() || quantity < 0 || sellingPrice < 0 || costPrice < 0
        || reorderThreshold < 0 || !supplierExists(supplierId)) {
        return -1; // invalid input: reject rather than store bad data
    }
    // Note: duplicate names/categories are allowed deliberately -- a shop
    // can legitimately stock two batches of the same item; each gets its
    // own auto-generated id, so nothing downstream (lookup, transactions,
    // reports) relies on names being unique.
    int id = nextProductId_++;
    products_.emplace_back(id, name, category, quantity, sellingPrice, costPrice,
                            reorderThreshold, supplierId);
    productIndexById_[id] = products_.size() - 1;
    return id;
}

bool InventoryManager::editProduct(int id, const std::string& name, const std::string& category,
                                    double sellingPrice, double costPrice, int reorderThreshold,
                                    int supplierId) {
    if (name.empty() || category.empty() || sellingPrice < 0 || costPrice < 0
        || reorderThreshold < 0 || !supplierExists(supplierId)) {
        return false;
    }
    auto idx = findProductIndex(id);
    if (!idx) return false;
    products_[*idx].setName(name);
    products_[*idx].setCategory(category);
    products_[*idx].setSellingPrice(sellingPrice);
    products_[*idx].setCostPrice(costPrice);
    products_[*idx].setReorderThreshold(reorderThreshold);
    products_[*idx].setSupplierId(supplierId);
    return true;
}

bool InventoryManager::removeProduct(int id) {
    auto idx = findProductIndex(id);
    if (!idx) return false;
    // Historical transactions referencing this id are intentionally kept
    // (see Design Decisions: audit trail).
    products_.erase(products_.begin() + static_cast<long>(*idx));
    rebuildIndex();
    return true;
}

std::optional<int> InventoryManager::findProductIndex(int id) const {
    auto it = productIndexById_.find(id);
    if (it == productIndexById_.end()) return std::nullopt;
    return static_cast<int>(it->second);
}

bool InventoryManager::productNameExists(const std::string& name) const {
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        return s;
    };
    std::string target = lower(name);
    return std::any_of(products_.begin(), products_.end(),
                        [&](const Product& p) { return lower(p.name()) == target; });
}

bool InventoryManager::supplierExists(int supplierId) const {
    if (supplierId == 0) return true; // 0 means "no supplier", always valid
    return std::any_of(suppliers_.begin(), suppliers_.end(),
                        [&](const Supplier& s) { return s.id() == supplierId; });
}

void InventoryManager::rebuildIndex() {
    productIndexById_.clear();
    for (std::size_t i = 0; i < products_.size(); ++i) {
        productIndexById_[products_[i].id()] = i;
    }
}

std::vector<Product> InventoryManager::searchProductsByName(const std::string& query) const {
    std::vector<Product> results;
    for (const auto& p : products_) {
        if (p.name().find(query) != std::string::npos) {
            results.push_back(p);
        }
    }
    return results;
}

std::vector<Product> InventoryManager::productsSortedByName() const {
    auto copy = products_;
    std::sort(copy.begin(), copy.end(),
              [](const Product& a, const Product& b) { return a.name() < b.name(); });
    return copy;
}

std::vector<Product> InventoryManager::productsSortedByQuantity() const {
    auto copy = products_;
    std::sort(copy.begin(), copy.end(),
              [](const Product& a, const Product& b) { return a.quantity() < b.quantity(); });
    return copy;
}

std::vector<Product> InventoryManager::productsSortedByPrice() const {
    auto copy = products_;
    std::sort(copy.begin(), copy.end(),
              [](const Product& a, const Product& b) { return a.sellingPrice() < b.sellingPrice(); });
    return copy;
}

int InventoryManager::addSupplier(const std::string& name, const std::string& contact) {
    int id = nextSupplierId_++;
    suppliers_.emplace_back(id, name, contact);
    return id;
}

bool InventoryManager::recordTransaction(int productId, TransactionType type, int quantity,
                                          const std::string& timestamp, const std::string& note) {
    if (quantity <= 0) {
        return false; // a transaction must move a positive amount of stock
    }
    auto idx = findProductIndex(productId);
    if (!idx) return false;

    int delta = (type == TransactionType::StockIn) ? quantity : -quantity;
    if (!products_[*idx].adjustQuantity(delta)) {
        return false; // would overdraw
    }

    Transaction t;
    t.id = nextTransactionId_++;
    t.productId = productId;
    t.type = type;
    t.quantity = quantity;
    t.timestamp = timestamp;
    t.note = note;
    log_.append(t);
    return true;
}

bool InventoryManager::load(const std::string& path) {
    return FileHandler::loadFromFile(path, products_, suppliers_, log_,
                                      nextProductId_, nextSupplierId_, nextTransactionId_,
                                      malformedLinesSkipped_);
}

bool InventoryManager::save(const std::string& path) const {
    return FileHandler::saveToFile(path, products_, suppliers_, log_);
}
