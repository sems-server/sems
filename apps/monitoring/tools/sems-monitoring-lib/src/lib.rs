pub use std::collections::BTreeMap;
pub use xmlrpc::{Request, Transport, Value};

use std::io::{BufRead, BufReader, Cursor, Read, Write};
use std::net::TcpStream;

const DEFAULT_URL: &str = "http://localhost:8090";

pub fn default_url() -> &'static str {
    DEFAULT_URL
}

/// Parse `--url <URL>` from the argument list.
/// Returns the URL (or default) and the remaining arguments.
pub fn parse_url_arg(args: &[String]) -> (String, Vec<String>) {
    let mut url = DEFAULT_URL.to_string();
    let mut rest = Vec::new();
    let mut i = 1; // skip argv[0]
    while i < args.len() {
        if args[i] == "--url" {
            if i + 1 < args.len() {
                url = args[i + 1].clone();
                i += 2;
            } else {
                eprintln!("error: --url requires a value");
                std::process::exit(1);
            }
        } else {
            rest.push(args[i].clone());
            i += 1;
        }
    }
    (url, rest)
}

/// XMLRPC transport using raw TCP (no external HTTP library needed).
/// Only supports plain HTTP to localhost — no TLS.
struct TcpTransport<'a> {
    url: &'a str,
}

impl<'a> Transport for TcpTransport<'a> {
    type Stream = Cursor<Vec<u8>>;

    fn transmit(
        self,
        request: &Request<'_>,
    ) -> Result<Self::Stream, Box<dyn std::error::Error + Send + Sync>> {
        let host_port = self.url.strip_prefix("http://").unwrap_or(self.url);

        let mut stream = TcpStream::connect(host_port)?;

        let mut body = Vec::new();
        request.write_as_xml(&mut body).unwrap();

        write!(stream, "POST / HTTP/1.0\r\n")?;
        write!(stream, "Host: {}\r\n", host_port)?;
        write!(stream, "Content-Type: text/xml; charset=utf-8\r\n")?;
        write!(stream, "Content-Length: {}\r\n", body.len())?;
        write!(stream, "User-Agent: Rust xmlrpc\r\n")?;
        write!(stream, "\r\n")?;
        stream.write_all(&body)?;
        stream.flush()?;

        // Skip HTTP response headers
        let mut reader = BufReader::new(stream);
        let mut line = String::new();
        loop {
            line.clear();
            reader.read_line(&mut line)?;
            if line.trim().is_empty() {
                break;
            }
        }

        let mut resp_body = Vec::new();
        reader.read_to_end(&mut resp_body)?;
        Ok(Cursor::new(resp_body))
    }
}

/// Call the `calls()` XMLRPC method and return the active call count.
pub fn calls(url: &str) -> Result<i64, Box<dyn std::error::Error>> {
    let req = Request::new("calls");
    let resp = req.call(TcpTransport { url })?;
    match resp {
        Value::Int(n) => Ok(n as i64),
        Value::Int64(n) => Ok(n),
        _ => Err(format!("unexpected response type for calls(): {:?}", resp).into()),
    }
}

/// Call the `di(...)` XMLRPC method with the given string arguments.
pub fn di(url: &str, args: &[&str]) -> Result<Value, Box<dyn std::error::Error>> {
    let mut req = Request::new("di");
    for arg in args {
        req = req.arg(Value::from(*arg));
    }
    Ok(req.call(TcpTransport { url })?)
}

/// Extract string values from an XMLRPC Array value, returning them as a Vec<String>.
pub fn value_as_string_vec(value: &Value) -> Vec<String> {
    match value {
        Value::Array(arr) => arr
            .iter()
            .filter_map(|v| match v {
                Value::String(s) => Some(s.clone()),
                _ => None,
            })
            .collect(),
        _ => vec![],
    }
}

/// Format an XMLRPC Value in a style matching Python's pprint.PrettyPrinter(indent=4).
pub fn pprint(value: &Value) -> String {
    format_value(value, 0, 4)
}

fn format_value(value: &Value, current_indent: usize, indent_step: usize) -> String {
    match value {
        Value::Int(n) => n.to_string(),
        Value::Int64(n) => n.to_string(),
        Value::Bool(b) => {
            if *b {
                "True".to_string()
            } else {
                "False".to_string()
            }
        }
        Value::String(s) => format!("'{}'", s),
        Value::Double(f) => format!("{}", f),
        Value::DateTime(dt) => format!("'{}'", dt),
        Value::Base64(b) => format!("{:?}", b),
        Value::Nil => "None".to_string(),
        Value::Array(arr) => format_array(arr, current_indent, indent_step),
        Value::Struct(map) => format_struct(map, current_indent, indent_step),
    }
}

