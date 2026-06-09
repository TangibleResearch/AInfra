#!/usr/bin/env sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPOS_DIR="$ROOT_DIR/repos"
PUBLISH_DIR="$ROOT_DIR/.publish-work"
GIT_NAME="${GIT_AUTHOR_NAME:-TangibleResearch}"
GIT_EMAIL="${GIT_AUTHOR_EMAIL:-tangibleresearch@users.noreply.github.com}"

copy_dir() {
  src="$1"
  dst="$2"
  mkdir -p "$(dirname "$dst")"
  rsync -a --delete \
    --exclude '.git' \
    --exclude '.venv' \
    --exclude 'node_modules' \
    --exclude 'target' \
    --exclude 'dist' \
    --exclude '__pycache__' \
    --exclude '*.pyc' \
    --exclude '*.aif' \
    --exclude '*.a2bc' \
    --exclude 'infravm' \
    "$src" "$dst"
}

write_mit() {
  repo="$1"
  cat > "$repo/LICENSE" <<'EOF'
MIT License

Copyright (c) 2026 TangibleResearch

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF
}

write_readme() {
  repo="$1"
  title="$2"
  body="$3"
  {
    printf '# %s\n\n' "$title"
    printf '%s\n\n' "$body"
    printf '## License\n\nMIT\n'
  } > "$repo/README.md"
}

write_ainfra_readme() {
  repo="$1"
  cat > "$repo/README.md" <<'EOF'
# AInfra

AInfra is the combined system for building AI infrastructure from source code:

- **AInfra language/compiler**: Rust compiler for `.ainfra` source.
- **AIF**: compiled AI infrastructure object format.
- **InfraVM**: C11 runtime for loading AIF and executing PointRun.
- **InfraOS**: FastAPI + vanilla TypeScript control plane with auth, IDE, Ops Assistant, object registry, and provider status.
- **Optimizer**: Rust and C libraries for faster/smaller compiler and VM behavior.

## Requirements

Install these tools before building:

- Git
- Rust stable toolchain: <https://rustup.rs>
- C compiler: `clang`, `gcc`, or MSVC/WSL
- Make
- Python 3.12+
- Node.js 22+
- npm
- curl development headers for the VM OpenAI connector

### macOS

```sh
xcode-select --install
brew install rust node python make curl
```

### Ubuntu/Debian

```sh
sudo apt-get update
sudo apt-get install -y build-essential make pkg-config libcurl4-openssl-dev python3 python3-venv nodejs npm curl git
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### Windows

Use WSL2 Ubuntu for the simplest path, then follow the Ubuntu commands above. Native Windows support is not the primary target yet.

## Setup

```sh
git clone https://github.com/TangibleResearch/AInfra.git
cd AInfra
shell/infraos.sh init
shell/infraos.sh build
```

`shell/infraos.sh init` creates runtime folders, initializes SQLite if missing, and creates the default local admin:

```text
username: admin
password: admin
```

Change this password before exposing anything beyond localhost.

## Run

```sh
shell/infraos.sh start
shell/infraos.sh open
```

Default URLs:

- UI: `http://127.0.0.1:5173`
- Backend: `http://127.0.0.1:8000`
- API docs: `http://127.0.0.1:8000/docs`

Stop services:

```sh
shell/infraos.sh stop
```

## Compile And Run AInfra

```sh
shell/infraos.sh compile examples/local-stub.ainfra data/objects/local-stub.aif
shell/infraos.sh run data/objects/local-stub.aif
```

Or run the full demo:

```sh
shell/infraos.sh demo
```

## Environment Variables

Provider keys are optional unless you run cloud models:

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

Server settings:

```sh
export INFRAOS_SERVER_NAME="My VM Server"
export INFRAOS_BACKEND_PORT=8000
export INFRAOS_UI_PORT=5173
```

## CI/CD

This repo includes GitHub Actions in `.github/workflows/ci.yml`.

CI checks:

- Rust compiler build
- Rust optimizer build
- C11 InfraVM build
- Python backend import check
- TypeScript/Vite UI build

GitHub Actions runs on pushes and pull requests to `main`.

## Repo Split

This combined repository is also split into:

- `AIF`: language/compiler/format
- `InfraVM`: C runtime
- `InfraOS`: web control plane

Use `./push.sh push` from the source workspace to regenerate and publish all four repos.

## License

MIT
EOF
}

