use crate::ast::{CompileError, ObjectType, Program, Property, RunStmt, Value};
use ainfra_optimizer::{compact_prompt, fingerprint, normalize_engine};

const MAGIC: &[u8; 4] = b"AIF0";
const VERSION: u16 = 1;

#[derive(Debug, Clone)]
struct AifPointer {
    pointer_type: String,
    target_object_id: String,
}

#[derive(Debug, Clone)]
struct AifObject {
    object_id: String,
    name: String,
    object_type: String,
    start: bool,
    properties: Vec<Property>,
    pointers: Vec<AifPointer>,
}

pub fn write_aif(program: &Program, namespace: &str) -> Result<Vec<u8>, CompileError> {
    let objects = build_objects(program, namespace);
    let mut writer = Writer::new();
    writer.bytes.extend_from_slice(MAGIC);
    writer.u16(VERSION);
    writer.u32(objects.len() as u32);
    for object in objects {
        writer.string(&object.object_id)?;
        writer.string(&object.name)?;
        writer.string(&object.object_type)?;
        writer.u8(u8::from(object.start));
        writer.u32(object.properties.len() as u32);
        for prop in &object.properties {
            writer.string(&prop.key)?;
            writer.value(&prop.value)?;
        }
        writer.u32(object.pointers.len() as u32);
        for pointer in &object.pointers {
            writer.string(&pointer.pointer_type)?;
            writer.string(&pointer.target_object_id)?;
        }
        writer.u32(0);
    }
    Ok(writer.bytes)
}

fn build_objects(program: &Program, namespace: &str) -> Vec<AifObject> {
    let mut objects = Vec::new();
    for block in &program.blocks {
        let mut pointers = Vec::new();
        if block.kind == ObjectType::Agent {
            if let Some(model) = prop_ref(&block.properties, "model") {
                pointers.push(AifPointer {
                    pointer_type: "uses".to_string(),
                    target_object_id: object_id(namespace, ObjectType::Model, model),
                });
            }
            if let Some(prompt) = prop_ref(&block.properties, "prompt") {
                pointers.push(AifPointer {
                    pointer_type: "uses".to_string(),
                    target_object_id: object_id(namespace, ObjectType::Prompt, prompt),
                });
            }
        }
        if block.kind == ObjectType::Port {
            if let Some(agent) = prop_ref(&block.properties, "agent")
                .or_else(|| prop_ref(&block.properties, "route"))
            {
                pointers.push(AifPointer {
                    pointer_type: "routes".to_string(),
                    target_object_id: object_id(namespace, ObjectType::Agent, agent),
                });
            }
        }
        let mut properties = optimize_properties(&block.properties);
        properties.push(Property {
            key: "optimizer_hash".to_string(),
            value: Value::String(fingerprint(&[
                block.kind.as_str(),
                &block.name,
                &format!("{}", properties.len()),
            ])),
        });
        pointers.sort_by(|a, b| {
            (&a.pointer_type, &a.target_object_id).cmp(&(&b.pointer_type, &b.target_object_id))
        });
        pointers.dedup_by(|a, b| {
            a.pointer_type == b.pointer_type && a.target_object_id == b.target_object_id
        });
        objects.push(AifObject {
            object_id: object_id(namespace, block.kind, &block.name),
            name: block.name.clone(),
            object_type: block.kind.as_str().to_string(),
            start: false,
            properties,
            pointers,
        });
    }

    let any_start = program.runs.iter().any(|run| run.start);
    for (index, run) in program.runs.iter().enumerate() {
        objects.push(run_object(
            run,
            index,
            run.start || (!any_start && index == 0),
            namespace,
        ));
    }
    objects.sort_by(|a, b| (&a.object_type, &a.name).cmp(&(&b.object_type, &b.name)));
    objects
}

fn optimize_properties(properties: &[Property]) -> Vec<Property> {
    properties
        .iter()
        .map(|prop| {
            let value = match (&prop.key[..], &prop.value) {
                ("engine", Value::String(s)) | ("engine", Value::Ref(s)) => {
                    Value::String(normalize_engine(s))
                }
                ("text", Value::String(s)) => Value::String(compact_prompt(s)),
                _ => prop.value.clone(),
            };
            Property {
                key: prop.key.clone(),
                value,
            }
        })
        .collect()
}

fn run_object(run: &RunStmt, index: usize, start: bool, namespace: &str) -> AifObject {
    let kind = run.kind.as_deref().unwrap_or("agent");
    let target_type = match kind {
        "model" => ObjectType::Model,
        "port" => ObjectType::Port,
        _ => ObjectType::Agent,
    };
    let mut properties = vec![
        Property {
            key: "kind".to_string(),
            value: Value::String(kind.to_string()),
        },
        Property {
            key: "target".to_string(),
            value: Value::String(run.target.clone()),
        },
    ];
    if let Some(input) = &run.input {
        properties.push(Property {
            key: "input".to_string(),
            value: Value::String(input.clone()),
        });
    }
    AifObject {
        object_id: namespaced_id(namespace, &format!("run:{}", index + 1)),
        name: format!("run_{}", index + 1),
        object_type: "run".to_string(),
        start,
        properties,
        pointers: vec![AifPointer {
            pointer_type: "runs".to_string(),
            target_object_id: object_id(namespace, target_type, &run.target),
        }],
    }
}

fn object_id(namespace: &str, kind: ObjectType, name: &str) -> String {
    namespaced_id(namespace, &format!("{}:{name}", kind.as_str()))
}

fn namespaced_id(namespace: &str, local_id: &str) -> String {
    if namespace.is_empty() {
        local_id.to_string()
    } else {
        format!("{namespace}::{local_id}")
    }
}

fn prop_ref<'a>(properties: &'a [Property], key: &str) -> Option<&'a str> {
    properties.iter().find_map(|prop| {
        if prop.key == key {
            match &prop.value {
                Value::Ref(s) | Value::String(s) => Some(s.as_str()),
                _ => None,
            }
        } else {
            None
        }
    })
}

struct Writer {
    bytes: Vec<u8>,
}

impl Writer {
    fn new() -> Self {
        Self { bytes: Vec::new() }
    }

    fn value(&mut self, value: &Value) -> Result<(), CompileError> {
        match value {
            Value::String(s) => {
                self.u8(1);
                self.string(s)?;
            }
            Value::Number(n) => {
                self.u8(2);
                self.string(n)?;
            }
            Value::Bool(b) => {
                self.u8(3);
                self.u8(u8::from(*b));
            }
            Value::Ref(r) => {
                self.u8(4);
                self.string(r)?;
            }
            Value::List(items) => {
                self.u8(5);
                self.u32(items.len() as u32);
                for item in items {
                    self.value(item)?;
                }
            }
        }
        Ok(())
    }

    fn string(&mut self, text: &str) -> Result<(), CompileError> {
        let len =
            u32::try_from(text.len()).map_err(|_| CompileError::new(0, 0, "string too large"))?;
        self.u32(len);
        self.bytes.extend_from_slice(text.as_bytes());
        Ok(())
    }

    fn u8(&mut self, value: u8) {
        self.bytes.push(value);
    }

    fn u16(&mut self, value: u16) {
        self.bytes.extend_from_slice(&value.to_le_bytes());
    }

    fn u32(&mut self, value: u32) {
        self.bytes.extend_from_slice(&value.to_le_bytes());
    }
}
