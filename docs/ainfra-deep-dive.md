# AInfra Deep Dive

This document describes the current AInfra system in detail. It is intentionally technical and honest about what works today, what is stubbed, and where the system still needs hardening.

AInfra is not one binary. It is a small stack:

```text
AInfra source (.ainfra)
  -> Rust compiler
  -> AIF binary object file
  -> C InfraVM loader/runtime
  -> PointRun graph execution
  -> optional model connector
  -> InfraOS backend/UI control plane
```

The project currently behaves like a prototype infrastructure runtime with a real compiler/VM/control-plane skeleton. It is not production secure yet. It has a useful local developer workflow, SQLite-backed dashboard auth, a web IDE, a local Ops Assistant, provider-key status reporting, and live OpenAI connector support in the VM. Other model providers are registered but stubbed.

## Repository Layout

The combined workspace is the `AInfra` system. It is also split into smaller publish repos:

- `AInfra`: the combined system.
- `AIF`: language, compiler, format docs, examples, and Rust optimizer dependency.
- `InfraVM`: C runtime, PointRun, C optimizer, and VM connectors.
- `InfraOS`: FastAPI backend, TypeScript UI, auth, IDE, Ops Assistant, registry, and shell.

In the combined workspace the important directories are:

```text
ainfra-compiler/     Rust compiler from AInfra source to AIF
optimizer/rust/      Rust optimizer used by compiler
optimizer/c/         C optimizer used by InfraVM
infravm/             C11 VM runtime and connectors
infraos-backend/     FastAPI backend and SQLite control plane
infraos-ui/          Vanilla TypeScript + Vite UI
shell/               Developer shell commands
docs/                Format, design, and deep technical docs
examples/            AInfra/A2 examples
data/objects/        Generated .aif object files
data/infraos.sqlite3 SQLite runtime metadata/auth database
```

## Conceptual Model

AInfra describes AI infrastructure as objects:

- `model`: describes a model provider and model settings.
- `prompt`: stores a prompt template.
- `agent`: connects a model to a prompt.
- `port`: describes an external interface; currently only partially executable.
- `run`: describes an execution entrypoint.
- `import`: records a provider/system dependency.
- `var`: stores a top-level variable-style value.

The compiler turns those declarations into AIF objects. Each AIF object has:

- an object id such as `model:local`, `prompt:answer`, `agent:helper`, `run:1`;
- a type such as `model`, `prompt`, `agent`, `port`, `run`;
- properties;
- pointers to other objects;
- a start flag.

InfraVM loads the AIF file and executes by following pointers.

## AInfra Language v0.1

The current language supports these top-level forms:

```text
import
var
model
prompt
agent
port
run
```

Example:

```ainfra
import ai.local.ollama

// $start$

model local {
    engine = "ollama"
    name = "llama3.2"
}

prompt answer {
    text = "Answer clearly and briefly: {input}"
}

agent helper {
    model = local
    prompt = answer
}

run agent helper on "What is PointRun?"
```

The parser supports:

- identifiers;
- dotted names such as `ai.local.ollama`;
- strings;
- numbers stored as source text;
- booleans;
- references;
- lists;
- object blocks;
- `run` statements with optional kind and optional `on` input.

The compiler validates some cross references:

- `model` blocks must have `engine`.
- `agent.model` must reference an existing `model`.
- `agent.prompt`, when provided, must reference an existing `prompt`.
- `run` kind must resolve to supported kinds such as `agent`, `model`, or `port`.

The language does not yet support a full statement language inside block bodies. Earlier A2 body-style examples such as `on_request { ... }` are not a full executable body language in the current AInfra compiler path. Port support exists as objects/pointers, but endpoint serving and request execution are not production behavior yet.

## Start Object Semantics

The compiler detects `// $start$` in source.

Current behavior:

- If a start marker exists, the first `run` statement is marked as start.
- If no run has an explicit start marker, the compiler marks the first `run` as start.
- InfraVM's `infravm_run_start` finds the first object with `start_flag`.
- If no object is marked as start, the VM falls back to the first loaded object.

Practical recommendation:

```ainfra
// $start$
run agent helper on "..."
```

Always include exactly one obvious `run` entrypoint while the language is still v0.1.

## Compiler Pipeline

The compiler is in `ainfra-compiler/`.

High-level flow:

```text
source text
  -> lexer
  -> tokens with line/column data
  -> parser
  -> AST-like Program
  -> validation
  -> optimizer-backed AIF writer
  -> .aif bytes
```

Important compiler modules:

