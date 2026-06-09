# PointRun

PointRun is an InfraVM feature that runs an AIF object graph by following object pointers.

v0.1 behavior:

1. Start at a selected object or the object marked `start`.
2. If the object is `run`, follow its `runs` pointer.
3. If the object is `agent`, resolve its `model` and `prompt`.
4. Replace `{input}` in the prompt text.
5. If `model.engine == "openai"`, call `vm_openai.h`.
6. If the engine is unsupported, print a clear stub message.

OpenAI keys are read only from `OPENAI_API_KEY`.