write_infravm_readme() {
  repo="$1"
  cat > "$repo/README.md" <<'EOF'
# InfraVM

InfraVM is a C11 virtual machine/runtime for AIF files. It loads compiled AI infrastructure objects, resolves PointRun execution, prints/debugs runtime state, and includes simple provider connector stubs plus an OpenAI HTTPS connector.

## Requirements

- Git
- C compiler: `clang` or `gcc`
- Make
- libcurl development headers

### macOS

```sh
xcode-select --install
brew install make curl
```

### Ubuntu/Debian

```sh
sudo apt-get update
sudo apt-get install -y build-essential make libcurl4-openssl-dev git
```

### Windows

Use WSL2 Ubuntu and follow the Ubuntu setup. Native MSVC support is not the primary target yet.

## Build

```sh
git clone https://github.com/TangibleResearch/InfraVM.git
cd InfraVM
make -C infravm
```

The binary is created at:

```text
infravm/infravm
```

## Run

Provide an AIF file generated by the AIF/AInfra compiler:

```sh
./infravm/infravm path/to/object.aif
```

Run a specific object:

```sh
./infravm/infravm path/to/object.aif run:1
./infravm/infravm path/to/object.aif agent:helper
```

## OpenAI Connector

InfraVM reads OpenAI keys from the environment only:

```sh
export OPENAI_API_KEY="..."
./infravm/infravm path/to/openai-object.aif
```

No API keys are hardcoded or stored in the repo.

## Optimizer

The repo includes the C optimizer library under `optimizer/c`. The VM Makefile builds it into the runtime.

## CI/CD

GitHub Actions in `.github/workflows/ci.yml` installs native dependencies, builds the VM, and verifies the binary exists.

## License

MIT
EOF
}

write_infraos_readme() {
  repo="$1"
  cat > "$repo/README.md" <<'EOF'
# InfraOS

InfraOS is the web console and backend control plane for AInfra/InfraVM systems.

It includes:

- FastAPI backend
- Vanilla TypeScript + Vite UI
- SQLite auth
- default local admin bootstrap
- server name support
- account and privilege management
- privilege request notifications
- AInfra IDE
- local Ops Assistant
- object registry APIs
- VM compile/run bridge
- provider key status dashboard

## Requirements

- Git
- Python 3.12+
- Node.js 22+
- npm
- Optional: Rust compiler and InfraVM binary when connected to the full AInfra workspace

### macOS

```sh
brew install python node git
```

### Ubuntu/Debian

```sh
sudo apt-get update
sudo apt-get install -y python3 python3-venv nodejs npm git curl
```

### Windows

Use WSL2 Ubuntu for the simplest path.

## Setup

```sh
git clone https://github.com/TangibleResearch/InfraOS.git
cd InfraOS
shell/infraos.sh init
```

The init command creates runtime folders, initializes SQLite if missing, and creates:

```text
username: admin
password: admin
```

Change the default admin password before exposing the backend beyond localhost.

## Run

```sh
shell/infraos.sh start
```

Open:

```text
http://127.0.0.1:5173
```

Stop:

```sh
shell/infraos.sh stop
```

`InfraOS` can run standalone for dashboard, auth, account management, and UI development. Compile/run actions require the AInfra compiler and InfraVM runtime from the combined `AInfra` repo or compatible local binaries.

## Manual Backend

```sh
cd infraos-backend
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
uvicorn main:app --reload --port 8000
```

## Manual UI

```sh
cd infraos-ui
npm ci
npm run dev
```

## Environment Variables

```sh
export INFRAOS_SERVER_NAME="My VM Server"
export INFRAOS_BACKEND_PORT=8000
export INFRAOS_UI_PORT=5173
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

## CI/CD

GitHub Actions in `.github/workflows/ci.yml` runs:

- backend dependency install
- Python import/compile check
- npm clean install
- TypeScript/Vite UI build

## License

MIT
EOF
}

write_aif_readme() {
  repo="$1"
  cat > "$repo/README.md" <<'EOF'
# AIF

AIF is the compiled AI infrastructure object format and language/compiler package for AInfra source.

This repo contains:

- Rust lexer
- Rust parser
- compiler validation
- AIF writer
- AIF format docs
- language docs
- examples

## Requirements

- Git
- Rust stable toolchain

### macOS/Linux/WSL

```sh
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

## Setup

```sh
git clone https://github.com/TangibleResearch/AIF.git
cd AIF
cargo build --manifest-path ainfra-compiler/Cargo.toml --locked
```

## Compile An Example

```sh
cargo run --manifest-path ainfra-compiler/Cargo.toml -- examples/local-stub.ainfra -o /tmp/local-stub.aif
```

The compiler emits an AIF file that can be loaded by InfraVM.

## Language Shape

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

## Docs

- `docs/language.md`
- `docs/aif-format.md`
- `language.md`

## CI/CD

GitHub Actions in `.github/workflows/ci.yml` builds the Rust compiler and compiles the local example into an AIF file.

## License

MIT
EOF
}

