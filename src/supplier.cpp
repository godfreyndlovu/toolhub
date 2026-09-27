#include "supplier.h"

Supplier::Supplier(int id, std::string name, std::string contact)
    : id_(id), name_(std::move(name)), contact_(std::move(contact)) {}
