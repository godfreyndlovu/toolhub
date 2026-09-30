#pragma once

#include <string>
#include <vector>
#include "product.h"
#include "supplier.h"
#include "transaction.h"

// Reads and writes the pipe-delimited flat-file persistence format described
// in CODEBOOK.md. Sections are bracketed headers: [PRODUCTS], [SUPPLIERS],
// [TRANSACTIONS].
namespace FileHandler {

struct LoadResult {
    std::vector<Product> products;
    std::vector<Supplier> suppliers;
    std::vector<Transaction> transactions;
};

// Loads from `path`. If the file does not exist, returns an empty
// LoadResult (this is a normal first-run state, not an error). Any line that
// cannot be parsed is skipped rather than aborting the load, and
// malformedLinesSkipped is incremented for each one.
bool loadFromFile(const std::string& path, LoadResult& out, int& malformedLinesSkipped);

// Writes atomically: the full content is written to `path + ".tmp"` first,
// and only if that write succeeds is the temp file renamed over `path` via
// std::rename. This means a crash or interruption during save can never
// leave a half-written, corrupted inventory.dat -- the original file is
// either left completely untouched, or completely replaced.
bool saveToFile(const std::string& path, const std::vector<Product>& products,
                  const std::vector<Supplier>& suppliers,
                  const std::vector<Transaction>& transactions);

} // namespace FileHandler
