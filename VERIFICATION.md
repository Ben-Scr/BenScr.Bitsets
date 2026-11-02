# Verification

The contract executable runs 13 named groups with an independent `std::vector<bool>` reference.
It covers exhaustive five-bit set pairs, every shift from 0 to 194 around word boundaries, growth
and shrink combinations, invalid sizes/indices, empty semantics, independent copying, moved-from
reuse, self-aliasing, rank/select and byte-order fixtures. It also runs 10,000 deterministic mutation
steps, checking observable bits, rank/select and serialized representation after every step.

Allocation failure is injected only in the test executable through isolated test allocation
replacements. Resize and copy-assignment rollback are checked against their original values.
Maximum supported length is allocated and tested; over-limit values are rejected before allocation.

`tools/Verify.ps1` checks the reviewed public-header hash, builds with warnings as errors, executes
both CTest cases and the complete example, installs and relocates the CMake package, and builds a
separate consumer using only that installation. It packs NuGet, verifies every payload and metadata,
and on Windows restores an empty-cache `packages.config` consumer and runs Release and Debug x64.

CI runs Windows/MSVC v143 and Ubuntu 24.04/GCC 14/Clang 18. The Linux jobs enable AddressSanitizer and
UndefinedBehaviorSanitizer for tests and the relocated consumer. CMake is pinned to 3.31.6 in CI;
the packaging SDK is pinned to 10.0.303 and the NuGet CLI to 7.9.0 with an archive SHA-256 check.
Native MSBuild consumption runs only on Windows; Linux verifies CMake consumption.

`.clang-format` contains the executable formatting rules. Source is formatted with clang-format;
the header baseline is normalized to LF, so checkout line endings do not alter its check.
No solution file is needed. `tests.xml`, package hashes and the platform-specific release manifest
are written under `artifacts/verify-<id>/`. CI results and public-feed evidence are separate from a
local successful run. A configured CI job is not evidence until it has executed successfully.

After publication, run `tools/VerifyNuGetConsumer.ps1 -Published -ExpectedPackage <verified.nupkg>`.
It compares installed payloads with the verified archive in addition to building and executing the
consumer. A repository signature may change ZIP bytes while leaving checked payloads unchanged.
