# CODEBOOK

Technical reference for ToolHub's data structures, memory model, file
format, and the design decisions behind them. Read alongside README.md
(build/run/menu) and `docs/class_diagram.png` / `docs/sequence_diagram.png`.

## Data structures

| Structure | Type | Why |
|---|---|---|
| Products | `std::vector<Product>` + `unordered_map<int,size_t>` id-index (conceptually; current implementation does a linear scan, documented below) | Random-access, search/sort/edit pattern; RAII-managed, no manual allocation |
| Suppliers | `std::vector<Supplier>` | Same access pattern, smaller collection |
| Transaction history | Hand-built singly linked list (`TransactionLog`), raw pointers | See "Why a linked list" below |

Note on lookup: `InventoryManager` currently performs a linear scan
(`findProductIndexLocked` / `findSupplierIndexLocked`) rather than
maintaining a separate `unordered_map<int,size_t>` id-index. For the data
volumes this project targets (a small shop's catalogue), this is O(n) but
fast in practice; the map-based index remains a documented, straightforward
future optimisation if the collection size ever became large enough to
matter, without changing any public method signature.

## Why a linked list for TransactionLog

Transaction history is append-only and read sequentially -- a pattern
`std::vector` would also serve well, and would in fact be simpler and
marginally more cache-efficient. The linked list is chosen **specifically**
to exercise dynamic memory allocation, pointer traversal, ownership, and
manual destruction in C++ -- the module's core memory-model competency --
rather than because it is technically necessary. This is a deliberate
demonstration, not a claim that a vector "wouldn't fit."

`TransactionLog` owns every node it allocates via `new`:
- **Destructor** walks the chain and `delete`s each node.
- **Copy constructor / copy assignment** deep-copy the chain (rule of
  three), so two independently destroyed logs never double-free a shared
  node.
- **`append`** is O(1) via a tracked tail pointer.
- **`removeLast`** (added to support undo) is O(n) -- a singly linked list
  has no way to reach the second-to-last node without a walk. This is
  acceptable because undo is a rare, single-shot operation, not a hot path.

Verified by `test_transaction_log_append_and_copy` and
`test_transaction_log_remove_last`.

## Relationships

- `Product.supplierId` is an integer id referencing a `Supplier`, not a raw
  pointer between structs. This avoids dangling references if
  `std::vector<Product>`/`std::vector<Supplier>` reallocates its internal
  buffer (which invalidates pointers and iterators but never invalidates an
  id). A `supplierId` of 0 means "no supplier."
- `Transaction.productId` is likewise an integer id, not a pointer, for the
  same reason, and also so that a transaction can continue to reference a
  product that has since been removed (see "Design decision: deletion and
  audit trail").
- Cardinalities (see `docs/class_diagram.png`): one `Supplier` to many
  `Product`s; one `Product` to many `Transaction`s; `InventoryManager`
  composes (owns) its `Product`/`Supplier` collections and its single
  `TransactionLog`.

## Design decision: what `editProduct` covers

`editProduct` updates name, category, sellingPrice, costPrice,
reorderThreshold, and supplierId -- every editable field **except**
quantity. Quantity changes exclusively through `recordTransaction` (and its
inverse, `undoLastTransaction`), so every quantity change is always
accompanied by a `Transaction` record. This is deliberate: allowing
`editProduct` to silently change quantity would create stock movements with
no corresponding audit-trail entry, which defeats the purpose of keeping a
transaction history at all.

## Design decision: deletion and audit trail

Removing a product does not delete its historical transactions -- they
remain as an audit trail. `Transaction.productId` continues to reference the
now-nonexistent product id; `Reports::transactionHistoryReport` displays
these rows using a `(deleted product #id)` placeholder name rather than
omitting them or crashing. Verified by
`test_removed_product_keeps_historical_transactions`.

The same principle applies to suppliers: removing a supplier does not
remove or orphan the products that reference it. Instead, every product
whose `supplierId` matched the removed supplier is reset to 0 ("no
supplier"), so no product is ever left pointing at a supplier id that no
longer exists. Verified by
`test_supplier_edit_and_remove_resets_product_reference`.

## Design decision: undo is single-level

`undoLastTransaction` reverses only the single most recent transaction, not
an arbitrary depth of history. This is a deliberate scope decision: a full
undo/redo stack would require either replaying the entire transaction log to
reconstruct intermediate states, or storing per-transaction snapshots,
neither of which is justified by the assignment's requirements. Single-level
undo directly answers "I made a mistake, reverse it" -- the common case --
without that added complexity. If the referenced product has since been
removed, the transaction record is still removed (undo always succeeds when
there is a transaction to undo); there is simply no quantity left to
reverse. Verified by `test_undo_last_transaction_reverses_quantity` and
`test_undo_with_deleted_product_still_removes_transaction`.

## Scope extension: responding to conception-phase review

The Phase 1 conception document was reviewed and found too narrow in scope
for the assignment's expectations -- the original baseline kept supplier
management, undo, and any concurrency mechanism as merely optional
extensions, in the interest of protecting implementation quality. In
response, three items were promoted from "optional" to "implemented
baseline":

1. **Full supplier management** (edit/remove, not just add) -- cheap to
   complete once add existed, and closes an otherwise-incomplete CRUD
   surface.
2. **Undo-last-transaction** -- reuses `TransactionLog`'s existing
   traversal machinery, genuinely useful, and demonstrates careful state
   reversal (see above).
3. **A background auto-save thread** using `std::thread`, `std::mutex`, and
   `std::atomic<bool>` (see below) -- the most substantial addition,
   directly named in the assignment's own list of optional extensions, and
   the one most likely to demonstrate advanced C++ concepts beyond the
   baseline CRUD-and-reports shape.

This keeps the "small scope + excellent implementation" principle intact
(all 18 tests still pass, zero compiler warnings) while giving the project
enough substance that it is no longer reasonably describable as too narrow.

## Background auto-save: concurrency design

`InventoryManager` owns:
- `std::mutex mutex_` -- guards `products_`, `suppliers_`, and
  `transactionLog_`. Every public method takes this lock for its duration.
- `std::atomic<bool> dirty_` -- set to `true` by every mutating operation
  (`markDirtyLocked()`), and atomically read-and-cleared
  (`dirty_.exchange(false)`) by the background thread each time it wakes.
- `std::atomic<bool> autoSaveRunning_` and `std::thread autoSaveThread_` --
  the thread's lifecycle.

`startAutoSave(intervalMs)` launches a thread running `autoSaveLoop`: it
sleeps for `intervalMs`, then checks the dirty flag. If set, it takes
`mutex_` and calls the same `saveLocked()` used by the explicit `save()`
path, so the file format and the atomic-write guarantee (see below) are
identical whether the save was triggered manually or by the background
thread. `stopAutoSave()` (called both by "Save & exit" and by the
destructor) stops the loop, joins the thread, and performs one final
synchronous save if anything is still unflushed -- so no work is ever lost
even if the process exits immediately after a mutation.

This design deliberately keeps the locking coarse-grained (a single mutex
over all inventory state) rather than fine-grained per-collection locking:
the operation volumes here do not justify the added complexity and risk of
a more elaborate locking scheme, and coarse-grained locking is easier to
reason about correctly -- which matters more for a portfolio project
demonstrating correct concurrency than for one demonstrating maximum
throughput. Verified by
`test_auto_save_thread_persists_without_manual_save`, which starts the
thread, mutates state, and confirms (by reading the file directly, without
ever calling `save()`) that the background thread alone persisted the
change.

## File format

Pipe-delimited flat file with three bracketed sections:

```
[PRODUCTS]
id|name|category|quantity|sellingPrice|costPrice|reorderThreshold|supplierId
[SUPPLIERS]
id|name|contact
[TRANSACTIONS]
id|productId|IN|OUT|quantity|timestamp|note
```

`Transaction.quantity` is always a non-negative magnitude; direction is
carried by the `IN`/`OUT` token, not by quantity's sign. This matches the
documented Product definition and avoids the earlier Phase 1 inconsistency
between a signed-delta convention in one section and a magnitude-plus-type
convention in the file-format table.

## Persistence: atomic writes

`FileHandler::saveToFile` writes the full content to `path + ".tmp"` first,
checks the stream is still `good()`, and only then uses `std::rename` to
atomically replace the original file. `std::rename` on the same filesystem
is atomic at the OS level -- a crash or interruption during the write can
never leave `inventory.dat` half-written or corrupted; the original file is
either left completely untouched (if the temp write failed) or completely
replaced (if it succeeded). On a failed write, the temp file is removed.

## Persistence: malformed-line handling

`FileHandler::loadFromFile` parses each line inside its current section and
increments `malformedLinesSkipped` (returned by reference) for any line that
does not parse -- wrong field count, or a field that fails numeric
conversion -- rather than aborting the load. `main.cpp` prints a warning at
startup if this count is greater than zero. Verified by
`test_malformed_lines_are_counted` and
`test_persistence_round_trip_and_malformed_line`.

## Toolchain and testing

C++17, CMake 3.10+, GCC/Clang/MSVC, Git/GitHub. Assert-based unit tests (no
external framework) in `tests/test_inventory.cpp`: 18 tests covering
product operations, supplier CRUD, transaction processing, undo,
persistence (round-trip, malformed lines, atomic write), validation, and
the background auto-save thread. All pass; the project builds with zero
warnings under `-Wall -Wextra`.

## References

- Hunt, A., & Thomas, D. (2019). *The Pragmatic Programmer* (20th
  Anniversary ed.). Addison-Wesley.
- Anggoro, W. (2018). *C++ Data Structures and Algorithms.* Packt
  Publishing.
- ISO/IEC. (2017). *ISO/IEC 14882:2017 -- Programming languages: C++.*
  International Organization for Standardization.
- cppreference.com. (n.d.). *std::thread, std::mutex, std::atomic.*
  Retrieved from https://en.cppreference.com/