write_workflow() {
  repo="$1"
  name="$2"
  mkdir -p "$repo/.github/workflows"
  cat > "$repo/.github/workflows/ci.yml" <<EOF
name: $name CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
EOF
}

append_ainfra_ci() {
  repo="$1"
  write_workflow "$repo" "AInfra"
  cat >> "$repo/.github/workflows/ci.yml" <<'EOF'
  compiler:
    name: Rust compiler
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
      - name: Build compiler
        run: cargo build --manifest-path ainfra-compiler/Cargo.toml --locked
      - name: Build optimizer
        run: cargo build --manifest-path optimizer/rust/Cargo.toml --locked

  vm:
    name: C VM
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install native dependencies
        run: sudo apt-get update && sudo apt-get install -y build-essential libcurl4-openssl-dev
      - name: Build VM
        run: make -C infravm

  infraos:
    name: InfraOS backend and UI
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.12"
      - uses: actions/setup-node@v4
        with:
          node-version: "22"
          cache: npm
          cache-dependency-path: infraos-ui/package-lock.json
      - name: Install backend dependencies
        run: python -m pip install -r infraos-backend/requirements.txt
      - name: Check backend imports
        run: PYTHONPATH=infraos-backend python -m compileall infraos-backend
      - name: Install UI dependencies
        run: npm ci
        working-directory: infraos-ui
      - name: Build UI
        run: npm run build
        working-directory: infraos-ui
EOF
}

append_infravm_ci() {
  repo="$1"
  write_workflow "$repo" "InfraVM"
  cat >> "$repo/.github/workflows/ci.yml" <<'EOF'
  build:
    name: Build C11 VM
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install native dependencies
        run: sudo apt-get update && sudo apt-get install -y build-essential libcurl4-openssl-dev
      - name: Build InfraVM
        run: make -C infravm
      - name: Smoke test binary
        run: test -x infravm/infravm
EOF
}

append_infraos_ci() {
  repo="$1"
  write_workflow "$repo" "InfraOS"
  cat >> "$repo/.github/workflows/ci.yml" <<'EOF'
  backend:
    name: Backend
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: "3.12"
      - name: Install dependencies
        run: python -m pip install -r infraos-backend/requirements.txt
      - name: Check imports
        run: PYTHONPATH=infraos-backend python -m compileall infraos-backend

  ui:
    name: UI
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: "22"
          cache: npm
          cache-dependency-path: infraos-ui/package-lock.json
      - name: Install dependencies
        run: npm ci
        working-directory: infraos-ui
      - name: Build UI
        run: npm run build
        working-directory: infraos-ui
EOF
}

append_aif_ci() {
  repo="$1"
  write_workflow "$repo" "AIF"
  cat >> "$repo/.github/workflows/ci.yml" <<'EOF'
  compiler:
    name: Build compiler
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
      - name: Build AIF compiler
        run: cargo build --manifest-path ainfra-compiler/Cargo.toml --locked
      - name: Compile local example
        run: |
          cargo run --manifest-path ainfra-compiler/Cargo.toml -- examples/local-stub.ainfra -o /tmp/local-stub.aif
          test -s /tmp/local-stub.aif
EOF
}

prepare_infraos() {
  repo="$REPOS_DIR/InfraOS"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/infraos-backend/" "$repo/infraos-backend/"
  copy_dir "$ROOT_DIR/infraos-ui/" "$repo/infraos-ui/"
  copy_dir "$ROOT_DIR/shell/" "$repo/shell/"
  mkdir -p "$repo/docs"
  cp "$ROOT_DIR/docs/infraos-design.md" "$repo/docs/" 2>/dev/null || true
  write_infraos_readme "$repo"
  append_infraos_ci "$repo"
  write_mit "$repo"
}

