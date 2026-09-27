#ifndef SHOPTRACK_SUPPLIER_H
#define SHOPTRACK_SUPPLIER_H

#include <string>

// Represents a supplier. Owned by std::vector<Supplier> in InventoryManager.
class Supplier {
public:
    Supplier() = default;
    Supplier(int id, std::string name, std::string contact);

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& contact() const { return contact_; }

private:
    int id_ = 0;
    std::string name_;
    std::string contact_;
};

#endif // SHOPTRACK_SUPPLIER_H
