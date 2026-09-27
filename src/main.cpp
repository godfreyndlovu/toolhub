// ToolHub -- command-line interface.
//
// This file owns all user interaction (prompting, reading input,
// printing results) and delegates every actual operation to
// InventoryManager. No business logic lives here.

#include "inventory_manager.h"
#include "reports.h"
#include <iostream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <chrono>
#include <ctime>

namespace {

const std::string DATA_FILE = "inventory.dat";

// ---------------------------------------------------------------
// Input helpers: every one of these loops until it gets a value
// that satisfies its constraint, so a single bad keystroke never
// crashes the program or corrupts a later prompt. This is the
// "invalid input" edge case from the assignment brief.
// ---------------------------------------------------------------

void clearBadInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

std::string readNonEmptyLine(const std::string& prompt) {
    while (true) {
        std::string line = readLine(prompt);
        if (!line.empty()) return line;
        std::cout << "  This can't be empty. Try again.\n";
    }
}

int readInt(const std::string& prompt, int min, int max) {
    while (true) {
        std::cout << prompt;
        int value;
        if (std::cin >> value && value >= min && value <= max) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        clearBadInput();
        std::cout << "  Enter a whole number between " << min << " and " << max << ".\n";
    }
}

int readNonNegativeInt(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        int value;
        if (std::cin >> value && value >= 0) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        clearBadInput();
        std::cout << "  Enter a whole number that is zero or greater (no negative quantities).\n";
    }
}

double readNonNegativeDouble(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        double value;
        if (std::cin >> value && value >= 0.0) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        clearBadInput();
        std::cout << "  Enter a number that is zero or greater.\n";
    }
}

bool readYesNo(const std::string& prompt) {
    while (true) {
        std::string line = readLine(prompt + " (y/n): ");
        if (!line.empty() && (line[0] == 'y' || line[0] == 'Y')) return true;
        if (!line.empty() && (line[0] == 'n' || line[0] == 'N')) return false;
        std::cout << "  Please answer y or n.\n";
    }
}

std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M");
    return oss.str();
}

// ---------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------

void printProductTable(const std::vector<Product>& products) {
    if (products.empty()) {
        std::cout << "  (no products to show)\n";
        return;
    }
    std::cout << std::left
               << std::setw(4)  << "ID"
               << std::setw(18) << "Name"
               << std::setw(12) << "Category"
               << std::setw(8)  << "Qty"
               << std::setw(10) << "Price"
               << std::setw(8)  << "Reorder"
               << "SupplierId\n";
    for (const auto& p : products) {
        std::cout << std::left
                   << std::setw(4)  << p.id()
                   << std::setw(18) << p.name()
                   << std::setw(12) << p.category()
                   << std::setw(8)  << p.quantity()
                   << std::setw(10) << p.sellingPrice()
                   << std::setw(8)  << p.reorderThreshold()
                   << p.supplierId() << "\n";
    }
}

void printLowStockReport(const InventoryManager& mgr) {
    auto entries = Reports::lowStockReport(mgr.products());
    // Edge case: empty inventory / nothing below threshold both handled here.
    if (entries.empty()) {
        std::cout << "  No products are below their reorder threshold.\n";
        return;
    }
    std::cout << std::left << std::setw(20) << "Product"
               << std::setw(10) << "Qty"
               << std::setw(12) << "Reorder at"
               << "Shortfall\n";
    for (const auto& e : entries) {
        std::cout << std::left << std::setw(20) << e.name
                   << std::setw(10) << e.quantity
                   << std::setw(12) << e.reorderThreshold
                   << e.shortfall << "\n";
    }
}

void printTransactionHistory(const InventoryManager& mgr) {
    auto history = Reports::transactionHistoryReport(mgr.transactionLog());
    if (history.empty()) {
        std::cout << "  No transactions recorded yet.\n";
        return;
    }
    std::cout << std::left << std::setw(6)  << "Txn"
               << std::setw(8)  << "Product"
               << std::setw(6)  << "Type"
               << std::setw(6)  << "Qty"
               << std::setw(18) << "When"
               << "Note\n";
    for (const auto& t : history) {
        std::cout << std::left << std::setw(6)  << t.id
                   << std::setw(8)  << t.productId
                   << std::setw(6)  << (t.type == TransactionType::StockIn ? "IN" : "OUT")
                   << std::setw(6)  << t.quantity
                   << std::setw(18) << t.timestamp
                   << t.note << "\n";
    }
}

