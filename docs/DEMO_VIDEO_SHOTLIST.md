# Demonstration Video -- Shot List

Required per the assignment: "A short demonstration video showing the
application in use (including data persisting after a restart) is
required." Aim for 4-7 minutes, screen recording with narration (or
on-screen captions). Record after rebuilding from the corrected code.

## 1. Build from scratch (reproducibility)

- Show a clean clone or a `build/` folder deleted.
- Run: `cmake -B build -S .` then `cmake --build build`.
- Narrate: "C++17, CMake 3.10+, no other dependencies."
- Show the build finishing with no errors.

## 2. Run the test suite

- Run: `./build/toolhub_tests` (or `.\build\Debug\toolhub_tests.exe` on
  Windows).
- Let all 18 tests scroll and show "All tests passed."
- Narrate: "18 unit tests covering CRUD, undo, persistence, validation, and
  the background auto-save thread."

## 3. First run -- empty state

- Run `./build/toolhub`.
- Show "Loaded 0 product(s), 0 supplier(s), 0 transaction(s)." and the
  13-option menu.
- Narrate that no seed data is auto-generated -- this is a genuinely empty
  first run.

## 4. Enter the seed data (add supplier, add products)

- Option 6: add supplier "Saunders Hardware Ltd".
- Option 1: add product "Claw Hammer" (qty 24, sell 15, cost 8, reorder 5,
  supplier 1).
- Option 1: add product "Screws" (qty 2, sell 3, cost 1, reorder 5,
  supplier 1).

## 5. Record a transaction

- Option 9: stock-out 3 units of Claw Hammer, note "sale".
- Show "Transaction recorded."

## 6. Demonstrate undo (new feature)

- Option 10: undo last transaction.
- Show the quantity reverting to 24 and the confirmation message.
- Narrate: single-level undo, reverses both the quantity effect and the
  transaction record.
- Re-record the same stock-out afterward so the seed data matches the
  written documentation (qty 21) for the rest of the demo.

## 7. Demonstrate supplier edit/remove (new feature)

- Option 7: edit the supplier's contact info, show it updated.
- Briefly show option 8 (remove supplier) on a *throwaway* second supplier
  you add just for this shot -- not the real seed supplier -- then show
  option 13 (list products) confirming any product that referenced it now
  shows supplier 0 ("no supplier"), not a dangling id.

## 8. Low-stock report and edge cases

- Option 11: low-stock report -- show Screws flagged with shortfall 3.
- Trigger one edge case on camera: try an invalid menu choice (e.g. 99) and
  show it re-prompts instead of crashing.
- Try a stock-out larger than available quantity and show it's rejected.

## 9. Auto-save persisting without an explicit save (new feature -- important)

- After adding a product or recording a transaction, **do not** choose
  "Save & exit." Instead, wait a few seconds on screen (narrate: "the
  background auto-save thread flushes within a few seconds").
- Force-close the terminal window or kill the process directly (not via
  the menu).
- Re-run `./build/toolhub` and show the change is still there in
  "Loaded N product(s)..." -- this is the concurrency feature working, not
  just persistence.

## 10. Persistence survives a normal restart (required by the brief)

- Choose option 0 (Save & exit) normally this time.
- Re-run `./build/toolhub` and show "Loaded 2 product(s), 1 supplier(s), 1
  transaction(s)." matching what was there before.

## 11. Wrap-up

- Show the GitHub repository page briefly (README rendering, file
  listing, commit history) to tie the video back to the submitted
  repository.
- One sentence to camera or caption: "18 tests, zero compiler warnings,
  all edge cases handled."

## Notes

- Keep each shot to what it needs to prove -- don't narrate every
  keystroke.
- If recording in one continuous take, the seed data added in step 4
  should be the only data used for the rest of the video, so the numbers
  stay consistent with the written documentation (Claw Hammer qty 21 after
  one stock-out of 3, Screws qty 2).
- Submit either as a video file or a `.txt`/`.md` file containing a link
  (e.g. an unlisted YouTube link or a link to the file in the GitHub repo),
  per the folder-structure spec's `04-Final-Product` entry.
