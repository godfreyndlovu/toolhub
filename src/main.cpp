// ToolHub -- Smart Inventory Management System
// Godfrey Ndlovu -- Matriculation No. 92131458
// Project: General Programming with C/C++ (DLBMINPAPCC01_E)
//
// This file is the sole entry point through which the user manages the
// system: every operation the assignment requires (add/edit/remove
// products, record stock movements, search/sort, reporting, supplier
// management, undo) is reached exclusively through this interactive CLI
// menu. There is no other interface -- no GUI, no scripting layer -- so the
// CLI is not an incidental wrapper around the "real" program but the
// user-facing design itself.

#include <iostream>
#include <limits>
#include <string>

#include "inventory_manager.h"
#include "reports.h"

namespace {

const char* kSavePath = "inventory.dat";

std::string readLine() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

std::string readNonEmptyLine(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line = readLine();
        if (!line.empty()) return line;
        std::cout << "  Value cannot be empty. Try again.\n";
    }
}

void clearBadInput() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int readInt(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        int value;
        if (std::cin >> value) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        std::cout << "  Please enter a whole number.\n";
        clearBadInput();
    }
}

int readNonNegativeInt(const std::string& prompt) {
    while (true) {
        int value = readInt(prompt);
        if (value >= 0) return value;
        std::cout << "  Value cannot be negative.\n";
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
        std::cout << "  Please enter a non-negative number.\n";
        clearBadInput();
    }
}

bool readYesNo(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (y/n): ";
        std::string line = readLine();
        if (!line.empty() && (line[0] == 'y' || line[0] == 'Y')) return true;
        if (!line.empty() && (line[0] == 'n' || line[0] == 'N')) return false;
        std::cout << "  Please answer y or n.\n";
    }
}

void printSuppliers(InventoryManager& mgr) {
    auto suppliers = mgr.suppliers();
    if (suppliers.empty()) {
        std::cout << "  (no suppliers on record)\n";
        return;
    }
    std::cout << "  Known suppliers:\n";
    for (const auto& s : suppliers) {
        std::cout << "    " << s.id() << ": " << s.name() << " (" << s.contact() << ")\n";
    }
}

int readSupplierIdChoice(InventoryManager& mgr) {
    printSuppliers(mgr);
    while (true) {
        int id = readNonNegativeInt("Supplier id (0 if none): ");
        if (mgr.supplierExists(id)) return id;
        std::cout << "  No supplier with that id. Try again.\n";
    }
}

void actionAddProduct(InventoryManager& mgr) {
    std::string name = readNonEmptyLine("Product name: ");
    if (mgr.productNameExists(name)) {
        if (!readYesNo("  A product with this name already exists. Add another with the same name?")) {
            std::cout << "  Cancelled.\n";
            return;
        }
    }
    std::string category = readNonEmptyLine("Category: ");
    int quantity = readNonNegativeInt("Initial quantity: ");
    double sellingPrice = readNonNegativeDouble("Selling price: ");
    double costPrice = readNonNegativeDouble("Cost price: ");
    int reorderThreshold = readNonNegativeInt("Reorder threshold: ");
    int supplierId = readSupplierIdChoice(mgr);

    int id = mgr.addProduct(name, category, quantity, sellingPrice, costPrice,
                              reorderThreshold, supplierId);
    std::cout << "  Added product #" << id << ".\n";
}

void actionEditProduct(InventoryManager& mgr) {
    int id = readNonNegativeInt("Product id to edit: ");
    std::string name = readNonEmptyLine("New name: ");
    std::string category = readNonEmptyLine("New category: ");
    double sellingPrice = readNonNegativeDouble("New selling price: ");
    double costPrice = readNonNegativeDouble("New cost price: ");
    int reorderThreshold = readNonNegativeInt("New reorder threshold: ");
    int supplierId = readSupplierIdChoice(mgr);

    bool ok = mgr.editProduct(id, name, category, sellingPrice, costPrice,
                                reorderThreshold, supplierId);
    std::cout << (ok ? "  Product updated.\n" : "  No product with that id (or invalid supplier).\n");
    std::cout << "  Note: quantity is not editable here -- use 'Record transaction' to change stock.\n";
}