- `lexer.rs`: tokenizes source and attaches line/column info.
- `parser.rs`: parses imports, vars, object blocks, values, lists, and runs.
- `ast.rs`: shared program/block/value/run structures.
- `aif_writer.rs`: converts a parsed program into stable AIF objects.
- `main.rs`: CLI entrypoint.

Build:

```sh
cargo build --manifest-path ainfra-compiler/Cargo.toml
```

Compile:

```sh
./ainfra-compiler/target/debug/ainfra-compiler examples/local-stub.ainfra -o data/objects/local-stub.aif
```

The compiler depends on the Rust optimizer:

```toml
ainfra-optimizer = { path = "../optimizer/rust" }
```

That is why the standalone `AIF` repo includes `optimizer/rust`.

## Optimizer

The optimizer currently exists in two forms:

- `optimizer/rust`: used during AIF writing by the compiler.
- `optimizer/c`: used by InfraVM while loading/running object graphs.

The Rust optimizer is used to:

- normalize engine names;
- compact prompt whitespace;
- compute object fingerprints.

The compiler also sorts and deduplicates pointers and writes objects in stable order.

The C optimizer is used by InfraVM for:

- provider identification from engine names;
- prompt compaction before execution;
- shared hashing/helpers.

This is not yet an advanced optimizing compiler. It is a small shared utility layer that makes output more stable and runtime handling cleaner.

## AIF Binary Format v0.1

AIF is the compiled object format loaded by InfraVM and indexed by InfraOS.

Binary layout:

```text
magic: "AIF0"
version: u16 little-endian
object_count: u32 little-endian

repeated object_count times:
  object_id: string
  name: string
  type: string
  start_flag: u8
  property_count: u32
  repeated properties:
    key: string
    value_tag: u8
    value bytes
  pointer_count: u32
  repeated pointers:
    pointer_type: string
    target_object_id: string
  instruction_count: u32
  repeated instruction bytes
```

String encoding:

```text
u32 byte_length
UTF-8 bytes
```

Value tags:

| Tag | Meaning |
| --- | --- |
| `1` | string |
| `2` | number stored as source text |
| `3` | bool |
| `4` | reference |
| `5` | list |

Current pointers emitted by the compiler:

| Source | Pointer |
| --- | --- |
| `agent.model = local` | `agent:* -> uses -> model:local` |
| `agent.prompt = answer` | `agent:* -> uses -> prompt:answer` |
| `port.agent` or `port.route` | `port:* -> routes -> agent:*` |
| `run agent helper` | `run:* -> runs -> agent:helper` |
| `run port api` | `run:* -> runs -> port:api` |

Each compiled block also gets an `optimizer_hash` property. This is a simple fingerprint, not a security signature.

## InfraVM Runtime

InfraVM is the C11 runtime in `infravm/`.

Build:

```sh
make -C infravm
```

Run:

```sh
./infravm/infravm data/objects/local-stub.aif
./infravm/infravm data/objects/local-stub.aif run:1
./infravm/infravm data/objects/local-stub.aif agent:helper
```

The VM:

1. Opens an AIF file.
2. Checks the magic header `AIF0`.
3. Checks version `1`.
4. Reads all objects.
5. Reads properties and typed values.
6. Reads pointers.
7. Prints a debug registry.
8. Runs either the start object or a requested object id.

The loader has bounds checks for truncated files and reports errors through `infravm_last_error`.

## PointRun

PointRun is the current graph execution algorithm in InfraVM.

PointRun starts from:

- the VM start object if no explicit object id is passed; or
- the object id passed as CLI argument/API request.

Execution flow:

```text
run object
  -> follows first runs pointer
  -> target object

agent object
  -> resolves model property
  -> resolves prompt property
  -> replaces {input}
  -> compacts final prompt
  -> dispatches to provider behavior

port object
  -> follows first pointer if present

model object
  -> prints stub that model objects execute through agents
```

For an agent:

1. Read `model` property, build object id `model:<name>`.
2. Read `prompt` property, build object id `prompt:<name>`.
3. Resolve both objects from the loaded AIF registry.
4. Read prompt template from `prompt.text`.
5. Replace `{input}` with run input.
6. Compact final prompt using the C optimizer.
7. Dispatch by `model.engine`.

Example:

```text
run:1
  -> runs agent:helper

agent:helper
  -> uses model:local
  -> uses prompt:answer
```

If the run input is:

```text
What is PointRun?
```

and the prompt is:

```text
Answer clearly and briefly: {input}
```

PointRun builds:

```text
Answer clearly and briefly: What is PointRun?
```