prepare_infravm() {
  repo="$REPOS_DIR/InfraVM"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/infravm/" "$repo/infravm/"
  mkdir -p "$repo/optimizer"
  copy_dir "$ROOT_DIR/optimizer/c/" "$repo/optimizer/c/"
  mkdir -p "$repo/docs"
  cp "$ROOT_DIR/docs/pointrun.md" "$repo/docs/" 2>/dev/null || true
  write_infravm_readme "$repo"
  append_infravm_ci "$repo"
  write_mit "$repo"
}

prepare_aif() {
  repo="$REPOS_DIR/AIF"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/ainfra-compiler/" "$repo/ainfra-compiler/"
  mkdir -p "$repo/docs" "$repo/examples"
  cp "$ROOT_DIR/docs/aif-format.md" "$repo/docs/" 2>/dev/null || true
  cp "$ROOT_DIR/docs/language.md" "$repo/docs/" 2>/dev/null || true
  cp "$ROOT_DIR/language.md" "$repo/" 2>/dev/null || true
  cp "$ROOT_DIR/examples/"*.ainfra "$repo/examples/" 2>/dev/null || true
  cp "$ROOT_DIR/examples/"*.a2 "$repo/examples/" 2>/dev/null || true
  write_aif_readme "$repo"
  append_aif_ci "$repo"
  write_mit "$repo"
}

prepare_ainfra() {
  repo="$REPOS_DIR/AInfra"
  mkdir -p "$repo"
  copy_dir "$ROOT_DIR/ainfra-compiler/" "$repo/ainfra-compiler/"
  copy_dir "$ROOT_DIR/infravm/" "$repo/infravm/"
  copy_dir "$ROOT_DIR/infraos-backend/" "$repo/infraos-backend/"
  copy_dir "$ROOT_DIR/infraos-ui/" "$repo/infraos-ui/"
  copy_dir "$ROOT_DIR/optimizer/" "$repo/optimizer/"
  copy_dir "$ROOT_DIR/shell/" "$repo/shell/"
  copy_dir "$ROOT_DIR/docs/" "$repo/docs/"
  mkdir -p "$repo/examples"
  cp "$ROOT_DIR/examples/"*.ainfra "$repo/examples/" 2>/dev/null || true
  cp "$ROOT_DIR/examples/"*.a2 "$repo/examples/" 2>/dev/null || true
  cp "$ROOT_DIR/Makefile" "$repo/" 2>/dev/null || true
  cp "$ROOT_DIR/push.sh" "$repo/" 2>/dev/null || true
  cp "$ROOT_DIR/language.md" "$repo/" 2>/dev/null || true
  write_ainfra_readme "$repo"
  append_ainfra_ci "$repo"
  write_mit "$repo"
}

generate() {
  rm -rf "$REPOS_DIR"
  mkdir -p "$REPOS_DIR"
  prepare_ainfra
  prepare_infravm
  prepare_infraos
  prepare_aif
  echo "generated repos in $REPOS_DIR"
}

publish_repo() {
  generated_repo="$1"
  remote="$2"
  name="$(basename "$generated_repo")"
  publish_repo="$PUBLISH_DIR/$name"
  rm -rf "$publish_repo"
  mkdir -p "$PUBLISH_DIR"
  if git clone "$remote" "$publish_repo"; then
    :
  else
    mkdir -p "$publish_repo"
    (
      cd "$publish_repo"
      git init
      git checkout -B main
      git remote add origin "$remote"
    )
  fi
  rsync -a --delete --exclude '.git' "$generated_repo/" "$publish_repo/"
  (
    cd "$publish_repo"
    git checkout -B main
    git add -A
    if git diff --cached --quiet; then
      echo "$name: no changes to commit"
    else
      git -c user.name="$GIT_NAME" -c user.email="$GIT_EMAIL" commit -m "docs and ci setup"
    fi
    git push -u origin main
  )
}

push_all() {
  generate
  publish_repo "$REPOS_DIR/AInfra" "https://github.com/TangibleResearch/AInfra.git"
  publish_repo "$REPOS_DIR/InfraVM" "https://github.com/TangibleResearch/InfraVM.git"
  publish_repo "$REPOS_DIR/InfraOS" "https://github.com/TangibleResearch/InfraOS.git"
  publish_repo "$REPOS_DIR/AIF" "https://github.com/TangibleResearch/AIF.git"
}

case "${1:-push}" in
  generate)
    generate
    ;;
  push)
    push_all
    ;;
  clean)
    rm -rf "$REPOS_DIR"
    echo "removed $REPOS_DIR"
    ;;
  *)
    echo "Usage: ./push.sh [generate|push|clean]"
    exit 2
    ;;
esac
