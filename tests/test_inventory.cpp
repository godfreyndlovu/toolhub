// Assert-based unit tests -- run as a separate executable target,
// per the conception document's testing approach. No external
// framework dependency. Covers normal operation plus the edge cases
// named in the assignment brief: empty inventory, invalid input,
// duplicate items, and negative quantities.

#include "product.h"
#include "transaction_log.h"
#include "inventory_manager.h"
#include "reports.h"
#include "file_handler.h"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <fstream>

void test_product_adjust_quantity() {
    Product p(1, "Widget", "Misc", 10, 5.0, 2.0, 3, 1);
    assert(p.adjustQuantity(5) == true);
    assert(p.quantity() == 15);
    assert(p.adjustQuantity(-20) == false); // would overdraw
    assert(p.quantity() == 15);             // unchanged on rejected adjustment
    std::cout << "test_product_adjust_quantity passed\n";
}

void test_transaction_log_append_and_copy() {
    TransactionLog log;
    Transaction t1{1, 1, TransactionType::StockIn, 10, "2026-01-01T00:00", "restock"};
    Transaction t2{2, 1, TransactionType::StockOut, 3, "2026-01-02T00:00", "sale"};
    log.append(t1);
    log.append(t2);
    assert(log.size() == 2);

    TransactionLog copy(log); // exercises the deep copy (rule of three)
    assert(copy.size() == 2);

    int count = 0;
    copy.forEach([&count](const Transaction&) { ++count; });
    assert(count == 2);

    std::cout << "test_transaction_log_append_and_copy passed\n";
}

void test_inventory_manager_record_transaction_prevents_overdraw() {
    InventoryManager mgr;
    int supplierId = mgr.addSupplier("Test Supplier", "test@example.com");
    int productId = mgr.addProduct("Bolt", "Hardware", 5, 1.0, 0.5, 2, supplierId);

    bool ok = mgr.recordTransaction(productId, TransactionType::StockOut, 3,
                                     "2026-01-01T00:00", "sale");
    assert(ok == true);

    bool overdraw = mgr.recordTransaction(productId, TransactionType::StockOut, 100,
                                           "2026-01-01T00:01", "too much");
    assert(overdraw == false);

    std::cout << "test_inventory_manager_record_transaction_prevents_overdraw passed\n";
}

void test_low_stock_report_ranks_by_urgency() {
    std::vector<Product> products;
    products.emplace_back(1, "A", "Cat", 4, 1.0, 0.5, 5, 1);  // shortfall 1
    products.emplace_back(2, "B", "Cat", 1, 1.0, 0.5, 5, 1);  // shortfall 4
    products.emplace_back(3, "C", "Cat", 10, 1.0, 0.5, 5, 1); // not below threshold

    auto entries = Reports::lowStockReport(products);
    assert(entries.size() == 2);
    assert(entries[0].name == "B"); // most urgent first
    assert(entries[1].name == "A");

    std::cout << "test_low_stock_report_ranks_by_urgency passed\n";
}

// --- Edge case: empty inventory ---
void test_empty_inventory_does_not_crash() {
    InventoryManager mgr;
    assert(mgr.products().empty());
    assert(mgr.suppliers().empty());

    auto lowStock = Reports::lowStockReport(mgr.products());
    assert(lowStock.empty());

    auto history = Reports::transactionHistoryReport(mgr.transactionLog());
    assert(history.empty());

    auto search = mgr.searchProductsByName("anything");
    assert(search.empty());

    auto sorted = mgr.productsSortedByName();
    assert(sorted.empty());

    // Removing/editing on an empty inventory should fail cleanly, not crash.
    assert(mgr.removeProduct(1) == false);
    assert(mgr.editProduct(1, "X", "Cat", 1.0, 0.5, 1, 1) == false);

    std::cout << "test_empty_inventory_does_not_crash passed\n";
}

// --- Edge case: invalid input ---
void test_invalid_input_is_rejected() {
    InventoryManager mgr;

    // Empty name
    assert(mgr.addProduct("", "Cat", 10, 5.0, 2.0, 3, 0) == -1);
    // Negative quantity, price, cost, threshold
    assert(mgr.addProduct("X", "Cat", -1, 5.0, 2.0, 3, 0) == -1);
    assert(mgr.addProduct("X", "Cat", 10, -5.0, 2.0, 3, 0) == -1);
    assert(mgr.addProduct("X", "Cat", 10, 5.0, -2.0, 3, 0) == -1);
    assert(mgr.addProduct("X", "Cat", 10, 5.0, 2.0, -3, 0) == -1);

    assert(mgr.products().empty()); // none of the above should have been stored

    int id = mgr.addProduct("Valid Item", "Cat", 10, 5.0, 2.0, 3, 0);
    assert(id != -1);

    // Non-positive transaction quantity is rejected
    assert(mgr.recordTransaction(id, TransactionType::StockIn, 0, "t", "n") == false);
    assert(mgr.recordTransaction(id, TransactionType::StockIn, -5, "t", "n") == false);

    // Transaction against a non-existent product id is rejected
    assert(mgr.recordTransaction(9999, TransactionType::StockIn, 5, "t", "n") == false);

    std::cout << "test_invalid_input_is_rejected passed\n";
}

