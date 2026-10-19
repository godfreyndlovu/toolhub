#include "file_handler.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// Replace `to` with `from` in one step. std::rename() fails on Windows when
// the destination already exists, which would make every save after the
// first one silently fail, so Windows uses MoveFileEx with REPLACE_EXISTING.
// On POSIX, rename() already replaces the destination atomically.
bool replaceFile(const std::string& from, const std::string& to) {
#ifdef _WIN32
    return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return std::rename(from.c_str(), to.c_str()) == 0;
#endif
}

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    return parts;
}

} // namespace

namespace FileHandler {

bool loadFromFile(const std::string& path, LoadResult& out, int& malformedLinesSkipped) {
    out = LoadResult{};
    malformedLinesSkipped = 0;

    std::ifstream in(path);
    if (!in.is_open()) {
        // No file yet -- normal first run, not an error.
        return true;
    }

    enum class Section { None, Products, Suppliers, Transactions };
    Section section = Section::None;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line == "[PRODUCTS]") { section = Section::Products; continue; }
        if (line == "[SUPPLIERS]") { section = Section::Suppliers; continue; }
        if (line == "[TRANSACTIONS]") { section = Section::Transactions; continue; }

        auto parts = splitPipe(line);
        try {
            if (section == Section::Products) {
                if (parts.size() != 8) { ++malformedLinesSkipped; continue; }
                Product p(std::stoi(parts[0]), parts[1], parts[2], std::stoi(parts[3]),
                           std::stod(parts[4]), std::stod(parts[5]), std::stoi(parts[6]),
                           std::stoi(parts[7]));
                out.products.push_back(p);
            } else if (section == Section::Suppliers) {
                if (parts.size() != 3) { ++malformedLinesSkipped; continue; }
                out.suppliers.emplace_back(std::stoi(parts[0]), parts[1], parts[2]);
            } else if (section == Section::Transactions) {
                if (parts.size() != 6) { ++malformedLinesSkipped; continue; }
                Transaction t;
                t.id = std::stoi(parts[0]);
                t.productId = std::stoi(parts[1]);
                t.type = (parts[2] == "IN") ? TransactionType::StockIn : TransactionType::StockOut;
                t.quantity = std::stoi(parts[3]);
                t.timestamp = parts[4];
                t.note = parts[5];
                out.transactions.push_back(t);
            } else {
                ++malformedLinesSkipped;
            }
        } catch (const std::exception&) {
            ++malformedLinesSkipped;
        }
    }
    return true;
}

bool saveToFile(const std::string& path, const std::vector<Product>& products,
                  const std::vector<Supplier>& suppliers,
                  const std::vector<Transaction>& transactions) {
    const std::string tmpPath = path + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out.is_open()) return false;

        out << "[PRODUCTS]\n";
        for (const auto& p : products) {
            out << p.id() << '|' << p.name() << '|' << p.category() << '|'
                << p.quantity() << '|' << p.sellingPrice() << '|' << p.costPrice()
                << '|' << p.reorderThreshold() << '|' << p.supplierId() << '\n';
        }

        out << "[SUPPLIERS]\n";
        for (const auto& s : suppliers) {
            out << s.id() << '|' << s.name() << '|' << s.contact() << '\n';
        }

        out << "[TRANSACTIONS]\n";
        for (const auto& t : transactions) {
            out << t.id << '|' << t.productId << '|'
                << (t.type == TransactionType::StockIn ? "IN" : "OUT") << '|'
                << t.quantity << '|' << t.timestamp << '|' << t.note << '\n';
        }

        if (!out.good()) return false;
    } // out closes/flushes here

    if (!replaceFile(tmpPath, path)) {
        std::remove(tmpPath.c_str());
        return false;
    }
    return true;
}

} // namespace FileHandler
