#ifndef SHOPTRACK_FILE_HANDLER_H
#define SHOPTRACK_FILE_HANDLER_H

#include "product.h"
#include "supplier.h"
#include "transaction_log.h"
#include <vector>
#include <string>

// Reads and writes inventory.dat. This is the only module that knows
// the on-disk format -- everything above it works with Product/Supplier/
// TransactionLog objects only.
namespace FileHandler {

// Loads products, suppliers, and transactions from `path`. A missing file
// is treated as a first run (returns true with empty containers). A
// malformed line is skipped rather than aborting the load -- the number
// skipped is written to malformedLinesSkipped so the caller can surface
// it. Also advances the next-id counters past the highest id seen, so
// new records don't collide with loaded ones.
bool loadFromFile(const std::string& path,
                   std::vector<Product>& products,
                   std::vector<Supplier>& suppliers,
                   TransactionLog& log,
                   int& nextProductId,
                   int& nextSupplierId,
                   int& nextTransactionId,
                   int& malformedLinesSkipped);

// Writes atomically: the new content is written to a temporary file in
// the same directory, then renamed over `path`. A failure partway
// through the write (e.g. disk full) leaves the original file untouched
// rather than a half-written, corrupted one -- std::rename is atomic on
// both POSIX and Windows when source and destination are on the same
// volume. Returns false if either step fails.
bool saveToFile(const std::string& path,
                 const std::vector<Product>& products,
                 const std::vector<Supplier>& suppliers,
                 const TransactionLog& log);

} // namespace FileHandler

#endif // SHOPTRACK_FILE_HANDLER_H
