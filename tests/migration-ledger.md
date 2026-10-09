# Migration ledger: tools/cansim_runner/checks.cpp -> tests/ (feature 030)

Each `CheckFeatureNNN()` body was moved verbatim into one `TEST(FeatureNNN, <ids>)` (path accessors
only changed), because the checks of a feature share local state. `checks.exe` stays until a
later cleanup retires it (spec FR-009).

Verified on 2026-10-07 (Release): `checks.exe` and `runTests` run the same 214 checks
(each `Check()` call is counted once per execution; loops run it several times). Right after
the move the `[PASS]` labels of both were identical (`CANSIM_TEST_VERBOSE=1` prints them from
`runTests`) apart from one measured time in the 018 SC-005 label.

The comments and `Check()` labels in `tests/` were then translated to Japanese (judgements, ids
and test names unchanged). After that the 214 checks still pass, and the ids at the head of the
labels match `checks.exe` one to one; only the wording after the id differs.

| Feature | Check() calls (checks.cpp) | Moved | Test file | Status | Notes |
|---|---|---|---|---|---|
| 001 | 15 | 15 | feature_001_tests.cpp | Done | |
| 002 | 12 | 12 | feature_002_tests.cpp | Done | |
| 003 | 9 | 9 | feature_003_tests.cpp | Done | |
| 004 | 12 | 12 | feature_004_tests.cpp | Done | SC-009 baseline comparison runs only with `CANSIM_BASELINE_DIR` (prints a NOTE otherwise) |
| 005 | 11 | 11 | feature_005_tests.cpp | Done | |
| 006 | 25 | 25 | feature_006_tests.cpp | Done | |
| 007 | 3 | 3 | feature_007_tests.cpp | Done | |
| 008 | 13 | 13 | feature_008_tests.cpp | Done | SC-005 baseline comparison: same as 004 |
| 009 | 7 | 7 | feature_009_tests.cpp | Done | |
| 010 | 6 | 6 | feature_010_tests.cpp | Done | |
| 011 | 4 | 4 | feature_011_tests.cpp | Done | |
| 014 | 8 | 8 | feature_014_tests.cpp | Done | |
| 015 | 7 | 7 | feature_015_tests.cpp | Done | |
| 017 | 4 | 4 | feature_017_tests.cpp | Done | |
| 018 | 12 | 12 | feature_018_tests.cpp | Done | the SC-005 speed check (1 of the 12) is its own test `Feature018Slow.SC005_EffectSpeed`, label `slow`; the rest is `Feature018.FR001_FR005` |
| 019 | 3 | 3 | feature_019_tests.cpp | Done | |
| 020 | 1 | 1 | feature_020_tests.cpp | Done | no SC/FR id in the labels (name `Checks`) |
| 022 | 7 | 7 | feature_022_tests.cpp | Done | |
| 023 | 7 | 7 | feature_023_tests.cpp | Done | |
| 024 | 3 | 3 | feature_024_tests.cpp | Done | |
| 025 | 6 | 6 | feature_025_tests.cpp | Done | |
| 026 | 4 | 4 | feature_026_tests.cpp | Done | |
| 027 | 6 | 6 | feature_027_tests.cpp | Done | |
| 028 | 9 | 9 | feature_028_tests.cpp | Done | |
| 029 | 10 | 10 | feature_029_tests.cpp | Done | |
| Total | 204 | 204 | | | |

GUI-only success criteria never were part of `checks.cpp` (it drives `CanSim/` directly), so nothing
is left out for that reason.

Test names list the SC/FR ids of the feature's own spec that appear in the labels
(`SC001to005` = SC-001..SC-005). Ids that belong to another feature (for example `002 SC-006`
inside feature 001's checks) stay in the label only.

Run time (26 tests): Release about 3 s, Debug about 13 s without `slow`. Only the 018 SC-005 speed check, which asserts a wall-clock limit of 1 s (0.9 s in Debug), carries the `slow` label; run Debug with `ctest -C Debug -LE slow`.
