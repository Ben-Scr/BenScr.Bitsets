# BenScr.Bitsets

Created by Ben Schneider — https://github.com/Ben-Scr

Owning C++20 dynamic bitsets for membership filters, feature masks and compact sets of integer
indices. Includes set operations, shifts, rank/select and a canonical byte representation.
There are no runtime dependencies beyond the C++ standard library.

The implementation is header-only so the word-level operations compile with the consumer's
toolchain and runtime. It ships no precompiled library or native debug-symbol archive.

## Example

```cpp
#include <benscr/bitsets/bitsets.hpp>

namespace Bitsets = BenScr::Bitsets;

Bitsets::DynamicBitset enabled(130);
enabled.Set(2);
enabled.Set(64);
enabled.Set(129);

Bitsets::DynamicBitset excluded(130);
excluded.Set(64);
enabled.ExceptWith(excluded);

const auto count = enabled.Count();       // 2
const auto prefix = enabled.Rank(65);     // 1: indices [0,65)
const auto second = enabled.Select(1);    // optional index 129
const auto bytes = enabled.ToBytes();
auto restored = Bitsets::DynamicBitset::FromBytes(enabled.Size(), bytes);
```

The [complete executable example](examples/bitsets.cpp) also checks shifts, shrink/grow and moves.

## Install

NuGet package ID: **BenScr.Bitsets**, version **1.0.0**. Install in a native Visual C++ project with
NuGet's `packages.config` integration. The exact `BenScr.Bitsets.props` and `BenScr.Bitsets.targets`
files add the header search path and validate the restored header. Enable C++20 in the consumer.
Windows x64 Debug and Release consumers are tested with MSVC v143 and the installed Visual Studio
toolchain. No fixed CRT linkage or separate ABI is imposed by this header-only package.

For CMake, install from a checkout:

```shell
cmake -S . -B build -DBENSCR_BITSETS_BUILD_TESTS=OFF -DBENSCR_BITSETS_BUILD_EXAMPLES=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix /path/to/prefix
```

Then use the installed package, setting `CMAKE_PREFIX_PATH` to that prefix:

```cmake
find_package(BenScrBitsets 1 CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE BenScr::Bitsets)
```

The NuGet archive supplies native MSBuild integration. Installed CMake configs are a separate
source-installation route, tested after moving the entire installation outside the source tree.
As a CMake subproject, tests and examples default to off. CMake 3.28 or newer is required.

## Size, ownership and errors

- `Size()` is the number of logical bits, including zero-valued trailing bits. Indices are in
  `[0, Size())`. `Empty()` means zero length, whereas `None()` means no set bits.
- `kMaxBits` is 134,217,728 bits: 16 MiB of packed payload. Constructors, `Resize` and `FromBytes`
  reject larger sizes with `std::length_error` before allocation. Allocations may additionally
  fail with `std::bad_alloc`; the limit is not a promise of available memory.
- `Test`, `Set`, `Reset`, `Flip` and `Rank` validate their bounds in all builds. Invalid indices
  throw `std::out_of_range`. No unchecked indexing or mutable raw-storage view is exposed.
- Copies own independent storage. Moves leave the source empty and reusable. Self-copy and
  self-move are no-ops. `Swap` exchanges complete values.
- All mutating operations provide the strong exception guarantee. Length/index/shape checks
  happen before changes, and allocation failure during resize or copy assignment preserves the
  original value. Successful mutation never leaves nonzero padding bits.
- Concurrent const operations are safe; synchronize every concurrent access involving a mutation.
  There are no atomic-bit operations, hidden caches or thread-local/shared global state.

## Operations

| API | Contract |
| --- | --- |
| `Set(index, value = true)`, `Reset(index)`, `Flip(index)` | Change one bit |
| `SetAll(value = true)`, `ResetAll()`, `FlipAll()` | Change all existing bits without changing size |
| `Count()`, `Any()`, `None()`, `All()` | Query set bits; for empty sets: 0, false, true, true |
| `Resize(size, value = false)` | Keep the shared prefix and initialize new bits to `value` |
| `UnionWith`, `IntersectWith`, `XorWith`, `ExceptWith` | In-place OR, AND, XOR and left-minus-right |
| `ShiftLeft(count)`, `ShiftRight(count)` | Move to higher/lower indices, fill with zero, discard overflow |
| `Rank(endExclusive)` | Count set bits strictly before the endpoint; `Size()` is valid |
| `Select(nth)` | Zero-based nth set-bit index; `std::nullopt` when absent |
| `==`, `!=` | Compare size and every logical bit |

Binary set operations require equal logical sizes and throw `std::invalid_argument` otherwise,
without modifying either input. Self-aliasing is supported: union/intersection preserve the value;
XOR/difference clear its bits. Copy first when a non-mutating result is wanted.

Shrinking discards bits permanently. Growing after a shrink initializes the added range; it does
not reveal retained capacity. A zero shift preserves the value; a shift at least as large as the
logical size clears all bits, even for `size_t`'s largest value. Shifts do not change size or allocate.

Single-bit access is constant-time. Bulk queries, mutations and serialization scan words or bytes;
rank scans the preceding words, and select scans words plus at most 63 set bits in the selected word.
Resize can allocate; copying and `ToBytes()` allocate owned output. No compressed or indexed
rank/select representation, speed benchmark, or constant-time cryptographic behavior is promised.

## Byte representation

`ToBytes()` returns exactly `ceil(Size()/8)` bytes. Bit index `i` is stored in byte `i/8` with mask
`1 << (i % 8)`. Bytes run from lower to higher bit indices, independently of host endianness.
Unused high bits in the last byte are zero. For example, nine bits with indices 0, 7 and 8 set encode
as `81 01` (hex). An empty bitset encodes as an empty byte sequence.

`FromBytes(size, bytes)` takes the logical bit count separately. It rejects both missing and extra
bytes, and nonzero unused padding, with `std::invalid_argument`. It validates the complete shape
before allocating and copies the input. The format has no implicit length prefix, version header,
compression or checksum; a containing protocol must supply any such framing it requires.

## Verification and license

From a clean committed checkout, run PowerShell 7:

```powershell
pwsh -File tools/Verify.ps1 -Generator 'Visual Studio 17 2022' -Toolset v143
```

Pass `-CMake` to select a stable CMake executable when necessary. Linux uses `-Generator Ninja
-Sanitizers` with GCC 14 or Clang 18. See [VERIFICATION.md](VERIFICATION.md),
[RELEASE.md](RELEASE.md) and [API contracts](docs/API.md). The custom original license is
[LICENSE.txt](LICENSE.txt); [provenance](docs/PROVENANCE.md) records its exact bytes and the unchanged
embedded `BenScr_Icon_Master.png`.