## Provider Connectors

Current provider behavior:

| Engine | Runtime behavior |
| --- | --- |
| `openai` | live HTTPS connector through `vm_openai.h` |
| `ollama` | registered local stub |
| `anthropic` | registered stub |
| `gemini` | registered stub |
| `microsoft` | registered stub |
| `deepseek` | registered stub |
| `huggingface` | registered stub |

Only OpenAI is live today.

The OpenAI connector:

- is header-only C;
- uses libcurl;
- calls `https://api.openai.com/v1/responses`;
- reads API key from `OPENAI_API_KEY` unless config explicitly provides `api_key`;
- does not hardcode secrets;
- returns `VMOpenAIResult` with `ok`, `text`, and `error`.

Usage:

```sh
export OPENAI_API_KEY="..."
./infravm/infravm data/objects/hello.aif
```

The Ollama connector is currently a stub:

```text
InfraVM stub: Ollama connector is a VM v0.1 stub
```

That is still useful for local compile/run demos because it avoids requiring cloud API keys.

## InfraOS

InfraOS is the local control plane.

It has:

- FastAPI backend;
- WebSocket event stream;
- SQLite database;
- object registry loaded from `data/objects/*.aif`;
- compile API;
- VM run API;
- auth API;
- provider key-status API;
- vanilla TypeScript + Vite UI;
- AInfra IDE;
- local Ops Assistant.

Run:

```sh
shell/infraos.sh start
```

URLs:

```text
UI:      http://127.0.0.1:5173
Backend: http://127.0.0.1:8000
Docs:    http://127.0.0.1:8000/docs
```

Stop:

```sh
shell/infraos.sh stop
```

Initialize runtime state:

```sh
shell/infraos.sh init
```

`init` creates runtime folders, initializes SQLite if missing, and bootstraps the default admin account.

## InfraOS Backend

Backend entrypoint:

```text
infraos-backend/main.py
```

Mounted routers:

- `/api/auth/*`
- `/api/objects/*`
- `/api/compile`
- `/api/vm/run-start`
- `/api/vm/pointrun/{object_id}`
- `/api/vm/run-file`
- `/api/peers/*`
- `/api/logs`
- `/ws/events`

Health endpoint:

```text
GET /api/health
```

It reports:

- service ok/name;
- visible server name;
- object count;
- start object;
- provider key status;
- compiler path;
- VM path;
- autostart flag.

## Object Registry

InfraOS loads all AIF files from:

```text
data/objects/*.aif
```

The Python registry reader duplicates enough of the AIF parser to index objects. It reads:

- `object_id`;
- `name`;
- `type`;
- `start_flag`;
- properties;
- pointers;
- file path.

It stores object metadata into SQLite via `upsert_object`.

Important limitation: if multiple AIF files contain the same object id, the SQLite `objects` table is updated by object id. The in-memory API list may still contain objects from multiple files, but DB metadata is last-writer style per object id.

## Compile And Run Bridge

InfraOS does not compile in-process. It shells out to the Rust compiler.

Compile flow:

```text
POST /api/compile
  -> write temp .ainfra source
  -> run ainfra-compiler
  -> output data/objects/<name>.aif
  -> reload registry
  -> emit websocket compile event
```

Run flow:

```text
POST /api/vm/run-start
  -> find registry start object
  -> run infravm <file_path>
```

PointRun flow:

```text
POST /api/vm/pointrun/{object_id}
  -> find object by id in registry
  -> run infravm <file_path> <object_id>
```

IDE run-file flow:

```text
POST /api/vm/run-file
  -> run infravm <file_path> [object_id]
```

The `run-file` endpoint exists because the IDE should run the exact AIF file it just compiled, not whichever start object happens to be first in the global registry.

## Authentication

InfraOS auth is SQLite-backed.

Database tables:

- `users`
- `user_privileges`
- `sessions`
- `privilege_requests`
- `notifications`

Default bootstrap:

```text
username: admin
password: admin
```

The password is not stored in plaintext. It is hashed using:

```text
PBKDF2-HMAC-SHA256
120,000 iterations
random hex salt
```

Stored format:

```text
pbkdf2_sha256$<salt>$<digest>
```

Login flow:

```text
POST /api/auth/login
  { "username": "...", "password": "..." }

-> verifies password
-> creates random session token
-> stores token in sessions table
-> returns token and user payload
```

Client behavior:

- stores token in browser localStorage;
- sends `Authorization: Bearer <token>` on API calls;
- calls `/api/auth/me` to restore session.

