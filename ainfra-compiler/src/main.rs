mod aif_writer;
mod ast;
mod lexer;
mod parser;

use std::env;
use std::fs;
use std::path::PathBuf;

use aif_writer::write_aif;
use lexer::Lexer;
use parser::Parser;

fn usage(program: &str) -> String {
    format!("Usage:\n  {program} <input.ainfra> [-o output.aif]\n  {program} --help")
}

fn parse_args() -> Result<(PathBuf, PathBuf), String> {
    let mut args = env::args().collect::<Vec<_>>();
    let program = args.remove(0);
    if args.is_empty() || args.iter().any(|arg| arg == "-h" || arg == "--help") {
        return Err(usage(&program));
    }
    let input = PathBuf::from(args.remove(0));
    let output = if args.first().map(String::as_str) == Some("-o") {
        if args.len() != 2 {
            return Err(usage(&program));
        }
        PathBuf::from(args.remove(1))
    } else if args.is_empty() {
        input.with_extension("aif")
    } else {
        return Err(usage(&program));
    };
    Ok((input, output))
}

fn main() {
    let (input, output) = match parse_args() {
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

    let bytes = match write_aif(&program) {
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