fn format_array(arr: &[Value], current_indent: usize, indent_step: usize) -> String {
    if arr.is_empty() {
        return "[]".to_string();
    }

    // Try single-line first
    let single_line = format!(
        "[{}]",
        arr.iter()
            .map(|v| format_value(v, 0, indent_step))
            .collect::<Vec<_>>()
            .join(", ")
    );

    if single_line.len() + current_indent <= 79 {
        return single_line;
    }

    // Multi-line
    let inner_indent = current_indent + indent_step;
    let pad = " ".repeat(inner_indent);
    let items: Vec<String> = arr
        .iter()
        .map(|v| format!("{}{}", pad, format_value(v, inner_indent, indent_step)))
        .collect();
    format!("[{}]", items.join(",\n"))
}

fn format_struct(map: &BTreeMap<String, Value>, current_indent: usize, indent_step: usize) -> String {
    if map.is_empty() {
        return "{}".to_string();
    }

    // Try single-line first
    let single_line_items: Vec<String> = map
        .iter()
        .map(|(k, v)| format!("'{}': {}", k, format_value(v, 0, indent_step)))
        .collect();
    let single_line = format!("{{{}}}", single_line_items.join(", "));

    if single_line.len() + current_indent <= 79 {
        return single_line;
    }

    // Multi-line
    let inner_indent = current_indent + indent_step;
    let pad = " ".repeat(inner_indent);
    let items: Vec<String> = map
        .iter()
        .map(|(k, v)| {
            format!(
                "{}'{}': {}",
                pad,
                k,
                format_value(v, inner_indent, indent_step)
            )
        })
        .collect();
    format!("{{{}}}", items.join(",\n"))
}

#[cfg(test)]
mod tests {
    use super::*;

    // --- pprint scalar tests ---

    #[test]
    fn pprint_int() {
        assert_eq!(pprint(&Value::Int(42)), "42");
    }

    #[test]
    fn pprint_int64() {
        assert_eq!(pprint(&Value::Int64(123456789)), "123456789");
    }

    #[test]
    fn pprint_negative_int() {
        assert_eq!(pprint(&Value::Int(-1)), "-1");
    }

    #[test]
    fn pprint_bool_true() {
        assert_eq!(pprint(&Value::Bool(true)), "True");
    }

    #[test]
    fn pprint_bool_false() {
        assert_eq!(pprint(&Value::Bool(false)), "False");
    }

    #[test]
    fn pprint_string() {
        assert_eq!(pprint(&Value::String("hello".into())), "'hello'");
    }

    #[test]
    fn pprint_empty_string() {
        assert_eq!(pprint(&Value::String("".into())), "''");
    }

    #[test]
    fn pprint_double() {
        assert_eq!(pprint(&Value::Double(3.14)), "3.14");
    }

    #[test]
    fn pprint_nil() {
        assert_eq!(pprint(&Value::Nil), "None");
    }

    // --- pprint array tests ---

    #[test]
    fn pprint_empty_array() {
        assert_eq!(pprint(&Value::Array(vec![])), "[]");
    }

    #[test]
    fn pprint_short_string_array() {
        let arr = Value::Array(vec![
            Value::String("abc".into()),
            Value::String("def".into()),
        ]);
        assert_eq!(pprint(&arr), "['abc', 'def']");
    }

    #[test]
    fn pprint_mixed_array() {
        let arr = Value::Array(vec![
            Value::Int(1),
            Value::String("two".into()),
            Value::Bool(true),
        ]);
        assert_eq!(pprint(&arr), "[1, 'two', True]");
    }

    #[test]
    fn pprint_long_array_wraps() {
        // Create an array long enough to exceed 79 chars on one line
        let arr = Value::Array(
            (0..10)
                .map(|i| Value::String(format!("call-id-{}-abcdefghij", i)))
                .collect(),
        );
        let output = pprint(&arr);
        // Should be multi-line
        assert!(output.contains('\n'), "long array should wrap to multiple lines");
        assert!(output.starts_with('['));
        assert!(output.ends_with(']'));
        // Each item should be indented with 4 spaces
        for line in output.lines().skip(1) {
            if line == "]" {
                continue;
            }
            assert!(
                line.starts_with("    "),
                "wrapped array items should be indented with 4 spaces: {:?}",
                line
            );
        }
    }

