mod aif_writer;
mod ast;
mod lexer;
mod parser;

use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use aif_writer::write_aif;
use lexer::Lexer;
use parser::Parser;

fn usage(program: &str) -> String {
    format!(
        "Usage:\n  {program} <input.ainfra> [-o output.aif] [--namespace name]\n  {program} --help"
    )
}

fn parse_args() -> Result<(PathBuf, PathBuf, Option<String>), String> {
    let mut args = env::args().collect::<Vec<_>>();
    let program = args.remove(0);
    if args.is_empty() || args.iter().any(|arg| arg == "-h" || arg == "--help") {
        return Err(usage(&program));
    }
    let input = PathBuf::from(args.remove(0));
    let mut output = input.with_extension("aif");
    let mut namespace = None;
    while !args.is_empty() {
        match args.remove(0).as_str() {
            "-o" => {
                if args.is_empty() {
                    return Err(usage(&program));
                }
                output = PathBuf::from(args.remove(0));
            }
            "--namespace" => {
                if args.is_empty() {
                    return Err(usage(&program));
                }
                namespace = Some(args.remove(0));
            }
            _ => return Err(usage(&program)),
        }
    }
    Ok((input, output, namespace))
}

fn namespace_from_path(path: &Path) -> String {
    let stem = path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("workspace");
    let mut out = String::new();
    for ch in stem.chars() {
        if ch.is_ascii_alphanumeric() || ch == '_' || ch == '-' || ch == '.' {
            out.push(ch);
        } else if !out.ends_with('-') {
            out.push('-');
        }
    }
    out.trim_matches('-').to_string()
}

fn main() {
    let (input, output, namespace_arg) = match parse_args() {
        Ok(paths) => paths,
        Err(message) => {
            eprintln!("{message}");
            std::process::exit(if message.starts_with("Usage") { 0 } else { 2 });
        }
    };

    let source = match fs::read_to_string(&input) {
        Ok(source) => source,
        Err(err) => {
            eprintln!("failed to read {}: {err}", input.display());
            std::process::exit(1);
        }
    };

    let tokens = match Lexer::new(&source).tokenize() {
        Ok(tokens) => tokens,
        Err(err) => {
            eprintln!("compile error: {err}");
            std::process::exit(1);
        }
    };

    let program = match Parser::new(tokens, &source).parse() {
        Ok(program) => program,
        Err(err) => {
            eprintln!("compile error: {err}");
            std::process::exit(1);
        }
    };

    let namespace = namespace_arg.unwrap_or_else(|| namespace_from_path(&output));
    let bytes = match write_aif(&program, &namespace) {
        Ok(bytes) => bytes,
        Err(err) => {
            eprintln!("compile error: {err}");
            std::process::exit(1);
        }
    };

    if let Err(err) = fs::write(&output, bytes) {
        eprintln!("failed to write {}: {err}", output.display());
        std::process::exit(1);
    }
    println!("wrote {}", output.display());
}
