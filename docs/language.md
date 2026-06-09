# AInfra Language v0.1

AInfra is a small DSL for declaring AI infrastructure objects.

Supported top-level forms:

- `import`
- `var`
- `model`
- `prompt`
- `agent`
- `port`
- `run`

The compiler turns AInfra source into AIF binary objects.

Supported `model.engine` values:

- `openai`
- `anthropic`
- `gemini`
- `microsoft`
- `deepseek`
- `huggingface`
- `ollama`

```ainfra
import ai.remote.openai

// $start$

model cloud {
    engine = "openai"
    name = "gpt-4.1-mini"
}

prompt answer {
    text = "Answer clearly: {input}"
}

agent helper {
    model = cloud
    prompt = answer
}

run agent helper on "What is InfraVM?"
```

The `// $start$` marker marks the first `run` object as the start object. If no marker exists, the compiler marks the first `run` object as start.