Logout:

```text
POST /api/auth/logout
```

This removes the session token from SQLite when possible and clears localStorage in the UI.

## Privileges

Current privilege names:

```text
objects:read
objects:write
vm:run
compile
peers:manage
auth:manage
admin
```

The default admin account gets all privileges.

Important current limitation: admin-only auth routes are enforced, but not every existing VM/object endpoint is fully privilege-gated yet. The privilege model exists and is used for admin auth management, but endpoint-level authorization should be expanded before production use.

Admin check:

```text
is_admin == true
or privilege list contains "admin"
```

Some UI logic also treats `auth:manage` as admin-like for display/listing, but backend mutation routes call `require_admin`.

## Account Management

Admins can:

- list accounts;
- create accounts;
- remove accounts except the default `admin`;
- grant privileges;
- revoke privileges.

Account fields:

- username;
- password;
- full name;
- phone;
- email;
- is_admin flag;
- privileges.

When a privilege is changed, the target user gets a notification.

## Privilege Request Flow

Non-admin users can request privileges.

Flow:

```text
user requests privilege
  -> row inserted into privilege_requests
  -> admin notifications inserted
  -> admin sees toast
  -> admin opens Auth page
  -> admin grants or denies
  -> request status updates
  -> user receives granted/denied notification
```

Request statuses:

```text
pending
granted
denied
```

Notifications are stored in SQLite and exposed through:

```text
GET  /api/auth/notifications
POST /api/auth/notifications/seen
```

## InfraOS UI

The UI is vanilla TypeScript + Vite. It intentionally avoids React.

Major pages:

- Dashboard;
- Objects;
- Graph;
- IDE;
- VM;
- Peers;
- Auth;
- Settings.

The UI has a cloud-console style layout:

- dark service navigation rail;
- topbar with breadcrumb, provider status, user chip, sign-out;
- resource metric cards;
- tables and status chips;
- auth/account panels.

The UI preserves form values across background renders. This matters because logs, websocket events, notification polling, and toasts can update state while a user is typing.

## AInfra IDE

The IDE page is the primary developer surface.

It supports:

- editing AInfra source;
- choosing templates;
- compiling to AIF;
- running the freshly compiled AIF file;
- PointRun by object id;
- viewing compiled objects;
- reading stdout/stderr output;
- asking the local Ops Assistant for debugging hints.

Templates:

- Local Agent: uses `ollama` stub for safe local demos.
- OpenAI Agent: uses live OpenAI connector if `OPENAI_API_KEY` is available.
- Port: creates a port-style object graph, but full port serving is not implemented.

The IDE stores the output path returned from `/api/compile`. Its Run Start and PointRun buttons call `/api/vm/run-file`, which prevents accidentally running a stale registry object from another compiled file.

## Ops Assistant

The current Ops Assistant is local/rules-based. It is not yet an LLM chatbot.

It inspects:

- active source;
- current output;
- recent logs;
- provider key status.

It gives hints for:

- missing `OPENAI_API_KEY`;
- rejected OpenAI keys;
- stale auth backend routes;
- common compiler reference errors;
- unknown AIF value tags;
- missing `// $start$`;
- missing `run` statements;
- safe local Ollama stub usage.

This is intentionally modest. A future version can connect to a model, but the current version works without any API key.

## Shell Commands

The main developer command surface is:

```sh
shell/infraos.sh
```

Common commands:

```sh
shell/infraos.sh init
shell/infraos.sh start
shell/infraos.sh stop
shell/infraos.sh restart
shell/infraos.sh status
shell/infraos.sh doctor
shell/infraos.sh logs backend
shell/infraos.sh logs ui
```

Build/run:

```sh
shell/infraos.sh build
shell/infraos.sh compile examples/local-stub.ainfra data/objects/local-stub.aif
shell/infraos.sh run data/objects/local-stub.aif
shell/infraos.sh demo
shell/infraos.sh providers
```

The shell script now self-heals runtime state:

- creates `.infraos`;
- creates `.infraos/logs`;
- creates `data/objects`;
- creates backend virtualenv if missing;
- installs backend requirements if needed;
- initializes SQLite;
- bootstraps default admin if missing.

It also skips missing compiler/VM builds when used from standalone split repos such as `InfraOS`.

## Environment Variables

Server/runtime:

```sh
export INFRAOS_SERVER_NAME="My VM Server"
export INFRAOS_BACKEND_URL="http://127.0.0.1:8000"
export INFRAOS_UI_URL="http://127.0.0.1:5173"
export INFRAOS_BACKEND_PORT=8000
export INFRAOS_UI_PORT=5173
export INFRAOS_AUTOSTART=0
```

