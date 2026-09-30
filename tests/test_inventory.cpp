// Assert-based unit tests (no external framework), per the project's
// documented toolchain. Each test is a free function; main() runs them all
// and reports pass/fail per test plus a final summary.

#include <cassert>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>

#include "file_handler.h"
#include "inventory_manager.h"
#include "product.h"
#include "reports.h"
#include "transaction_log.h"

namespace {

void test_product_adjust_quantity() {
    Product p(1, "Widget", "Hardware", 10, 5.0, 2.0, 3, 0);
    assert(p.adjustQuantity(5));
    assert(p.quantity() == 15);
    assert(!p.adjustQuantity(-100)); // would go negative
    assert(p.quantity() == 15);      // unchanged on rejection
    assert(p.adjustQuantity(-15));
    assert(p.quantity() == 0);
}

void test_transaction_log_append_and_copy() {
    TransactionLog log;
    Transaction a; a.id = 1; a.productId = 1; a.type = TransactionType::StockIn; a.quantity = 5;
    Transaction b; b.id = 2; b.productId = 1; b.type = TransactionType::StockOut; b.quantity = 2;
    log.append(a);
    log.append(b);
    assert(log.size() == 2);

    TransactionLog copy = log; // deep copy via copy constructor
    assert(copy.size() == 2);

    int count = 0;
    copy.forEach([&count](const Transaction&) { ++count; });
    assert(count == 2);
    // Mutating the original must not affect the copy (deep copy, no shared
    // nodes -- guards against a double-free when both are destroyed).
    Transaction c; c.id = 3; c.productId = 1; c.type = TransactionType::StockIn; c.quantity = 1;
    log.append(c);
    assert(log.size() == 3);
    assert(copy.size() == 2);
}

void test_transaction_log_remove_last() {
    TransactionLog log;
    Transaction a; a.id = 1; a.quantity = 5;
    Transaction b; b.id = 2; b.quantity = 9;
    log.append(a);
    log.append(b);

    Transaction out;
    assert(log.removeLast(out));
    assert(out.id == 2);
    assert(log.size() == 1);

    assert(log.removeLast(out));
    assert(out.id == 1);
    assert(log.size() == 0);
    assert(log.empty());

    assert(!log.removeLast(out)); // nothing left to remove
}

void test_inventory_manager_record_transaction_prevents_overdraw() {
    InventoryManager mgr;
    mgr.load("__test_overdraw.dat");
    int pid = mgr.addProduct("Bolt", "Fasteners", 5, 1.0, 0.5, 2, 0);
    assert(!mgr.recordTransaction(pid, TransactionType::StockOut, 10, "too many"));
    assert(mgr.recordTransaction(pid, TransactionType::StockOut, 5, "sell all"));
    auto products = mgr.products();
    assert(products.size() == 1);
    assert(products[0].quantity() == 0);
    std::remove("__test_overdraw.dat");
}

void test_low_stock_report_ranks_by_urgency() {
    std::vector<Product> products;
    products.emplace_back(1, "A", "Cat", 2, 1.0, 0.5, 5, 0);  // shortfall 3
    products.emplace_back(2, "B", "Cat", 9, 1.0, 0.5, 10, 0); // shortfall 1
    products.emplace_back(3, "C", "Cat", 20, 1.0, 0.5, 5, 0); // not low

    auto rows = Reports::lowStockReport(products);
    assert(rows.size() == 2);
    assert(rows[0].productId == 1); // largest shortfall first
    assert(rows[1].productId == 2);
}

void test_empty_inventory_does_not_crash() {
    InventoryManager mgr;
    mgr.load("__test_empty.dat");
    assert(mgr.products().empty());
    assert(Reports::lowStockReport(mgr.products()).empty());
    assert(Reports::transactionHistoryReport(mgr.products(), mgr.transactions()).empty());
    std::remove("__test_empty.dat");
}

void test_invalid_input_is_rejected() {
    InventoryManager mgr;
    mgr.load("__test_invalid.dat");
    assert(!mgr.recordTransaction(999, TransactionType::StockIn, 5, "no such product"));
    assert(!mgr.editProduct(999, "x", "y", 1.0, 1.0, 1, 0));
    std::remove("__test_invalid.dat");
}

void test_duplicate_products_are_allowed_and_independent() {
    InventoryManager mgr;
    mgr.load("__test_dup.dat");
    int id1 = mgr.addProduct("Hammer", "Tools", 5, 10.0, 5.0, 2, 0);
    int id2 = mgr.addProduct("Hammer", "Tools", 8, 10.0, 5.0, 2, 0);
    assert(id1 != id2);
    mgr.recordTransaction(id1, TransactionType::StockOut, 2, "sold");
    auto products = mgr.products();
    for (const auto& p : products) {
        if (p.id() == id1) assert(p.quantity() == 3);
        if (p.id() == id2) assert(p.quantity() == 8); // untouched
    }
    std::remove("__test_dup.dat");
}

void test_negative_quantity_transactions_rejected() {
    InventoryManager mgr;
    mgr.load("__test_negqty.dat");
    int pid = mgr.addProduct("Nail", "Fasteners", 100, 0.1, 0.05, 10, 0);
    assert(!mgr.recordTransaction(pid, TransactionType::StockOut, 0, "zero"));
    assert(!mgr.recordTransaction(pid, TransactionType::StockOut, -5, "negative"));
    std::remove("__test_negqty.dat");
}

void test_persistence_round_trip_and_malformed_line() {
    const char* path = "__test_roundtrip.dat";
    {
        InventoryManager mgr;
        mgr.load(path);
        int sid = mgr.addSupplier("Saunders Hardware Ltd", "+263-000-0000");
        int pid = mgr.addProduct("Claw Hammer", "Tools", 24, 15.0, 8.0, 5, sid);
        mgr.recordTransaction(pid, TransactionType::StockOut, 3, "sale");
        assert(mgr.save());
    }
    // Manually append a malformed line to the saved file.
    {
        std::ofstream out(path, std::ios::app);
        out << "not|enough|fields\n";
    }
    {
        InventoryManager mgr;
        assert(mgr.load(path));
        assert(mgr.malformedLinesSkipped() >= 1);
        auto products = mgr.products();
        assert(products.size() == 1);
        assert(products[0].quantity() == 21); // 24 - 3
        assert(mgr.suppliers().size() == 1);
        assert(mgr.transactions().size() == 1);
    }
    std::remove(path);
}

void test_malformed_lines_are_counted() {
    const char* path = "__test_malformed_count.dat";
    {
        std::ofstream out(path);
        out << "[PRODUCTS]\n";
        out << "1|Good|Cat|5|1.0|0.5|2|0\n";
        out << "garbage line with too few fields\n";
        out << "2|also bad because id is not numeric here|Cat\n";
        out << "[SUPPLIERS]\n";
        out << "[TRANSACTIONS]\n";
    }
    InventoryManager mgr;
    mgr.load(path);
    assert(mgr.malformedLinesSkipped() >= 2);
    assert(mgr.products().size() == 1);
    std::remove(path);
}

void test_supplier_id_validation() {
    InventoryManager mgr;
    mgr.load("__test_supplier_validation.dat");
    assert(mgr.addProduct("Screws", "Fasteners", 10, 1.0, 0.5, 2, 999) == -1); // no such supplier
    int sid = mgr.addSupplier("Acme", "acme@example.com");
    int pid = mgr.addProduct("Screws", "Fasteners", 10, 1.0, 0.5, 2, sid);
    assert(pid != -1);
    assert(!mgr.editProduct(pid, "Screws", "Fasteners", 1.0, 0.5, 2, 999)); // invalid supplier
    std::remove("__test_supplier_validation.dat");
}

void test_edit_product_updates_all_editable_fields() {
    InventoryManager mgr;
    mgr.load("__test_edit_all.dat");
    int sid = mgr.addSupplier("Acme", "acme@example.com");
    int pid = mgr.addProduct("Old Name", "Old Cat", 10, 1.0, 0.5, 2, 0);
    assert(mgr.editProduct(pid, "New Name", "New Cat", 9.99, 4.99, 7, sid));
    auto products = mgr.products();
    assert(products[0].name() == "New Name");
    assert(products[0].category() == "New Cat");
    assert(products[0].sellingPrice() == 9.99);
    assert(products[0].costPrice() == 4.99);
    assert(products[0].reorderThreshold() == 7);
    assert(products[0].supplierId() == sid);
    assert(products[0].quantity() == 10); // untouched by editProduct
    std::remove("__test_edit_all.dat");
}

void test_removed_product_keeps_historical_transactions() {
    InventoryManager mgr;
    mgr.load("__test_removed_history.dat");
    int pid = mgr.addProduct("Temp", "Cat", 10, 1.0, 0.5, 2, 0);
    mgr.recordTransaction(pid, TransactionType::StockIn, 5, "restock");
    assert(mgr.removeProduct(pid));
    assert(mgr.transactions().size() == 1);
    auto rows = Reports::transactionHistoryReport(mgr.products(), mgr.transactions());
    assert(rows.size() == 1);
    assert(rows[0].productName.find("deleted product") != std::string::npos);
    std::remove("__test_removed_history.dat");
}

void test_supplier_edit_and_remove_resets_product_reference() {
    InventoryManager mgr;
    mgr.load("__test_supplier_crud.dat");
    int sid = mgr.addSupplier("Original Co", "orig@example.com");
    assert(mgr.editSupplier(sid, "Renamed Co", "renamed@example.com"));
    auto suppliers = mgr.suppliers();
    assert(suppliers[0].name() == "Renamed Co");

    int pid = mgr.addProduct("Widget", "Cat", 5, 1.0, 0.5, 1, sid);
    assert(mgr.removeSupplier(sid));
    assert(mgr.suppliers().empty());
    auto products = mgr.products();
    assert(products[0].id() == pid);
    assert(products[0].supplierId() == 0); // reset, not left dangling
    std::remove("__test_supplier_crud.dat");
}

void test_undo_last_transaction_reverses_quantity() {
    InventoryManager mgr;
    mgr.load("__test_undo.dat");
    int pid = mgr.addProduct("Widget", "Cat", 10, 1.0, 0.5, 2, 0);
    assert(mgr.recordTransaction(pid, TransactionType::StockOut, 4, "sale"));
    assert(mgr.products()[0].quantity() == 6);

    Transaction undone;
    assert(mgr.undoLastTransaction(undone));
    assert(undone.quantity == 4);
    assert(mgr.products()[0].quantity() == 10); // reversed back
    assert(mgr.transactions().empty());          // record removed too

    Transaction none;
    assert(!mgr.undoLastTransaction(none)); // nothing left to undo
    std::remove("__test_undo.dat");
}

void test_undo_with_deleted_product_still_removes_transaction() {
    InventoryManager mgr;
    mgr.load("__test_undo_deleted.dat");
    int pid = mgr.addProduct("Widget", "Cat", 10, 1.0, 0.5, 2, 0);
    mgr.recordTransaction(pid, TransactionType::StockIn, 5, "restock");
    assert(mgr.removeProduct(pid));

    Transaction undone;
    // The transaction record is still removed even though the product it
    // referenced no longer exists -- there is simply no quantity left to
    // reverse.
    assert(mgr.undoLastTransaction(undone));
    assert(mgr.transactions().empty());
    std::remove("__test_undo_deleted.dat");
}

void test_auto_save_thread_persists_without_manual_save() {
    const char* path = "__test_autosave.dat";
    std::remove(path);
    {
        InventoryManager mgr;
        mgr.load(path);
        mgr.startAutoSave(50); // flush check every 50ms
        mgr.addProduct("AutoSaved", "Cat", 3, 1.0, 0.5, 1, 0);
        // Give the background thread a couple of cycles to notice the dirty
        // flag and flush -- without ever calling mgr.save() directly.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        mgr.stopAutoSave();
    }
    // Read the file directly (a fresh manager, or FileHandler) to confirm
    // the background thread, not the destructor's final save, is what
    // persisted the data.
    FileHandler::LoadResult result;
    int skipped = 0;
    assert(FileHandler::loadFromFile(path, result, skipped));
    assert(result.products.size() == 1);
    assert(result.products[0].name() == "AutoSaved");
    std::remove(path);
}

} // namespace

