# Migration

BenScr.Bitsets 1.0.0 introduces a new API; it replaces no existing BenScr package or published API.
There is no private-source migration or automatic conversion from application-specific bit masks.

Use `BenScr::Bitsets::DynamicBitset`. Logical size is part of identity and must match for set
operations. The byte representation carries no length, so preserve the bit count separately.
Bit index zero is the least-significant bit of byte zero; validate this convention when converting
other file formats. `Rank` uses an exclusive endpoint and `Select` uses a zero-based ordinal.