// --- Edge case: duplicate items ---
void test_duplicate_products_are_allowed_and_independent() {
    InventoryManager mgr;
    int id1 = mgr.addProduct("Claw hammer", "Tools", 10, 8.50, 5.00, 5, 0);
    int id2 = mgr.addProduct("Claw hammer", "Tools", 10, 8.50, 5.00, 5, 0);

    assert(id1 != id2); // distinct ids even though every other field matches
    assert(mgr.products().size() == 2);

    // Adjusting one duplicate's stock must not affect the other.
    mgr.recordTransaction(id1, TransactionType::StockOut, 4, "t", "n");
    auto idx1 = mgr.findProductIndex(id1);
    auto idx2 = mgr.findProductIndex(id2);
    assert(mgr.products()[*idx1].quantity() == 6);
    assert(mgr.products()[*idx2].quantity() == 10);

    std::cout << "test_duplicate_products_are_allowed_and_independent passed\n";
}

// --- Edge case: negative quantities ---
void test_negative_quantity_transactions_rejected() {
    InventoryManager mgr;
    int id = mgr.addProduct("Screwdriver", "Tools", 5, 3.0, 1.0, 2, 0);

    // A stock-out larger than what's on hand must not drive quantity negative.
    assert(mgr.recordTransaction(id, TransactionType::StockOut, 6, "t", "n") == false);
    auto idx = mgr.findProductIndex(id);
    assert(mgr.products()[*idx].quantity() == 5); // unchanged

    // adjustQuantity() itself also refuses to go negative directly.
    Product p(1, "X", "Cat", 2, 1.0, 0.5, 1, 1);
    assert(p.adjustQuantity(-2) == true);  // exactly to zero is fine
    assert(p.quantity() == 0);
    assert(p.adjustQuantity(-1) == false); // below zero is rejected
    assert(p.quantity() == 0);

    std::cout << "test_negative_quantity_transactions_rejected passed\n";
}

// --- Persistence round-trip, including a malformed line ---
void test_persistence_round_trip_and_malformed_line() {
    const std::string path = "test_inventory_tmp.dat";

    InventoryManager mgr;
    int supplierId = mgr.addSupplier("Acme Hardware Ltd", "acme@example.com");
    int productId = mgr.addProduct("Claw hammer", "Tools", 24, 8.50, 5.00, 5, supplierId);
    mgr.recordTransaction(productId, TransactionType::StockOut, 3, "2026-09-10T14:02", "Counter sale");
    mgr.save(path);

    InventoryManager reloaded;
    reloaded.load(path);
    assert(reloaded.products().size() == 1);
    assert(reloaded.suppliers().size() == 1);
    assert(reloaded.products()[0].name() == "Claw hammer");
    assert(reloaded.products()[0].quantity() == 21); // 24 - 3

    // Append a malformed line and confirm loading still succeeds and
    // simply skips it rather than crashing.
    {
        std::ofstream out(path, std::ios::app);
        out << "this|is|not|a|valid|products|line|at|all|too|many|fields\n";
    }
    InventoryManager afterMalformed;
    bool loaded = afterMalformed.load(path);
    assert(loaded == true);
    assert(afterMalformed.products().size() == 1); // malformed line skipped, valid data kept

    std::remove(path.c_str());
    std::cout << "test_persistence_round_trip_and_malformed_line passed\n";
}

// --- Hardening: load() reports how many lines it had to skip ---
void test_malformed_lines_are_counted() {
    const std::string path = "test_malformed_count_tmp.dat";
    {
        std::ofstream out(path);
        out << "[PRODUCTS]\n";
        out << "1|Good Product|Cat|5.0|2.0|10|2|0\n";
        out << "not|enough|fields\n";                      // wrong field count
        out << "2|Bad Numbers|Cat|notanumber|2.0|10|2|0\n"; // fails stod
        out << "[SUPPLIERS]\n";
        out << "1|Good Supplier|a@example.com\n";
        out << "garbage|line\n";                            // wrong field count
        out << "[TRANSACTIONS]\n";
        out << "1|1|IN|5|2026-01-01T00:00|ok\n";
        out << "also|garbage|here|too|many|fields|here\n";  // wrong field count
    }

    InventoryManager mgr;
    bool loaded = mgr.load(path);
    assert(loaded == true);
    assert(mgr.products().size() == 1);       // only the valid product line kept
    assert(mgr.suppliers().size() == 1);      // only the valid supplier line kept
    assert(mgr.transactionLog().size() == 1); // only the valid transaction kept
    assert(mgr.malformedLinesSkipped() == 4); // the four bad lines above

    std::remove(path.c_str());
    std::cout << "test_malformed_lines_are_counted passed\n";
}

