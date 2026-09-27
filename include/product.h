#ifndef SHOPTRACK_PRODUCT_H
#define SHOPTRACK_PRODUCT_H

#include <string>

// Represents a single stocked item. Owned by std::vector<Product> in
// InventoryManager (RAII — no manual allocation needed here).
class Product {
public:
    Product() = default;
    Product(int id, std::string name, std::string category, int quantity,
            double sellingPrice, double costPrice, int reorderThreshold, int supplierId);

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& category() const { return category_; }
    int quantity() const { return quantity_; }
    double sellingPrice() const { return sellingPrice_; }
    double costPrice() const { return costPrice_; }
    int reorderThreshold() const { return reorderThreshold_; }
    int supplierId() const { return supplierId_; }

    void setName(const std::string& name) { name_ = name; }
    void setCategory(const std::string& category) { category_ = category; }
    void setSellingPrice(double price) { sellingPrice_ = price; }
    void setCostPrice(double price) { costPrice_ = price; }
    void setReorderThreshold(int threshold) { reorderThreshold_ = threshold; }
    void setSupplierId(int supplierId) { supplierId_ = supplierId; }

    // Applies a transaction delta (positive = restock, negative = sale).
    // Returns false if the change would take quantity below zero.
    bool adjustQuantity(int delta);

    bool isBelowThreshold() const { return quantity_ <= reorderThreshold_; }

private:
    int id_ = 0;
    std::string name_;
    std::string category_;
    int quantity_ = 0;
    double sellingPrice_ = 0.0;
    double costPrice_ = 0.0;
    int reorderThreshold_ = 0;
    int supplierId_ = 0;
};

#endif // SHOPTRACK_PRODUCT_H