Provider keys:

```sh
export OPENAI_API_KEY="..."
export ANTHROPIC_API_KEY="..."
export GEMINI_API_KEY="..."
export GOOGLE_API_KEY="..."
export AZURE_OPENAI_API_KEY="..."
export MICROSOFT_API_KEY="..."
export DEEPSEEK_API_KEY="..."
export HUGGINGFACE_API_KEY="..."
export HF_TOKEN="..."
```

Only OpenAI is live in InfraVM today. The others are reported in status and registered as future/stub providers.

## CI/CD

The split repos include GitHub Actions.

Current checks:

- `AInfra`: Rust compiler build, Rust optimizer build, C VM build, Python backend import check, UI build.
- `AIF`: Rust compiler build and local example compile.
- `InfraVM`: C11 VM build and binary smoke check.
- `InfraOS`: Python backend import check and UI build.

The CI is build/test oriented. It does not currently deploy services.

## Publishing Repos

The workspace has `push.sh`.

Commands:

```sh
./push.sh generate
./push.sh push
./push.sh clean
```

`generate` creates clean split repos under:

```text
repos/
```

`push` clones/fetches the GitHub repos into:

```text
.publish-work/
```

Then it syncs generated content, commits changes, and pushes `main`.

Generated repos exclude:

- `.git`;
- `.venv`;
- `node_modules`;
- `target`;
- `dist`;
- `__pycache__`;
- compiled `.aif`;
- compiled `.a2bc`.

## Security Notes

Current security posture is local-development only.

Things that are good:

- API keys are read from environment variables.
- API keys are not intentionally stored in frontend code.
- Passwords are PBKDF2 hashed with salts.
- Session tokens are random.
- Admin mutation routes require authenticated admin users.

Things that are not production-ready:

- default `admin/admin` must be changed;
- no HTTPS termination is configured;
- CORS currently allows all origins;
- localStorage token storage is convenient but not ideal for high-security deployments;
- session expiry is not implemented;
- endpoint privilege enforcement is incomplete outside admin/auth routes;
- no audit-log hardening;
- no rate limiting;
- no CSRF protection;
- no multi-tenant isolation model;
- VM execution runs local binaries and should be sandboxed before untrusted use;
- OpenAI connector is live but other provider connectors are stubs.

## Current Limitations

Language/compiler:

- no full procedural body language;
- no type checker beyond current reference/property validation;
- no package manager;
- no import resolution beyond recording imports;
- port bodies are not executable server logic yet.

AIF:

- instruction section exists as a count, but v0.1 writes zero instructions;
- object ids can collide across multiple compiled files in the global registry;
- `optimizer_hash` is not cryptographic integrity.

InfraVM:

- only OpenAI connector is live;
- Ollama and other providers are stubs;
- no sandboxing;
- no concurrency model;
- no real port server runtime;
- no persistent VM state.

InfraOS:

- auth exists but endpoint-level privilege enforcement is incomplete;
- no production deployment profile;
- no migrations framework beyond `create table if not exists`;
- no password reset flow;
- no real team chat yet;
- Ops Assistant is rules-based, not model-backed.

## Development Roadmap

The natural next steps are:

1. Add complete endpoint privilege enforcement.
2. Add session expiry and password-change flows.
3. Add real Ollama connector support.
4. Add Anthropic/Gemini/DeepSeek/Hugging Face connectors.
5. Add a real port server runtime.
6. Add AInfra body statement parsing.
7. Add AIF instruction execution.
8. Add migrations for SQLite schema changes.
9. Add VM sandboxing.
10. Add model-backed Ops Assistant with guarded context.
11. Add resource-attached comments/activity rather than generic chat.
12. Add integration tests that compile and run sample graphs in CI.

## Mental Model For Debugging

When something fails, trace the layer:

```text
Does source parse?
  -> compiler error with line/column

Does compiler emit AIF?
  -> check data/objects/<name>.aif

Does InfraOS registry see the object?
  -> GET /api/objects

Does VM load the file?
  -> ./infravm/infravm file.aif

Does PointRun find the target?
  -> check run pointer and object ids

Does agent resolve model/prompt?
  -> check agent.model and agent.prompt names

Does provider execute?
  -> OpenAI needs OPENAI_API_KEY; others are currently stubs

Does UI call the correct backend?
  -> check VITE_API_BASE or default http://localhost:8000
```

That layered model is the most reliable way to work with AInfra today.
