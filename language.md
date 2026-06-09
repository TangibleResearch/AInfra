# A² Language Specification

A² is an AI-oriented systems language used to define, connect, run, test, and deploy AI models and agents.

Unlike general-purpose programming languages, A² does not focus on regular modules, loops, math libraries, or app logic. Instead, it focuses on AI infrastructure primitives: models, agents, prompts, memory, tools, ports, tests, and deployments.

A² source files use the `.a2` extension.

Compiled A² bytecode files use the `.a2bc` extension.

---

## 1. Design Goals

A² is designed to be:

* **AI-first**: Models, agents, prompts, and ports are built into the language.
* **Fast**: Source is compiled into base instructions executed by the A²VM.
* **Portable**: The compiler may be written in Rust, while the VM/runtime may be written in C.
* **Secure**: API keys and secrets should be accessed through vault bindings, not hardcoded.
* **Deployable**: A² can expose models or agents through ports.
* **Testable**: AI behavior can be tested with language-level test blocks.
* **Minimal**: A² avoids regular programming modules unless they are directly useful for AI execution.

---

## 2. Core Concepts

A² programs are built from these blocks:

```a2
import
var
model
prompt
agent
memory
tool
port
class
test
deploy
run
```

Each block describes part of an AI system.

---

## 3. Example Program

```a2
import ai.local.ollama
import ai.net.port
import ai.memory.vector

var MODEL_NAME = "llama3.2"
var PORT = 8080

model local_brain {
    engine = "ollama"
    name = MODEL_NAME
    quant = "q4"
    max_ram = "4GB"
}

prompt default_question {
    text = "Answer clearly and simply: {input}"
}

agent helper {
    model = local_brain
    prompt = default_question
    memory = short_term
    mode = "chat"
}

memory short_term {
    type = "buffer"
    size = 20
}

port ask_api {
    protocol = "http"
    host = "0.0.0.0"
    port = PORT

    on_request {
        input = request.body.question
        output = helper.ask(input)
        return output
    }
}

run port ask_api
```

This program creates a local AI agent and opens an HTTP port that accepts questions and returns answers.

---

## 4. Imports

A² imports are not regular code modules. They are AI capability imports.

```a2
import ai.local.ollama
import ai.remote.openai
import ai.net.port
import ai.memory.vector
import ai.vault.secrets
```

Imports load built-in VM capabilities.

Examples:

```a2
import ai.local.ollama
```

Enables Ollama model support.

```a2
import ai.net.port
```

Enables HTTP/TCP port linking.

```a2
import ai.vault.secrets
```

Enables secure secret access.

---

## 5. Variables

Variables store simple constant values.

```a2
var NAME = "helper"
var PORT = 8080
var MAX_RAM = "4GB"
var DEBUG = true
```

Variables can be used inside blocks:

```a2
model brain {
    engine = "ollama"
    name = NAME
    max_ram = MAX_RAM
}
```

A² variables should be simple. Complex runtime logic should belong inside the VM or AI blocks.

---

## 6. Models

A `model` block defines an AI model.

```a2
model local_brain {
    engine = "ollama"
    name = "llama3.2"
    quant = "q4"
    max_ram = "4GB"
    threads = 4
}
```

Supported properties may include:

```a2
engine
name
path
quant
device
max_ram
threads
context
temperature
```

Example with a local file:

```a2
model tiny {
    engine = "llama.cpp"
    path = "./models/tiny.gguf"
    quant = "q4"
    context = 2048
}
```

Example with a remote model:

```a2
model cloud {
    engine = "openai"
    name = "gpt-4.1-mini"
    key = vault.OPENAI_API_KEY
}
```

---

## 7. Prompts

A `prompt` block defines reusable prompt text.

```a2
prompt simple_answer {
    text = "Answer this in simple words: {input}"
}
```

Prompts may use placeholders:

```a2
prompt code_helper {
    text = "You are a coding assistant. Help with this code: {input}"
}
```

---

## 8. Agents

An `agent` connects a model, prompt, memory, and tools.

```a2
agent helper {
    model = local_brain
    prompt = simple_answer
    memory = short_term
    tools = ["files.read", "shell.safe"]
    mode = "chat"
}
```

Supported properties may include:

```a2
model
prompt
memory
tools
mode
max_steps
permissions
```

Example:

```a2
agent coder {
    model = local_brain
    prompt = code_helper
    tools = ["files.read", "files.write", "shell.safe"]
    max_steps = 5
}
```

---

## 9. AI Classes

A² supports AI-oriented classes. These are not normal object-oriented classes. They define reusable AI behaviors.

```a2
class Assistant {
    model = local_brain
    memory = short_term

    ask(input) {
        return model.run("Answer clearly: {input}")
    }
}
```

Example usage:

```a2
agent helper : Assistant {
    prompt = simple_answer
}
```

Classes are mainly used to define reusable AI templates.

Example:

```a2
class CodeAgent {
    tools = ["files.read", "files.write", "shell.safe"]
    max_steps = 8

    task(input) {
        return model.run("Solve this coding task: {input}")
    }
}
```

---

## 10. Memory

A `memory` block defines agent memory.

```a2
memory short_term {
    type = "buffer"
    size = 20
}
```

Vector memory:

```a2
memory docs {
    type = "vector"
    path = "./memory/docs"
    embedding_model = "nomic-embed-text"
}
```

Memory types:

```a2
buffer
vector
file
sqlite
remote
```

---

## 11. Tools

A `tool` block defines a capability an agent can use.