// ---------------------------------------------------------------
// Menu actions -- each one gathers input, calls InventoryManager,
// and reports the outcome. No business logic here.
// ---------------------------------------------------------------

void actionAddProduct(InventoryManager& mgr) {
    std::string name = readNonEmptyLine("Product name: ");

    // Edge case: duplicate items. We don't silently reject -- we warn
    // and let the user confirm, since two genuinely different products
    // (e.g. two different-sized boxes of the same nails) could share a name.
    if (mgr.productNameExists(name)) {
        std::cout << "  A product named \"" << name << "\" already exists.\n";
        if (!readYesNo("  Add it anyway as a separate entry?")) {
            std::cout << "  Cancelled.\n";
            return;
        }
    }

    std::string category = readNonEmptyLine("Category: ");
    int quantity = readNonNegativeInt("Starting quantity: ");
    double sellingPrice = readNonNegativeDouble("Selling price: ");
    double costPrice = readNonNegativeDouble("Cost price: ");
    int reorderThreshold = readNonNegativeInt("Reorder threshold: ");

    int supplierId = -1;
    if (!mgr.suppliers().empty()) {
        std::cout << "  Known suppliers:\n";
        for (const auto& s : mgr.suppliers()) {
            std::cout << "    " << s.id() << ": " << s.name() << "\n";
        }
        supplierId = readInt("Supplier id (0 if none): ", 0, std::numeric_limits<int>::max());
    } else {
        std::cout << "  (No suppliers on file yet -- add one from the main menu if needed.)\n";
        supplierId = 0;
    }

    int id = mgr.addProduct(name, category, quantity, sellingPrice, costPrice,
                             reorderThreshold, supplierId);
    std::cout << "  Added product #" << id << ".\n";
}

void actionEditProduct(InventoryManager& mgr) {
    if (mgr.products().empty()) {
        std::cout << "  Inventory is empty -- nothing to edit.\n";
        return;
    }
    printProductTable(mgr.products());
    int id = readInt("Product id to edit: ", 0, std::numeric_limits<int>::max());
    if (!mgr.findProductIndex(id)) {
        std::cout << "  No product with id " << id << ".\n";
        return;
    }
    // Quantity is intentionally not editable here -- it only changes via
    // "Record transaction", so the transaction log stays the single
    // source of truth for every stock change.
    std::cout << "  (Quantity isn't edited here -- use \"Record transaction\" for that.)\n";
    std::string name = readNonEmptyLine("New name: ");
    std::string category = readNonEmptyLine("New category: ");
    double sellingPrice = readNonNegativeDouble("New selling price: ");
    double costPrice = readNonNegativeDouble("New cost price: ");
    int reorderThreshold = readNonNegativeInt("New reorder threshold: ");
    if (!mgr.suppliers().empty()) {
        std::cout << "  Known suppliers:\n";
        for (const auto& s : mgr.suppliers()) {
            std::cout << "    " << s.id() << ": " << s.name() << "\n";
        }
    }
    int supplierId = readInt("New supplier id (0 if none): ", 0, std::numeric_limits<int>::max());

    if (mgr.editProduct(id, name, category, sellingPrice, costPrice, reorderThreshold, supplierId)) {
        std::cout << "  Updated.\n";
    } else {
        std::cout << "  Update failed -- check the supplier id refers to an existing supplier (or 0).\n";
    }
}

void actionRemoveProduct(InventoryManager& mgr) {
    if (mgr.products().empty()) {
        std::cout << "  Inventory is empty -- nothing to remove.\n";
        return;
    }
    printProductTable(mgr.products());
    int id = readInt("Product id to remove: ", 0, std::numeric_limits<int>::max());
    if (mgr.removeProduct(id)) {
        std::cout << "  Removed. Its past transactions remain in the history as an audit trail.\n";
    } else {
        std::cout << "  No product with id " << id << ".\n";
    }
}

