#include "file_handler.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace {

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '|')) {
        fields.push_back(field);
    }
    return fields;
}

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

namespace FileHandler {

bool loadFromFile(const std::string& path,
                   std::vector<Product>& products,
                   std::vector<Supplier>& suppliers,
                   TransactionLog& log,
                   int& nextProductId,
                   int& nextSupplierId,
                   int& nextTransactionId,
                   int& malformedLinesSkipped) {
    products.clear();
    suppliers.clear();
    log = TransactionLog();
    malformedLinesSkipped = 0;

    std::ifstream in(path);
    if (!in.is_open()) {
        // First run: no file yet. Not an error.
        return true;
    }

    enum class Section { None, Products, Suppliers, Transactions };
    Section section = Section::None;
    std::string line;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line == "[PRODUCTS]") { section = Section::Products; continue; }
        if (line == "[SUPPLIERS]") { section = Section::Suppliers; continue; }
        if (line == "[TRANSACTIONS]") { section = Section::Transactions; continue; }

        auto f = splitPipe(line);
        bool ok = false;

        if (section == Section::Products && f.size() == 8) {
            try {
                int id = std::stoi(f[0]);
                Product p(id, f[1], f[2], std::stoi(f[5]),
                          std::stod(f[3]), std::stod(f[4]),
                          std::stoi(f[6]), std::stoi(f[7]));
                products.push_back(p);
                nextProductId = std::max(nextProductId, id + 1);
                ok = true;
            } catch (...) {
                ok = false;
            }
        } else if (section == Section::Suppliers && f.size() == 3) {
            try {
                int id = std::stoi(f[0]);
                suppliers.emplace_back(id, f[1], f[2]);
                nextSupplierId = std::max(nextSupplierId, id + 1);
                ok = true;
            } catch (...) {
                ok = false;
            }
        } else if (section == Section::Transactions && f.size() == 6) {
            try {
                Transaction t;
                t.id = std::stoi(f[0]);
                t.productId = std::stoi(f[1]);
                t.type = (f[2] == "IN") ? TransactionType::StockIn : TransactionType::StockOut;
                t.quantity = std::stoi(f[3]);
                t.timestamp = f[4];
                t.note = f[5];
                log.append(t);
                nextTransactionId = std::max(nextTransactionId, t.id + 1);
                ok = true;
            } catch (...) {
                ok = false;
            }
        }
        // A line with the wrong field count for its section (or no
        // recognised section at all) is also malformed.

        if (!ok) {
            ++malformedLinesSkipped;
        }
    }

    return true;
}

bool saveToFile(const std::string& path,
                 const std::vector<Product>& products,
                 const std::vector<Supplier>& suppliers,
                 const TransactionLog& log) {
    const std::string tmpPath = path + ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out.is_open()) return false;

        out << "[PRODUCTS]\n";
        for (const auto& p : products) {
            out << p.id() << "|" << p.name() << "|" << p.category() << "|"
                << p.sellingPrice() << "|" << p.costPrice() << "|"
                << p.quantity() << "|" << p.reorderThreshold() << "|"
                << p.supplierId() << "\n";
        }

        out << "[SUPPLIERS]\n";
        for (const auto& s : suppliers) {
            out << s.id() << "|" << s.name() << "|" << s.contact() << "\n";
        }

        out << "[TRANSACTIONS]\n";
        log.forEach([&out](const Transaction& t) {
            out << t.id << "|" << t.productId << "|"
                << (t.type == TransactionType::StockIn ? "IN" : "OUT") << "|"
                << t.quantity << "|" << t.timestamp << "|" << t.note << "\n";
        });

        if (!out.good()) return false;
        // out closes here (end of scope), flushing to disk before rename.
    }

    // Atomic swap: if this fails, the original file at `path` is
    // untouched -- a crash or a failed write never corrupts existing data.
    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        return false;
    }
    return true;
}

} // namespace FileHandler
