use crate::ast::{Block, CompileError, ObjectType, Program, Property, RunStmt, Value};
use crate::lexer::{Token, TokenKind, name_text};

pub struct Parser {
    tokens: Vec<Token>,
    pos: usize,
    start_marker: bool,
}

impl Parser {
    pub fn new(tokens: Vec<Token>, source: &str) -> Self {
        Self {
            tokens,
            pos: 0,
            start_marker: source.contains("// $start$"),
        }
    }

    pub fn parse(&mut self) -> Result<Program, CompileError> {
        let mut program = Program::default();
        while !self.at(&TokenKind::Eof) {
            match self.peek().kind.clone() {
                TokenKind::Import => program.imports.push(self.parse_import()?),
                TokenKind::Var => program.vars.push(self.parse_var()?),
                TokenKind::Model => program.blocks.push(self.parse_block(ObjectType::Model)?),
                TokenKind::Prompt => program.blocks.push(self.parse_block(ObjectType::Prompt)?),
                TokenKind::Agent => program.blocks.push(self.parse_block(ObjectType::Agent)?),
                TokenKind::Port => program.blocks.push(self.parse_block(ObjectType::Port)?),
                TokenKind::Run => program.runs.push(self.parse_run(program.runs.is_empty())?),
                other => {
                    let tok = self.peek();
                    return Err(CompileError::new(
                        tok.line,
                        tok.col,
                        format!("unexpected top-level token {other:?}"),
                    ));
                }
            }
        }
        validate(&program)?;
        Ok(program)
    }

    fn parse_import(&mut self) -> Result<String, CompileError> {
        self.expect_simple(TokenKind::Import)?;
        self.parse_dotted_name()
    }

    fn parse_var(&mut self) -> Result<Property, CompileError> {
        self.expect_simple(TokenKind::Var)?;
        let key = self.expect_name()?;
        self.expect_simple(TokenKind::Equal)?;
        let value = self.parse_value()?;
        Ok(Property { key, value })
    }

    fn parse_block(&mut self, kind: ObjectType) -> Result<Block, CompileError> {
        let start = self.peek().clone();
        self.bump();
        let name = self.expect_name()?;
        self.expect_simple(TokenKind::LBrace)?;
        let mut properties = Vec::new();
        while !self.at(&TokenKind::RBrace) {
            if self.at(&TokenKind::Eof) {
                let tok = self.peek();
                return Err(CompileError::new(tok.line, tok.col, "unterminated block"));
            }
            let key = self.expect_name()?;
            self.expect_simple(TokenKind::Equal)?;
            let value = self.parse_value()?;
            properties.push(Property { key, value });
        }
        self.expect_simple(TokenKind::RBrace)?;
        Ok(Block {
            kind,
            name,
            properties,
            line: start.line,
            col: start.col,
        })
    }

    fn parse_run(&mut self, first_run: bool) -> Result<RunStmt, CompileError> {
        let start = self.peek().clone();
        self.expect_simple(TokenKind::Run)?;
        let first = self.expect_name()?;
        let (kind, target) = if matches!(first.as_str(), "model" | "agent" | "port") {
            (Some(first), self.expect_name()?)
        } else {
            (None, first)
        };
        let input = if self.peek_name() == Some("on") {
            self.bump();
            Some(match self.peek().kind.clone() {
                TokenKind::String(s) => {
                    self.bump();
                    s
                }
                _ => self.parse_dotted_name()?,
            })
        } else {
            None
        };
        Ok(RunStmt {
            kind,
            target,
            input,
            start: self.start_marker && first_run,
            line: start.line,
            col: start.col,
        })
    }

    fn parse_value(&mut self) -> Result<Value, CompileError> {
        match self.peek().kind.clone() {
            TokenKind::String(s) => {
                self.bump();
                Ok(Value::String(s))
            }
            TokenKind::Number(n) => {
                self.bump();
                Ok(Value::Number(n))
            }
            TokenKind::Bool(b) => {
                self.bump();
                Ok(Value::Bool(b))
            }
            TokenKind::Ident(_)
            | TokenKind::Model
            | TokenKind::Prompt
            | TokenKind::Agent
            | TokenKind::Port => Ok(Value::Ref(self.parse_dotted_name()?)),
            TokenKind::LBracket => self.parse_list(),
            other => {
                let tok = self.peek();
                Err(CompileError::new(
                    tok.line,
                    tok.col,
                    format!("expected value, found {other:?}"),
                ))
            }
        }
    }