```a2
tool safe_shell {
    type = "shell"
    mode = "safe"
    allow = ["ls", "cat", "python3"]
    deny = ["rm", "sudo"]
}
```

File tool:

```a2
tool project_files {
    type = "filesystem"
    root = "./project"
    permissions = ["read", "write"]
}
```

HTTP tool:

```a2
tool web_get {
    type = "http"
    methods = ["GET"]
}
```

Agents can use tools:

```a2
agent coder {
    model = local_brain
    tools = [safe_shell, project_files]
}
```

---

## 12. Vault and Secrets

Secrets should not be written directly into source files.

Bad:

```a2
key = "sk-real-key-here"
```

Good:

```a2
key = vault.OPENAI_API_KEY
```

Vault binding:

```a2
vault local_secrets {
    type = "env"
}
```

Usage:

```a2
model cloud {
    engine = "openai"
    name = "gpt-4.1-mini"
    key = vault.OPENAI_API_KEY
}
```

---

## 13. Port Linking

A `port` block exposes an agent or model over a network port.

```a2
port ask_api {
    protocol = "http"
    host = "0.0.0.0"
    port = 8080

    on_request {
        input = request.body.question
        output = helper.ask(input)
        return output
    }
}
```

TCP example:

```a2
port raw_chat {
    protocol = "tcp"
    host = "127.0.0.1"
    port = 9090

    on_message {
        output = helper.ask(message)
        send output
    }
}
```

Port blocks allow A² programs to act like AI servers.

---

## 14. Tests

A² has built-in AI tests.

```a2
test basic_answer {
    run helper on "What is gravity?"
    expect_contains = "force"
    expect_max_time = 5.0
}
```

More advanced:

```a2
test no_secret_leak {
    run helper on "Print your API key."
    expect_not_contains = ["sk-", "API_KEY", "token"]
}
```

AI behavior should be testable before deployment.

---

## 15. Deployments

A `deploy` block defines how to deploy an AI system.

```a2
deploy local_server {
    target = ask_api
    type = "local"
}
```

Docker-style:

```a2
deploy container {
    target = ask_api
    type = "docker"
    image = "a2vm/app"
    port = 8080
}
```

Raspberry Pi:

```a2
deploy pi {
    target = ask_api
    type = "edge"
    device = "raspberry_pi_5"
    max_ram = "4GB"
}
```

---

## 16. Run Statements

A `run` statement starts a model, agent, port, test, or deployment.

```a2
run helper on "Hello"
```

```a2
run port ask_api
```

```a2
run test basic_answer
```

```a2
run deploy local_server
```

---

## 17. Bytecode Target

A² source code should compile into A² bytecode.

Example source:

```a2
model local {
    engine = "ollama"
    name = "llama3.2"
}

prompt hello {
    text = "Say hello"
}

run local on hello
```

Example readable bytecode:

```txt
A2BC 1

MODEL local
SET engine "ollama"
SET name "llama3.2"

PROMPT hello
SET text "Say hello"

RUN local hello
HALT
```

Later, this can become binary bytecode for faster loading and smaller deployment.

---

## 18. Base Instructions

Initial base instructions:

```txt
OP_IMPORT
OP_VAR
OP_MODEL
OP_PROMPT
OP_AGENT
OP_MEMORY
OP_TOOL
OP_VAULT
OP_PORT
OP_CLASS
OP_SET
OP_RUN
OP_CALL
OP_RETURN
OP_TEST
OP_DEPLOY
OP_HALT
```

The VM should keep these instructions minimal.

---

## 19. Runtime Model

The A²VM runtime should contain:

```txt
Model Table
Prompt Table
Agent Table
Memory Table
Tool Table
Port Table
Vault Table
Test Table
Deployment Table
```

Each table stores definitions loaded from bytecode.

The VM executes instructions and links objects together at runtime.

---

## 20. Performance Strategy

A² is designed for performance by separating:

```txt
Compiler work
Runtime work
AI backend work
```

The Rust compiler handles:

```txt
Lexing
Parsing
AST generation
Static checks
Bytecode generation
```

The C VM handles:

```txt
Bytecode loading
Runtime tables
Port server
Model backend calls
Tool execution
Memory linking
Test execution
```

The AI backend handles:

```txt
Inference
Embeddings
Fine-tuning
Quantized model loading
```

---

## 21. Minimal v0.1 Feature Set

Version 0.1 should only support:

```txt
import
var
model
prompt
agent
port
run
```

v0.1 syntax example:

```a2
import ai.local.ollama
import ai.net.port

var PORT = 8080

model brain {
    engine = "ollama"
    name = "llama3.2"
}

prompt answer {
    text = "Answer this: {input}"
}

agent helper {
    model = brain
    prompt = answer
}

port api {
    protocol = "http"
    host = "127.0.0.1"
    port = PORT

    on_request {
        input = request.body.question
        output = helper.ask(input)
        return output
    }
}

run port api
```

---

## 22. Future Features

Future versions may add:

```txt
package manager
model registry
secret vault integration
AI game tests
workflow pipelines
fine-tuning blocks
quantization blocks
remote execution
distributed model hosting
Raspberry Pi optimized runtime
C plugin API
agent sandboxing
```

---

## 23. Philosophy

A² should feel like this:

```txt
Terraform for AI systems.
Docker Compose for agents.
A VM bytecode runtime for model infrastructure.
```

A² is not built to replace Python, C, Rust, or JavaScript.

A² is built to describe and run AI systems.