void actionRemoveProduct(InventoryManager& mgr) {
    int id = readNonNegativeInt("Product id to remove: ");
    bool ok = mgr.removeProduct(id);
    std::cout << (ok ? "  Product removed. Its historical transactions are kept as an audit trail.\n"
                       : "  No product with that id.\n");
}

void printProductTable(const std::vector<Product>& products) {
    if (products.empty()) {
        std::cout << "  (no products)\n";
        return;
    }
    std::cout << "ID   Name                 Category       Qty   Sell    Cost   Reorder  Supplier\n";
    for (const auto& p : products) {
        std::cout << p.id() << "    " << p.name() << "  " << p.category() << "  "
                   << p.quantity() << "  " << p.sellingPrice() << "  " << p.costPrice()
                   << "  " << p.reorderThreshold() << "  " << p.supplierId() << "\n";
    }
}

void actionSearch(InventoryManager& mgr) {
    std::string query = readNonEmptyLine("Search text: ");
    printProductTable(mgr.searchByName(query));
}

void actionSortAndList(InventoryManager& mgr) {
    std::cout << "Sort by: 1) Name  2) Quantity  3) Price\n";
    int choice = readNonNegativeInt("Choice: ");
    std::vector<Product> result;
    switch (choice) {
        case 1: result = mgr.sortedByName(); break;
        case 2: result = mgr.sortedByQuantity(); break;
        case 3: result = mgr.sortedByPrice(); break;
        default: std::cout << "  Unknown option, showing unsorted list.\n"; result = mgr.products(); break;
    }
    printProductTable(result);
}

void actionAddSupplier(InventoryManager& mgr) {
    std::string name = readNonEmptyLine("Supplier name: ");
    std::string contact = readNonEmptyLine("Contact info: ");
    int id = mgr.addSupplier(name, contact);
    std::cout << "  Added supplier #" << id << ".\n";
}

void actionEditSupplier(InventoryManager& mgr) {
    printSuppliers(mgr);
    int id = readNonNegativeInt("Supplier id to edit: ");
    std::string name = readNonEmptyLine("New name: ");
    std::string contact = readNonEmptyLine("New contact info: ");
    bool ok = mgr.editSupplier(id, name, contact);
    std::cout << (ok ? "  Supplier updated.\n" : "  No supplier with that id.\n");
}

void actionRemoveSupplier(InventoryManager& mgr) {
    printSuppliers(mgr);
    int id = readNonNegativeInt("Supplier id to remove: ");
    bool ok = mgr.removeSupplier(id);
    std::cout << (ok ? "  Supplier removed. Any products that referenced it now show 'no supplier'.\n"
                       : "  No supplier with that id.\n");
}

void actionRecordTransaction(InventoryManager& mgr) {
    int productId = readNonNegativeInt("Product id: ");
    std::cout << "Type: 1) Stock in  2) Stock out\n";
    int typeChoice = readNonNegativeInt("Choice: ");
    TransactionType type = (typeChoice == 1) ? TransactionType::StockIn : TransactionType::StockOut;
    int quantity = readNonNegativeInt("Quantity: ");
    std::string note = readNonEmptyLine("Note: ");

    bool ok = mgr.recordTransaction(productId, type, quantity, note);
    std::cout << (ok ? "  Transaction recorded.\n"
                       : "  Rejected: no such product, quantity <= 0, or stock-out would go negative.\n");
}

void actionUndoLastTransaction(InventoryManager& mgr) {
    Transaction undone;
    if (!mgr.undoLastTransaction(undone)) {
        std::cout << "  No transaction to undo.\n";
        return;
    }
    std::cout << "  Undone: transaction #" << undone.id << " on product #" << undone.productId
               << " (" << (undone.type == TransactionType::StockIn ? "IN" : "OUT") << " "
               << undone.quantity << "). This reverses only the single most recent transaction.\n";
}

