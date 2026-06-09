# Optimizer

The Optimizer module holds shared performance helpers for the AInfra system.

It has two libraries:

- `optimizer/rust`: used by the AInfra compiler before writing AIF objects.
- `optimizer/c`: used by InfraVM while loading/running object graphs.

## Rust Optimizer

The Rust crate provides:

- stable object ordering
- pointer deduplication
- engine/provider normalization
- prompt text compaction
- fast FNV-1a fingerprints for generated object metadata

## C Optimizer

The C library provides:

- prompt whitespace compaction before model calls
- engine family classification
- FNV-1a hashing for runtime IDs/logging
- small string helpers with no heap-heavy framework dependency

## License

MIT. See `LICENSE`.
