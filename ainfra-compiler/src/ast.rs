use std::fmt;

#[derive(Debug, Clone)]
pub struct CompileError {
    pub line: usize,
    pub col: usize,
    pub message: String,
}

impl CompileError {
    pub fn new(line: usize, col: usize, message: impl Into<String>) -> Self {
        Self {
            line,
            col,
            message: message.into(),
        }
    }
}

impl fmt::Display for CompileError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}:{}: {}", self.line, self.col, self.message)
    }
}

impl std::error::Error for CompileError {}

#[derive(Debug, Clone)]
pub enum Value {
    String(String),
    Number(String),
    Bool(bool),
    Ref(String),
    List(Vec<Value>),
}

#[derive(Debug, Clone)]
pub struct Property {
    pub key: String,
    pub value: Value,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ObjectType {
    Model,
    Prompt,
    Agent,
    Port,
}

impl ObjectType {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Model => "model",
            Self::Prompt => "prompt",
            Self::Agent => "agent",
            Self::Port => "port",
        }
    }
}

#[derive(Debug, Clone)]
pub struct Block {
    pub kind: ObjectType,
    pub name: String,
    pub properties: Vec<Property>,
    pub line: usize,
    pub col: usize,
}

#[derive(Debug, Clone)]
pub struct RunStmt {
    pub kind: Option<String>,
    pub target: String,
    pub input: Option<String>,
    pub start: bool,
    pub line: usize,
    pub col: usize,
}

#[derive(Debug, Default, Clone)]
pub struct Program {
    pub imports: Vec<String>,
    pub vars: Vec<Property>,
    pub blocks: Vec<Block>,
    pub runs: Vec<RunStmt>,
}

impl Program {
    pub fn find_block(&self, kind: ObjectType, name: &str) -> Option<&Block> {
        self.blocks
            .iter()
            .find(|b| b.kind == kind && b.name == name)
    }
}
