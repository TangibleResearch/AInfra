use crate::ast::CompileError;

#[derive(Debug, Clone, PartialEq)]
pub enum TokenKind {
    Ident(String),
    String(String),
    Number(String),
    Bool(bool),
    Import,
    Var,
    Model,
    Prompt,
    Agent,
    Port,
    Run,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Comma,
    Equal,
    Dot,
    Eof,
}

#[derive(Debug, Clone)]
pub struct Token {
    pub kind: TokenKind,
    pub line: usize,
    pub col: usize,
}

pub struct Lexer {
    chars: Vec<char>,
    pos: usize,
    line: usize,
    col: usize,
}

impl Lexer {
    pub fn new(source: &str) -> Self {
        Self {
            chars: source.chars().collect(),
            pos: 0,
            line: 1,
            col: 1,
        }
    }

    pub fn tokenize(mut self) -> Result<Vec<Token>, CompileError> {
        let mut tokens = Vec::new();
        loop {
            self.skip_ws_and_comments();
            let line = self.line;
            let col = self.col;
            let kind = match self.peek() {
                None => TokenKind::Eof,
                Some('"') => TokenKind::String(self.lex_string()?),
                Some('{') => {
                    self.bump();
                    TokenKind::LBrace
                }
                Some('}') => {
                    self.bump();
                    TokenKind::RBrace
                }
                Some('[') => {
                    self.bump();
                    TokenKind::LBracket
                }
                Some(']') => {
                    self.bump();
                    TokenKind::RBracket
                }
                Some(',') => {
                    self.bump();
                    TokenKind::Comma
                }
                Some('=') => {
                    self.bump();
                    TokenKind::Equal
                }
                Some('.') => {
                    self.bump();
                    TokenKind::Dot
                }
                Some(c) if c.is_ascii_digit() || c == '-' => TokenKind::Number(self.lex_number()?),
                Some(c) if is_ident_start(c) => self.lex_ident_or_keyword(),
                Some(c) => {
                    return Err(CompileError::new(
                        line,
                        col,
                        format!("unexpected character `{c}`"),
                    ));
                }
            };
            let eof = kind == TokenKind::Eof;
            tokens.push(Token { kind, line, col });
            if eof {
                return Ok(tokens);
            }
        }
    }

    fn peek(&self) -> Option<char> {
        self.chars.get(self.pos).copied()
    }

    fn peek_next(&self) -> Option<char> {
        self.chars.get(self.pos + 1).copied()
    }

    fn bump(&mut self) -> Option<char> {
        let ch = self.peek()?;
        self.pos += 1;
        if ch == '\n' {
            self.line += 1;
            self.col = 1;
        } else {
            self.col += 1;
        }
        Some(ch)
    }

    fn skip_ws_and_comments(&mut self) {
        loop {
            while matches!(self.peek(), Some(c) if c.is_whitespace()) {
                self.bump();
            }
            if self.peek() == Some('/') && self.peek_next() == Some('/') {
                while !matches!(self.peek(), None | Some('\n')) {
                    self.bump();
                }
                continue;
            }
            if self.peek() == Some('#') {
                while !matches!(self.peek(), None | Some('\n')) {
                    self.bump();
                }
                continue;
            }
            break;
        }
    }

    fn lex_string(&mut self) -> Result<String, CompileError> {
        let line = self.line;
        let col = self.col;
        self.bump();
        let mut out = String::new();
        while let Some(ch) = self.bump() {
            match ch {
                '"' => return Ok(out),
                '\\' => {
                    let escaped = self
                        .bump()
                        .ok_or_else(|| CompileError::new(line, col, "unterminated string"))?;
                    match escaped {
                        'n' => out.push('\n'),
                        'r' => out.push('\r'),
                        't' => out.push('\t'),
                        '"' => out.push('"'),
                        '\\' => out.push('\\'),
                        other => {
                            return Err(CompileError::new(
                                self.line,
                                self.col,
                                format!("unsupported escape `\\{other}`"),
                            ));
                        }
                    }
                }
                other => out.push(other),
            }
        }
        Err(CompileError::new(line, col, "unterminated string"))
    }

    fn lex_number(&mut self) -> Result<String, CompileError> {
        let line = self.line;
        let col = self.col;
        let mut out = String::new();
        if self.peek() == Some('-') {
            out.push('-');
            self.bump();
        }
        let mut saw_digit = false;
        while matches!(self.peek(), Some(c) if c.is_ascii_digit()) {
            saw_digit = true;
            out.push(self.bump().unwrap());
        }
        if self.peek() == Some('.') && matches!(self.peek_next(), Some(c) if c.is_ascii_digit()) {
            out.push(self.bump().unwrap());
            while matches!(self.peek(), Some(c) if c.is_ascii_digit()) {
                out.push(self.bump().unwrap());
            }
        }
        if !saw_digit {
            return Err(CompileError::new(line, col, "expected number after `-`"));
        }
        Ok(out)
    }

    fn lex_ident_or_keyword(&mut self) -> TokenKind {
        let mut out = String::new();
        while matches!(self.peek(), Some(c) if is_ident_continue(c)) {
            out.push(self.bump().unwrap());
        }
        match out.as_str() {
            "import" => TokenKind::Import,
            "var" => TokenKind::Var,
            "model" => TokenKind::Model,
            "prompt" => TokenKind::Prompt,
            "agent" => TokenKind::Agent,
            "port" => TokenKind::Port,
            "run" => TokenKind::Run,
            "true" => TokenKind::Bool(true),
            "false" => TokenKind::Bool(false),
            _ => TokenKind::Ident(out),
        }
    }
}

fn is_ident_start(c: char) -> bool {
    c.is_ascii_alphabetic() || c == '_'
}

fn is_ident_continue(c: char) -> bool {
    c.is_ascii_alphanumeric() || c == '_'
}

pub fn name_text(kind: &TokenKind) -> Option<&str> {
    match kind {
        TokenKind::Ident(s) => Some(s.as_str()),
        TokenKind::Import => Some("import"),
        TokenKind::Var => Some("var"),
        TokenKind::Model => Some("model"),
        TokenKind::Prompt => Some("prompt"),
        TokenKind::Agent => Some("agent"),
        TokenKind::Port => Some("port"),
        TokenKind::Run => Some("run"),
        _ => None,
    }
}