void actionLowStockReport(InventoryManager& mgr) {
    auto rows = Reports::lowStockReport(mgr.products());
    if (rows.empty()) {
        std::cout << "  No products at or below their reorder threshold.\n";
        return;
    }
    std::cout << "Product              Qty     Reorder at  Shortfall\n";
    for (const auto& r : rows) {
        std::cout << r.name << "  " << r.quantity << "  " << r.reorderThreshold << "  "
                   << r.shortfall << "\n";
    }
}

void actionTransactionHistory(InventoryManager& mgr) {
    auto rows = Reports::transactionHistoryReport(mgr.products(), mgr.transactions());
    if (rows.empty()) {
        std::cout << "  No transactions recorded yet.\n";
        return;
    }
    std::cout << "ID   Product              Type  Qty   Timestamp            Note\n";
    for (const auto& r : rows) {
        std::cout << r.transactionId << "  " << r.productName << "  "
                   << (r.type == TransactionType::StockIn ? "IN " : "OUT") << "  " << r.quantity
                   << "  " << r.timestamp << "  " << r.note << "\n";
    }
}

void printMenu() {
    std::cout << "\n=== ToolHub ===\n"
                  " 1) Add product\n"
                  " 2) Edit product\n"
                  " 3) Remove product\n"
                  " 4) Search products\n"
                  " 5) Sort & list products\n"
                  " 6) Add supplier\n"
                  " 7) Edit supplier\n"
                  " 8) Remove supplier\n"
                  " 9) Record transaction\n"
                  "10) Undo last transaction\n"
                  "11) Low-stock report\n"
                  "12) Transaction history\n"
                  "13) List all products\n"
                  " 0) Save & exit\n"
                  "Choice: ";
}

} // namespace

int main() {
    std::cout << "ToolHub -- Smart Inventory Management System\n";

    InventoryManager mgr;
    mgr.load(kSavePath);
    std::cout << "Loaded " << mgr.products().size() << " product(s), "
               << mgr.suppliers().size() << " supplier(s), "
               << mgr.transactions().size() << " transaction(s).\n";
    if (mgr.malformedLinesSkipped() > 0) {
        std::cout << "Warning: " << mgr.malformedLinesSkipped()
                   << " malformed line(s) in inventory.dat were skipped.\n";
    }

    // Background auto-save: a dirty flag is set (atomically) by every
    // mutating InventoryManager call; this thread wakes periodically,
    // checks and clears that flag, and flushes to disk under the same
    // mutex the main thread uses -- so an operator who forgets to choose
    // "Save & exit" still has their work persisted within a few seconds.
    mgr.startAutoSave();

    bool running = true;
    while (running) {
        printMenu();
        int choice;
        if (!(std::cin >> choice)) {
            clearBadInput();
            std::cout << "  Please enter a number.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
            case 1: actionAddProduct(mgr); break;
            case 2: actionEditProduct(mgr); break;
            case 3: actionRemoveProduct(mgr); break;
            case 4: actionSearch(mgr); break;
            case 5: actionSortAndList(mgr); break;
            case 6: actionAddSupplier(mgr); break;
            case 7: actionEditSupplier(mgr); break;
            case 8: actionRemoveSupplier(mgr); break;
            case 9: actionRecordTransaction(mgr); break;
            case 10: actionUndoLastTransaction(mgr); break;
            case 11: actionLowStockReport(mgr); break;
            case 12: actionTransactionHistory(mgr); break;
            case 13: printProductTable(mgr.products()); break;
            case 0:
                mgr.stopAutoSave();
                mgr.save();
                std::cout << "Saved to inventory.dat. Goodbye.\n";
                running = false;
                break;
            default:
                std::cout << "  Unknown option (0-13 only).\n";
                break;
        }
    }
    return 0;
}
