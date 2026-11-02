# Release procedure

1. Review source, API contracts, examples, license and icon; commit with a clean checkout.
2. Run `tools/Verify.ps1` and require successful Windows and both Linux CI jobs for that commit.
3. Freeze the exact verified `.nupkg` and its manifest. Confirm package ID/version/source commit,
   unchanged icon/license, native integration and independent-consumer evidence.
4. Point `packages/BenScr.Bitsets/1.0.0` at that commit and publish the exact package archive.
5. Restore and execute a fresh public-feed native consumer, compare package payloads, verify the
   NuGet signature and inspect the indexed gallery icon. Record pending checks as pending.
6. Attach the verified package and final evidence to the matching GitHub release.

This header-only package has no native binary or PDB and does not produce a `.snupkg`.
Never reuse version 1.0.0 for changed package contents or publish from an untrusted pull request.