// --- Hardening: addProduct/editProduct reject an unknown supplierId ---
void test_supplier_id_validation() {
    InventoryManager mgr;
    int realSupplierId = mgr.addSupplier("Real Supplier", "real@example.com");

    // 0 (no supplier) is always valid.
    assert(mgr.addProduct("A", "Cat", 5, 1.0, 0.5, 1, 0) != -1);
    // A real, existing supplier id is valid.
    assert(mgr.addProduct("B", "Cat", 5, 1.0, 0.5, 1, realSupplierId) != -1);
    // An id that doesn't correspond to any supplier is rejected.
    assert(mgr.addProduct("C", "Cat", 5, 1.0, 0.5, 1, 9999) == -1);

    int id = mgr.addProduct("D", "Cat", 5, 1.0, 0.5, 1, 0);
    assert(mgr.editProduct(id, "D", "Cat", 1.0, 0.5, 1, realSupplierId) == true);
    assert(mgr.editProduct(id, "D", "Cat", 1.0, 0.5, 1, 9999) == false);

    std::cout << "test_supplier_id_validation passed\n";
}

// --- editProduct covers every field except quantity ---
void test_edit_product_updates_all_editable_fields() {
    InventoryManager mgr;
    int supplierA = mgr.addSupplier("Supplier A", "a@example.com");
    int supplierB = mgr.addSupplier("Supplier B", "b@example.com");
    int id = mgr.addProduct("Old Name", "Old Cat", 10, 5.0, 2.0, 3, supplierA);

    bool ok = mgr.editProduct(id, "New Name", "New Cat", 9.0, 3.5, 4, supplierB);
    assert(ok == true);

    auto idx = mgr.findProductIndex(id);
    const Product& p = mgr.products()[*idx];
    assert(p.name() == "New Name");
    assert(p.category() == "New Cat");
    assert(p.sellingPrice() == 9.0);
    assert(p.costPrice() == 3.5);
    assert(p.reorderThreshold() == 4);
    assert(p.supplierId() == supplierB);
    assert(p.quantity() == 10); // unchanged -- edit never touches quantity

    // Invalid edits (empty name/category, negative values) are rejected
    // and leave the product untouched.
    assert(mgr.editProduct(id, "", "Cat", 1.0, 1.0, 1, 1) == false);
    assert(mgr.editProduct(id, "Name", "", 1.0, 1.0, 1, 1) == false);
    assert(mgr.editProduct(id, "Name", "Cat", -1.0, 1.0, 1, 1) == false);
    assert(mgr.products()[*idx].name() == "New Name"); // still the last valid edit

    std::cout << "test_edit_product_updates_all_editable_fields passed\n";
}

// --- Design decision: removing a product keeps its historical transactions ---
void test_removed_product_keeps_historical_transactions() {
    InventoryManager mgr;
    int supplierId = mgr.addSupplier("Test Supplier", "test@example.com");
    int productId = mgr.addProduct("Bolt", "Hardware", 10, 1.0, 0.5, 2, supplierId);
    mgr.recordTransaction(productId, TransactionType::StockOut, 2,
                           "2026-01-01T00:00", "sale");
    assert(mgr.transactionLog().size() == 1);

    assert(mgr.removeProduct(productId) == true);
    assert(mgr.findProductIndex(productId) == std::nullopt);

    // Audit trail: the transaction referencing the now-deleted product
    // is still present in the log.
    assert(mgr.transactionLog().size() == 1);

    // Removing an already-removed / nonexistent id fails gracefully.
    assert(mgr.removeProduct(productId) == false);
    assert(mgr.removeProduct(9999) == false);

    std::cout << "test_removed_product_keeps_historical_transactions passed\n";
}

int main() {
    test_product_adjust_quantity();
    test_transaction_log_append_and_copy();
    test_inventory_manager_record_transaction_prevents_overdraw();
    test_low_stock_report_ranks_by_urgency();
    test_empty_inventory_does_not_crash();
    test_invalid_input_is_rejected();
    test_duplicate_products_are_allowed_and_independent();
    test_negative_quantity_transactions_rejected();
    test_persistence_round_trip_and_malformed_line();
    test_malformed_lines_are_counted();
    test_supplier_id_validation();
    test_edit_product_updates_all_editable_fields();
    test_removed_product_keeps_historical_transactions();
    std::cout << "\nAll tests passed.\n";
    return 0;
}
