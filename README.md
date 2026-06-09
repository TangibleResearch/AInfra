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
