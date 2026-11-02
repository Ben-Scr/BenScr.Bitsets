# API contract, version 1.0.0

Public header: `<benscr/bitsets/bitsets.hpp>`.
Namespace: `BenScr::Bitsets`; type: `DynamicBitset`.

The checked `public-header-baseline.json` records the complete reviewed header with LF-normalized
SHA-256. Any header change requires reviewing both API and behavior before updating this baseline.

- Construction: explicit `(size_t size = 0, bool value = false)`, bounded by public `kMaxBits`.
- Copy construction/assignment: independent storage; assignment has the strong exception guarantee.
- Move construction/assignment: noexcept; source becomes empty; self-move leaves the value intact.
- `Swap`, `Size`, `Empty`, `Count`, `Any`, `None`, `All`, `SetAll`, `ResetAll`, `FlipAll`, shifts, `Select`
  and equality do not allocate and are noexcept.
- `Test`, `Set`, `Reset`, `Flip`: checked zero-based indices strictly below `Size()`.
- `Resize`: preserves the shared prefix; new bits receive the supplied value; removed bits vanish.
- `UnionWith`, `IntersectWith`, `XorWith`, `ExceptWith`: same-size precondition checked before
  mutation, no allocation; the argument may alias the receiver.
- `Rank`: endpoint is inclusive in its allowed range `[0,Size()]`, exclusive in counted bits.
- `Select`: optional index for a zero-based set-bit ordinal, without sentinel index values.
- `ToBytes`: returns owned canonical packed bytes. `FromBytes`: validates then copies a borrowed
  exact-length span, with a separate logical bit count and zero high padding.
- `operator==` compares logical length and content; C++20 supplies rewritten `!=`.

See README for exception types, byte format, complexity, ownership and synchronization contracts.
