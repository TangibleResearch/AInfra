# AIF Format v0.1

AIF is the compiled AI infrastructure object format.

Binary layout:

```txt
magic: "AIF0"
version: u16
object_count: u32

repeated objects:
  object_id: string
  name: string
  type: string
  start_flag: u8
  property_count: u32
  properties:
    key: string
    value_tag: u8
    value
  pointer_count: u32
  pointers:
    pointer_type: string
    target_object_id: string
  instruction_count: u32
```

String encoding is `u32 byte_length` followed by UTF-8 bytes.

Value tags:

- `1`: string
- `2`: number stored as source text
- `3`: bool
- `4`: reference
- `5`: list

Pointers emitted in v0.1:

- `agent.model` creates `uses -> namespace::model:name`
- `agent.prompt` creates `uses -> namespace::prompt:name`
- `port.agent` or `port.route` creates `routes -> namespace::agent:name`
- `run agent X` creates `runs -> namespace::agent:X`

Object IDs are namespaced by the compiler from the output file or `--namespace`, for example `local-stub::run:1`. InfraVM accepts short aliases such as `run:1` only when they resolve unambiguously inside the loaded file.

The `instruction_count` field is reserved. AIF v1 writers emit `0`, and current loaders reject nonzero instruction counts until the instruction encoding is designed.

The compiler uses the Rust Optimizer before writing AIF:

- prompt whitespace is compacted
- engine names are normalized
- pointers are sorted and deduplicated
- objects are written in a stable order
- each object gets an `optimizer_hash` property