void actionSearch(InventoryManager& mgr) {
    if (mgr.products().empty()) {
        std::cout << "  Inventory is empty -- nothing to search.\n";
        return;
    }
    std::string query = readNonEmptyLine("Search by name (partial match): ");
    auto results = mgr.searchProductsByName(query);
    if (results.empty()) {
        std::cout << "  No products matched \"" << query << "\".\n";
        return;
    }
    printProductTable(results);
}

void actionSort(InventoryManager& mgr) {
    if (mgr.products().empty()) {
        std::cout << "  Inventory is empty -- nothing to sort.\n";
        return;
    }
    std::cout << "  Sort by: 1) Name  2) Quantity  3) Price\n";
    int choice = readInt("  Choice: ", 1, 3);
    std::vector<Product> sorted;
    if (choice == 1) sorted = mgr.productsSortedByName();
    else if (choice == 2) sorted = mgr.productsSortedByQuantity();
    else sorted = mgr.productsSortedByPrice();
    printProductTable(sorted);
}

void actionAddSupplier(InventoryManager& mgr) {
    std::string name = readNonEmptyLine("Supplier name: ");
    std::string contact = readNonEmptyLine("Contact (email/phone): ");
    int id = mgr.addSupplier(name, contact);
    std::cout << "  Added supplier #" << id << ".\n";
}

void actionRecordTransaction(InventoryManager& mgr) {
    if (mgr.products().empty()) {
        std::cout << "  Inventory is empty -- add a product first.\n";
        return;
    }
    printProductTable(mgr.products());
    int productId = readInt("Product id: ", 0, std::numeric_limits<int>::max());
    if (!mgr.findProductIndex(productId)) {
        std::cout << "  No product with id " << productId << ".\n";
        return;
    }
    std::cout << "  Type: 1) Stock in  2) Stock out\n";
    int typeChoice = readInt("  Choice: ", 1, 2);
    TransactionType type = (typeChoice == 1) ? TransactionType::StockIn : TransactionType::StockOut;
    int quantity = readNonNegativeInt("Quantity: ");
    std::string note = readLine("Note (optional): ");

    bool ok = mgr.recordTransaction(productId, type, quantity, currentTimestamp(), note);
    if (ok) {
        std::cout << "  Recorded.\n";
    } else {
        // Edge case: stock-out that would take quantity below zero.
        std::cout << "  Rejected: that would take quantity below zero.\n";
    }
}

void printMenu() {
    std::cout << "\n=== ToolHub ===\n"
               << " 1) Add product\n"
               << " 2) Edit product\n"
               << " 3) Remove product\n"
               << " 4) Search products\n"
               << " 5) Sort & list products\n"
               << " 6) Add supplier\n"
               << " 7) Record transaction\n"
               << " 8) Low-stock report\n"
               << " 9) Transaction history\n"
               << "10) List all products\n"
               << " 0) Save & exit\n";
}

} // namespace

int main() {
    InventoryManager manager;
    manager.load(DATA_FILE);

    std::cout << "ToolHub -- Smart Inventory Management System\n";
    std::cout << "Loaded " << manager.products().size() << " product(s), "
               << manager.suppliers().size() << " supplier(s), "
               << manager.transactionLog().size() << " transaction(s).\n";
    if (manager.malformedLinesSkipped() > 0) {
        std::cout << "  Note: " << manager.malformedLinesSkipped()
                   << " malformed line(s) in " << DATA_FILE
                   << " were skipped during load.\n";
    }

    bool running = true;
    while (running) {
        printMenu();
        int choice = readInt("Choice: ", 0, 10);
        switch (choice) {
            case 1:  actionAddProduct(manager); break;
            case 2:  actionEditProduct(manager); break;
            case 3:  actionRemoveProduct(manager); break;
            case 4:  actionSearch(manager); break;
            case 5:  actionSort(manager); break;
            case 6:  actionAddSupplier(manager); break;
            case 7:  actionRecordTransaction(manager); break;
            case 8:  printLowStockReport(manager); break;
            case 9:  printTransactionHistory(manager); break;
            case 10: printProductTable(manager.products()); break;
            case 0:  running = false; break;
        }
        if (running) {
            manager.save(DATA_FILE); // auto-save after every successful mutating action
        }
    }

    manager.save(DATA_FILE);
    std::cout << "Saved to " << DATA_FILE << ". Goodbye.\n";
    return 0;
}
