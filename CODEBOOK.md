# ToolHub — Code Book

This document describes the data structures, memory model, and on-disk
file format in detail, as required alongside the README for Phase 2
reproducibility and transparency.

## Data structures

| Structure | Type | Why |
|---|---|---|
| Products | `std::vector<Product>` | Random access, search, sort, in-place edit — the pattern `std::vector` is built for |
| Suppliers | `std::vector<Supplier>` | Same access pattern as products, much smaller in practice |
| Product id → index | `std::unordered_map<int, size_t>` | O(1) lookup by id for edit/remove/find, avoiding a linear scan on every operation |
| Transaction history | `TransactionLog` (hand-built singly linked list, raw pointers) | Append-only, sequential-read access — chosen to demonstrate manual pointer/memory management deliberately, not because `std::vector` is inadequate for this access pattern (a vector would also work well here) |

## Memory model

- **Products and suppliers**: fully RAII-managed via `std::vector`. No
  manual allocation, no raw pointers, no owner ambiguity.
- **TransactionLog**: owns every `TransactionNode*` it allocates via
  `new`. The destructor walks the chain and calls `delete` on each
  node. Copy construction and copy assignment perform a **deep copy**
  (traversing the source list and re-appending each transaction) —
  a shallow copy of the raw `next` pointers would cause two
  `TransactionLog` instances to free the same nodes, a double-free,
  when both are destroyed. This is the rule-of-three implementation:
  destructor, copy constructor, copy assignment operator, all
  consistent with each other.
- **Relationships between entities** (Product → Supplier, Transaction →
  Product) are expressed as plain `int` ids, resolved at the point of
  use via the id→index map — not as pointers between structs. This
  avoids dangling references if a `std::vector` reallocates its
  internal buffer.

## File format: `inventory.dat`

Plain text, pipe-delimited, three labelled sections. Example:

```
[PRODUCTS]
1|Claw hammer|Tools|8.50|5.00|24|5|1
[SUPPLIERS]
1|Acme Hardware Ltd|acme@example.com
[TRANSACTIONS]
1|1|OUT|3|2026-09-10T14:02|Counter sale
```

**`[PRODUCTS]` field order:**
`id | name | category | sellingPrice | costPrice | quantity | reorderThreshold | supplierId`

**`[SUPPLIERS]` field order:**
`id | name | contact`

**`[TRANSACTIONS]` field order:**
`id | productId | type (IN/OUT) | quantity | timestamp | note`

Quantity in the transaction section is always **non-negative**; the
`type` field carries direction. This was changed from an earlier
signed-delta design specifically so the file is unambiguous to read by
eye and doesn't rely on a sign convention agreeing with the `type`
field (see "Changes from Phase 1" below).

**Parsing behaviour** (`file_handler.cpp`):
- A missing file is treated as a first run — the app starts with empty
  containers rather than failing.
- A line with the wrong number of fields for its section, or a field
  that fails to parse as a number, is **skipped**, not fatal — the
  rest of the file still loads. This is covered by
  `test_persistence_round_trip_and_malformed_line` in the test suite.
- Loading also advances the in-memory `next...Id` counters past the
  highest id seen in the file, so newly added records after a reload
  never collide with existing ids.
- Removing a product does **not** remove its historical transactions.
  They remain in `TransactionLog`, referencing a `productId` that may
  no longer resolve to a live product — a deliberate audit-trail
  decision, covered by
  `test_removed_product_keeps_historical_transactions`.

## Changes from the Phase 1 conception-phase proposal

Per the assignment's instruction to document any changes from the
conception-phase proposal:

1. **Supplier management scope reduced.** The Phase 1 document listed
   full supplier CRUD as baseline. The assignment brief's "at minimum"
   list does not include it — only products, transactions, search/sort,
   one report, and persistence are baseline — and lists supplier
   management explicitly as an optional extension. The data model still
   stores suppliers and links each product to one (`supplierId`), but
   only `addSupplier` is implemented; edit/remove supplier are not part
   of this implementation.

