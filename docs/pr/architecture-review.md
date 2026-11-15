# A Second Check of the Architecture, and a Review

Branch `architecture-review`.

## Goal

A second, independent check of the documents, by a stronger model than the one that wrote them: every claim of the architecture document read against the code, and a design review with its suspicions proved by throwaway tests.

This pull request changes documents only. No code, no test and no configuration changes.

## Decisions

- **Both architecture documents are corrected, not rewritten.** In `ARCHITECTURE.md`: the read-only parameters, a table of the servers and ports, the Threads table, a task that is not short when it downloads a picture, a timed-out task that still runs. In `WEB_PAGE.md`: an idea in bold, the diagram of the imports, How to Extend the Page.
- **`docs/Review.md` is new.** Its verdict: the one-thread design holds; the weakness is the edges of that thread, a request that waits too long, an error in the middle of a turn, a disk that refuses a file. With `keep_on_camera` false, several of them lose pictures.
- **The review does not name the users of the camera.** What it found in them was reported apart.

- **The review fixes nothing.** Each finding says whether a test proved it or it comes from reading, and whether it is a safety issue for the rig. The fixes are separate pull requests, in the order of the review's recommendations.
- **The throwaway tests are not kept.** They ran in the image of StepIt Macro, on a scratch copy of the packages, and were removed. A finding worth a permanent test gets one with its fix.
- **The architecture document describes the design and holds no review.** Where a probe of the review disproved a sentence of it, the sentence is corrected and points to `Review.md`.

## Open Items

- A range past the end of a file makes the web server send its own memory (cpp-httplib 0.14.3). The server listens on every address.
- A picture is removed from the camera before it is saved.
- A `take_picture` that timed out still fires later.
- The one idea of `WEB_PAGE.md`, in bold, is a proposal: confirm or replace it.