    fn parse_list(&mut self) -> Result<Value, CompileError> {
        self.expect_simple(TokenKind::LBracket)?;
        let mut items = Vec::new();
        while !self.at(&TokenKind::RBracket) {
            items.push(self.parse_value()?);
            if self.at(&TokenKind::Comma) {
                self.bump();
            } else if !self.at(&TokenKind::RBracket) {
                let tok = self.peek();
                return Err(CompileError::new(tok.line, tok.col, "expected `,` or `]`"));
            }
        }
        self.expect_simple(TokenKind::RBracket)?;
        Ok(Value::List(items))
    }

    fn parse_dotted_name(&mut self) -> Result<String, CompileError> {
        let mut out = self.expect_name()?;
        while self.at(&TokenKind::Dot) {
            self.bump();
            out.push('.');
            out.push_str(&self.expect_name()?);
        }
        Ok(out)
    }

    fn peek(&self) -> &Token {
        &self.tokens[self.pos]
    }

    fn bump(&mut self) {
        if !self.at(&TokenKind::Eof) {
            self.pos += 1;
        }
    }

    fn at(&self, expected: &TokenKind) -> bool {
        std::mem::discriminant(&self.peek().kind) == std::mem::discriminant(expected)
    }

    fn expect_simple(&mut self, expected: TokenKind) -> Result<(), CompileError> {
        if self.at(&expected) {
            self.bump();
            Ok(())
        } else {
            let tok = self.peek();
            Err(CompileError::new(
                tok.line,
                tok.col,
                format!("expected {:?}, found {:?}", expected, tok.kind),
            ))
        }
    }

    fn expect_name(&mut self) -> Result<String, CompileError> {
        if let Some(text) = name_text(&self.peek().kind) {
            let out = text.to_string();
            self.bump();
            Ok(out)
        } else {
            let tok = self.peek();
            Err(CompileError::new(tok.line, tok.col, "expected name"))
        }
    }

    fn peek_name(&self) -> Option<&str> {
        name_text(&self.peek().kind)
    }
}

fn validate(program: &Program) -> Result<(), CompileError> {
    for block in &program.blocks {
        if block.kind == ObjectType::Model && prop_text(block, "engine").is_none() {
            return Err(CompileError::new(
                block.line,
                block.col,
                "model.engine is required",
            ));
        }
        if block.kind == ObjectType::Agent {
            let model = required_ref(block, "model")?;
            if program.find_block(ObjectType::Model, model).is_none() {
                return Err(CompileError::new(
                    block.line,
                    block.col,
                    format!("agent.model `{model}` does not reference a model"),
                ));
            }
            if let Some(prompt) = prop_text(block, "prompt") {
                if program.find_block(ObjectType::Prompt, prompt).is_none() {
                    return Err(CompileError::new(
                        block.line,
                        block.col,
                        format!("agent.prompt `{prompt}` does not reference a prompt"),
                    ));
                }
            }
        }
    }
    for run in &program.runs {
        let kind = run.kind.as_deref().unwrap_or("agent");
        let object_type = match kind {
            "model" => ObjectType::Model,
            "agent" => ObjectType::Agent,
            "port" => ObjectType::Port,
            other => {
                return Err(CompileError::new(
                    run.line,
                    run.col,
                    format!("unsupported run kind `{other}`"),
                ));
            }
        };
        if program.find_block(object_type, &run.target).is_none() {
            return Err(CompileError::new(
                run.line,
                run.col,
                format!("run target `{}` does not exist", run.target),
            ));
        }
    }
    Ok(())
}

fn required_ref<'a>(block: &'a Block, key: &str) -> Result<&'a str, CompileError> {
    prop_text(block, key).ok_or_else(|| {
        CompileError::new(
            block.line,
            block.col,
            format!("{}.{} is required", block.kind.as_str(), key),
        )
    })
}

fn prop_text<'a>(block: &'a Block, key: &str) -> Option<&'a str> {
    block.properties.iter().find_map(|p| {
        if p.key == key {
            match &p.value {
                Value::String(s) | Value::Number(s) | Value::Ref(s) => Some(s.as_str()),
                _ => None,
            }
        } else {
            None
        }
    })
}