2. **Reorder-forecast report replaced with a low-stock report.** Phase 1
   proposed a forecast that estimated days-until-stockout from
   transaction velocity. This added implementation and testing
   complexity (defining "urgency," choosing a consumption window,
   handling insufficient history) disproportionate to its weight in
   the grading criteria. The implemented baseline report instead ranks
   products at or below their reorder threshold by shortfall
   (`reorderThreshold - quantity`), which is simpler, fully specified,
   and still ranks by urgency rather than a flat yes/no flag.

3. **Transaction quantity convention changed from a signed delta to
   `type` + non-negative `quantity`.** This matches the submitted
   Phase 1 document's file-format table and removes any ambiguity
   between a transaction's direction and its magnitude.

4. **`TransactionLog`'s justification reworded.** Phase 1 stated a
   `std::vector` "wouldn't fit" the transaction-history access pattern.
   That overstates the case — a vector would work fine for append-only,
   sequential-read data. The linked list is used because it deliberately
   demonstrates the module's pointer/memory-management competency, not
   because it's technically necessary.

5. **`Product` gained a `costPrice` field**, matching the Phase 1
   file-format table, which included a Cost column not present in the
   original entity description.

## Design decision: what `editProduct` covers

`editProduct` updates name, category, selling price, cost price, reorder
threshold, and supplier id — every field except **quantity**. This is
deliberate, not an oversight: quantity changes only through
`recordTransaction`, so `TransactionLog` remains the single source of
truth for every stock movement. If `editProduct` could also silently
change quantity, a product's stock level could drift from its
transaction history with no record of why, which would undermine the
audit-trail design decision documented above. To correct a wrong
quantity in practice, record a stock-in or stock-out transaction with
a note explaining the correction (e.g. "stocktake adjustment") — this
keeps the change visible in the transaction history report rather than
silent. Covered by `test_edit_product_updates_all_editable_fields` in
the test suite, which asserts every listed field updates correctly and
that quantity is unaffected.

## Phase 3 hardening: persistence and input validation

Per the assignment's finalization-phase instruction to "harden
persistence and input validation," three concrete changes were made
to the Phase 2 implementation:

1. **Atomic saves.** `FileHandler::saveToFile` previously wrote
   directly to `inventory.dat` with `std::ios::trunc`, which truncates
   the file *before* writing the new content. If the write failed
   partway through (disk full, power loss, the process being killed),
   the existing data would already be gone, replaced by a partial,
   corrupted file. It now writes the full new content to a temporary
   file (`inventory.dat.tmp`) first, and only replaces the original via
   `std::rename` once that write has fully succeeded and the stream
   reports no error. `std::rename` is atomic on both POSIX and Windows
   when source and destination share a volume, so the original file is
   never left in a half-written state — a failed save now fails
   cleanly, leaving the last good version intact, rather than silently
   destroying it.

2. **Supplier-id validation.** `addProduct` and `editProduct` previously
   accepted any non-negative integer as a product's `supplierId`,
   including ids that didn't correspond to any supplier that had
   actually been added. That let a product silently reference a
   supplier that doesn't exist, with no error and no way to notice
   short of manually cross-checking the file. Both methods now reject
   the operation (returning `-1` / `false`) unless the id is `0`
   ("no supplier") or matches a real, currently-known supplier, via the
   new `supplierExists()` check. The CLI's edit flow now also lists
   known suppliers before prompting, matching the add-product flow.

3. **Malformed-line visibility.** `loadFromFile` already skipped
   malformed lines rather than aborting the whole load, but did so
   silently — a corrupted file could lose data with no indication
   anything had gone wrong. It now counts every skipped line and
   reports the count via `InventoryManager::malformedLinesSkipped()`;
   the CLI prints a one-line warning on startup if that count is
   greater than zero, so data loss is visible rather than silent.

All three are covered by dedicated tests: `test_supplier_id_validation`
and `test_malformed_lines_are_counted`, alongside the existing
`test_persistence_round_trip_and_malformed_line`. The test suite now
totals 13 tests, up from 11 at the end of Phase 2.