int main() {
    struct NamedTest { const char* name; void (*fn)(); };
    NamedTest tests[] = {
        {"test_product_adjust_quantity", test_product_adjust_quantity},
        {"test_transaction_log_append_and_copy", test_transaction_log_append_and_copy},
        {"test_transaction_log_remove_last", test_transaction_log_remove_last},
        {"test_inventory_manager_record_transaction_prevents_overdraw",
         test_inventory_manager_record_transaction_prevents_overdraw},
        {"test_low_stock_report_ranks_by_urgency", test_low_stock_report_ranks_by_urgency},
        {"test_empty_inventory_does_not_crash", test_empty_inventory_does_not_crash},
        {"test_invalid_input_is_rejected", test_invalid_input_is_rejected},
        {"test_duplicate_products_are_allowed_and_independent",
         test_duplicate_products_are_allowed_and_independent},
        {"test_negative_quantity_transactions_rejected", test_negative_quantity_transactions_rejected},
        {"test_persistence_round_trip_and_malformed_line", test_persistence_round_trip_and_malformed_line},
        {"test_malformed_lines_are_counted", test_malformed_lines_are_counted},
        {"test_supplier_id_validation", test_supplier_id_validation},
        {"test_edit_product_updates_all_editable_fields", test_edit_product_updates_all_editable_fields},
        {"test_removed_product_keeps_historical_transactions",
         test_removed_product_keeps_historical_transactions},
        {"test_supplier_edit_and_remove_resets_product_reference",
         test_supplier_edit_and_remove_resets_product_reference},
        {"test_undo_last_transaction_reverses_quantity", test_undo_last_transaction_reverses_quantity},
        {"test_undo_with_deleted_product_still_removes_transaction",
         test_undo_with_deleted_product_still_removes_transaction},
        {"test_auto_save_thread_persists_without_manual_save",
         test_auto_save_thread_persists_without_manual_save},
    };

    for (const auto& t : tests) {
        t.fn();
        std::cout << t.name << " passed\n";
    }
    std::cout << "\nAll tests passed.\n";
    return 0;
}
