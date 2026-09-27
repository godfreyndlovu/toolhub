#include "product.h"

Product::Product(int id, std::string name, std::string category, int quantity,
                  double sellingPrice, double costPrice, int reorderThreshold, int supplierId)
    : id_(id), name_(std::move(name)), category_(std::move(category)),
      quantity_(quantity), sellingPrice_(sellingPrice), costPrice_(costPrice),
      reorderThreshold_(reorderThreshold), supplierId_(supplierId) {}

bool Product::adjustQuantity(int delta) {
    if (quantity_ + delta < 0) {
        return false;
    }
    quantity_ += delta;
    return true;
}