    // --- pprint struct tests ---

    #[test]
    fn pprint_empty_struct() {
        assert_eq!(pprint(&Value::Struct(BTreeMap::new())), "{}");
    }

    #[test]
    fn pprint_short_struct() {
        let mut map = BTreeMap::new();
        map.insert("key".into(), Value::String("val".into()));
        assert_eq!(pprint(&Value::Struct(map)), "{'key': 'val'}");
    }

    #[test]
    fn pprint_struct_sorted_keys() {
        let mut map = BTreeMap::new();
        map.insert("zebra".into(), Value::Int(1));
        map.insert("alpha".into(), Value::Int(2));
        let output = pprint(&Value::Struct(map));
        // BTreeMap is sorted, so alpha comes before zebra
        assert_eq!(output, "{'alpha': 2, 'zebra': 1}");
    }

    #[test]
    fn pprint_long_struct_wraps() {
        let mut map = BTreeMap::new();
        for i in 0..5 {
            map.insert(
                format!("long-property-name-{}", i),
                Value::String(format!("long-property-value-{}", i)),
            );
        }
        let output = pprint(&Value::Struct(map));
        assert!(output.contains('\n'), "long struct should wrap to multiple lines");
        assert!(output.starts_with('{'));
        assert!(output.ends_with('}'));
    }

    #[test]
    fn pprint_nested_struct_in_array() {
        let mut map = BTreeMap::new();
        map.insert("id".into(), Value::String("call-1".into()));
        map.insert("status".into(), Value::String("active".into()));
        let arr = Value::Array(vec![Value::Struct(map)]);
        let output = pprint(&arr);
        assert!(output.contains("'id'"));
        assert!(output.contains("'status'"));
    }

    // --- value_as_string_vec tests ---

    #[test]
    fn string_vec_from_array() {
        let arr = Value::Array(vec![
            Value::String("a".into()),
            Value::String("b".into()),
            Value::String("c".into()),
        ]);
        assert_eq!(value_as_string_vec(&arr), vec!["a", "b", "c"]);
    }

    #[test]
    fn string_vec_skips_non_strings() {
        let arr = Value::Array(vec![
            Value::String("a".into()),
            Value::Int(42),
            Value::String("b".into()),
        ]);
        assert_eq!(value_as_string_vec(&arr), vec!["a", "b"]);
    }

    #[test]
    fn string_vec_empty_array() {
        assert_eq!(value_as_string_vec(&Value::Array(vec![])), Vec::<String>::new());
    }

    #[test]
    fn string_vec_non_array_returns_empty() {
        assert_eq!(value_as_string_vec(&Value::Int(1)), Vec::<String>::new());
        assert_eq!(value_as_string_vec(&Value::String("x".into())), Vec::<String>::new());
    }

    // --- default_url test ---

    #[test]
    fn default_url_is_localhost_8090() {
        assert_eq!(default_url(), "http://localhost:8090");
    }

    // --- parse_url_arg tests ---

    fn s(v: &[&str]) -> Vec<String> {
        v.iter().map(|x| x.to_string()).collect()
    }

    #[test]
    fn parse_url_default() {
        let args = s(&["prog"]);
        let (url, rest) = parse_url_arg(&args);
        assert_eq!(url, "http://localhost:8090");
        assert!(rest.is_empty());
    }

    #[test]
    fn parse_url_custom() {
        let args = s(&["prog", "--url", "http://10.0.0.1:9090"]);
        let (url, rest) = parse_url_arg(&args);
        assert_eq!(url, "http://10.0.0.1:9090");
        assert!(rest.is_empty());
    }

    #[test]
    fn parse_url_with_other_args() {
        let args = s(&["prog", "--full", "--url", "http://host:1234"]);
        let (url, rest) = parse_url_arg(&args);
        assert_eq!(url, "http://host:1234");
        assert_eq!(rest, vec!["--full"]);
    }

    #[test]
    fn parse_url_after_positional() {
        let args = s(&["prog", "some-call-id", "--url", "http://host:5555"]);
        let (url, rest) = parse_url_arg(&args);
        assert_eq!(url, "http://host:5555");
        assert_eq!(rest, vec!["some-call-id"]);
    }

    #[test]
    fn parse_url_not_given() {
        let args = s(&["prog", "--full"]);
        let (url, rest) = parse_url_arg(&args);
        assert_eq!(url, "http://localhost:8090");
        assert_eq!(rest, vec!["--full"]);
    }
}
