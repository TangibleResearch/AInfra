#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Provider {
    OpenAI,
    Anthropic,
    Gemini,
    Microsoft,
    DeepSeek,
    HuggingFace,
    Ollama,
    Unknown,
}

pub fn provider_from_engine(engine: &str) -> Provider {
    match normalize_engine(engine).as_str() {
        "openai" => Provider::OpenAI,
        "anthropic" | "claude" => Provider::Anthropic,
        "gemini" | "google" | "google-gemini" => Provider::Gemini,
        "microsoft" | "azure" | "azure-openai" => Provider::Microsoft,
        "deepseek" => Provider::DeepSeek,
        "huggingface" | "hf" => Provider::HuggingFace,
        "ollama" => Provider::Ollama,
        _ => Provider::Unknown,
    }
}

pub fn normalize_engine(engine: &str) -> String {
    engine.trim().to_ascii_lowercase().replace('_', "-")
}

pub fn compact_prompt(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    let mut last_was_space = false;
    for ch in text.chars() {
        if ch.is_whitespace() {
            if !last_was_space {
                out.push(' ');
                last_was_space = true;
            }
        } else {
            out.push(ch);
            last_was_space = false;
        }
    }
    out.trim().to_string()
}

pub fn fnv1a64(text: &str) -> u64 {
    let mut hash = 0xcbf29ce484222325u64;
    for byte in text.as_bytes() {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    hash
}

pub fn fingerprint(parts: &[&str]) -> String {
    let mut hash = 0xcbf29ce484222325u64;
    for part in parts {
        for byte in part.as_bytes() {
            hash ^= u64::from(*byte);
            hash = hash.wrapping_mul(0x100000001b3);
        }
        hash ^= 0xff;
        hash = hash.wrapping_mul(0x100000001b3);
    }
    format!("{hash:016x}")
}

pub fn sorted_unique(mut items: Vec<String>) -> Vec<String> {
    items.sort();
    items.dedup();
    items
}
