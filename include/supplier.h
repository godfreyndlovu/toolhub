#pragma once

#include <string>

class Supplier {
public:
    Supplier() = default;
    Supplier(int id, std::string name, std::string contact);

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& contact() const { return contact_; }

    void setName(const std::string& name) { name_ = name; }
    void setContact(const std::string& contact) { contact_ = contact; }

private:
    int id_ = 0;
    std::string name_;
    std::string contact_;
};
