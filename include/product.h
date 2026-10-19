#pragma once

#include <string>

// A stocked item. Quantity only ever changes through InventoryManager's
// transaction path (recordTransaction / adjustQuantity), never directly via
// editProduct -- this is a deliberate design decision so that every quantity
// change is accompanied by a Transaction record (see CODEBOOK.md).
class Product {
public:
    Product() = default;
    Product(int id, std::string name, std::string category, int quantity,
             double sellingPrice, double costPrice, int reorderThreshold,
             int supplierId);

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

    // Returns false (and leaves quantity unchanged) if delta would push
    // quantity below zero.
    bool adjustQuantity(int delta);

private:
    int id_ = 0;
    std::string name_;
    std::string category_;
    int quantity_ = 0;
    double sellingPrice_ = 0.0;
    double costPrice_ = 0.0;
    int reorderThreshold_ = 0;
    int supplierId_ = 0; // 0 == no supplier
};
