# ToolHub

A C++17 command-line inventory management system for a small hardware
shop, built for the IU module *Project: General Programming with C/C++*
(DLBMINPAPCC01_E).

## What it does ❓

- Add / edit / remove products
- Record stock-in and stock-out transactions against a product
- Search products by name, sort by name/quantity/price
- Low-stock report, ranked by urgency (largest shortfall first)
- Transaction history report
- Add suppliers and link products to them
- Persists everything to a plain-text file (`inventory.dat`) that survives a restart

## Build ⚙️

Requires CMake 3.10+ and a C++17 compiler.

```bash
cmake -B build -S .
cmake --build build
```

This produces two executables:

- `build/toolhub` - the application
- `build/toolhub_tests` - the assert-based test suite

Run from the project root (it reads/writes `inventory.dat` in the
current working directory):

```bash
./build/toolhub
./build/toolhub_tests
```

## Using the CLI 🖥️

On launch, ToolHub loads `inventory.dat` if it exists (an empty/missing
file is treated as a first run) and shows a numbered menu:

```
 1  Add product             6  Add supplier
 2  Edit product             7  Record transaction
 3  Remove product           8  Low-stock report
 4  Search products          9  Transaction history
 5  Sort & list products    10  List all products
                              0  Save and exit
```

Every mutating action (add/edit/remove product, record transaction, add
supplier) is validated before being applied - see "Edge cases" below.
The application **auto-saves to `inventory.dat` after every menu
action** (not only on exit), so an unexpected interruption loses at
most the single in-progress action, not the whole session.

## Architecture 🧭

| Module | Responsibility |
|---|---|
| `product.h/.cpp` | `Product` - a single stocked item |
| `supplier.h/.cpp` | `Supplier` - id, name, contact |
| `transaction.h` | `Transaction` struct + `TransactionType` (StockIn/StockOut) |
| `transaction_log.h/.cpp` | `TransactionLog` - a hand-built singly linked list (raw pointers) holding the transaction history |
| `inventory_manager.h/.cpp` | `InventoryManager` - owns all domain data, coordinates products/suppliers/transactions/persistence, and validates input |
| `reports.h/.cpp` | Pure functions that compute the low-stock report and transaction history from data passed to them — no side effects, no direct access to `InventoryManager`'s stored state |
| `file_handler.h/.cpp` | Reads/writes `inventory.dat`; the only module that knows the on-disk format |
| `main.cpp` | CLI: reads input, calls `InventoryManager`/`Reports`, prints the result — no business logic here |
| `tests/test_inventory.cpp` | Assert-based unit tests, including edge cases |

## Data structure choices 💿

Products and suppliers are stored in `std::vector`, with an
`std::unordered_map<int, size_t>` id-to-index map for O(1) lookup -
matched to their random-access, search/sort/edit access pattern, and
fully RAII-managed (no manual allocation).

The transaction history is stored in a hand-built singly linked list
using raw pointers (`TransactionLog`), rather than `std::vector`. A
`std::vector` would in fact work perfectly well for this access pattern
too - the linked list was chosen deliberately to demonstrate manual
pointer ownership, allocation, and destruction (the module's core C/C++
memory-model competency), not because a vector is technically
inadequate. `TransactionLog` owns every node it allocates; its
destructor walks the list and frees each node, and copy
construction/assignment perform a deep copy to avoid a double-free
(rule of three).

## File format 📂

`inventory.dat` is a plain-text, pipe-delimited file with three sections:

```
[PRODUCTS]
id|name|category|sellingPrice|costPrice|quantity|reorderThreshold|supplierId

[SUPPLIERS]
id|name|contact

[TRANSACTIONS]
id|productId|type(IN/OUT)|quantity|timestamp|note
```

A missing file is treated as a first run (empty structures, no error).
A malformed line (wrong field count, or a field that fails to parse as
a number) is skipped, not fatal - the rest of the file still loads.
Loading also advances the internal id counters past the highest id
seen, so newly added records never collide with ones loaded from disk.

Removing a product does **not** delete its historical transactions —
they remain as an audit trail, referencing a product id that may no
longer resolve to a live product. This is a deliberate design decision,
not an oversight.

## Edge cases handled (and tested)📈

- **Empty inventory** - search, sort, both reports, and remove/edit all
  return cleanly (empty results / `false`) rather than crashing.
- **Invalid input** — `addProduct` rejects an empty name or any negative
  quantity/price/threshold (`-1` sentinel, nothing is stored);
  `recordTransaction` rejects a non-positive quantity and an unknown
  product id.
- **Duplicate items** - adding two products with identical
  name/category/etc. is allowed (a shop can legitimately stock two
  batches of the same item); each gets its own id, and a transaction
  against one never affects the other.
- **Negative quantities** - a stock-out that would take a product below
  zero is rejected and the quantity is left unchanged; `Product::adjustQuantity`
  itself also refuses to go negative, independent of the caller.
- **Malformed persisted data** - a corrupted line appended to
  `inventory.dat` is skipped on load; everything else still loads
  correctly. The number of skipped lines is counted and reported to the
  user on startup, so data loss is visible rather than silent.
- **Unknown supplier reference** - `addProduct`/`editProduct` reject a
  `supplierId` that isn't `0` ("no supplier") or an id belonging to an
  actual known supplier, rather than silently storing a dangling
  reference.
- **Interrupted save** - `inventory.dat` is written atomically: the new
  content goes to a temporary file first, which only replaces the real
  file once the write has fully succeeded. A failed write (disk full,
  crash mid-write) leaves the last good file intact instead of
  corrupting it.

All of the above are exercised in `tests/test_inventory.cpp`, run via
`build/toolhub_tests` (13 tests as of Phase 3).

## Changes from the conception-phase (Phase 1) proposal

- **Supplier management scope reduced.** Suppliers remain part of the
  data model (products link to a `supplierId`), but full supplier
  CRUD (edit/remove) was not implemented in the baseline - only
  `addSupplier` - since the assignment brief lists full supplier
  management as an optional extension, not baseline.
- **Reorder-forecast report replaced with a plain low-stock report.**
  The conception phase proposed projecting days-until-stockout from
  transaction velocity. That added scope and testing risk beyond what
  the brief requires (it asks for at least one *simple* report); the
  baseline report is now current-quantity-vs-threshold, ranked by
  shortfall.
- **Transaction convention changed from a signed delta to `Type` +
  unsigned `Quantity`.** The conception document's early drafts used a
  signed delta (e.g. `-3` for a sale); the final format uses an
  explicit `IN`/`OUT` type paired with a always-positive quantity,
  which is clearer to read in the persisted file and simpler to
  validate on input.
- **`Product` gained a `costPrice` field** alongside `sellingPrice`, to
  match the on-disk format.

## Scope 🔭

**Baseline (implemented):** product add/edit/remove, transactions that
update quantity, search, sort, low-stock report, transaction history
report, persistence across restarts, input validation.
