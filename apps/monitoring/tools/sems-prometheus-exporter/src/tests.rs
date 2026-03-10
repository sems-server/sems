use super::*;

// --- sanitize_label tests ---

#[test]
fn sanitize_plain_string() {
    assert_eq!(sanitize_label("hello"), "hello");
}

#[test]
fn sanitize_backslash() {
    assert_eq!(sanitize_label("a\\b"), "a\\\\b");
}

#[test]
fn sanitize_quotes() {
    assert_eq!(sanitize_label("a\"b"), "a\\\"b");
}

#[test]
fn sanitize_newline() {
    assert_eq!(sanitize_label("a\nb"), "a\\nb");
}

#[test]
fn sanitize_combined() {
    assert_eq!(sanitize_label("a\\\"b\nc"), "a\\\\\\\"b\\nc");
}

// --- metrics module tests ---

#[test]
fn metric_render_gauge() {
    let mut out = String::new();
    Metric::new("test_gauge", "Test help", MetricType::Gauge).render(42, &mut out);
    assert!(out.contains("# HELP test_gauge Test help\n"));
    assert!(out.contains("# TYPE test_gauge gauge\n"));
    assert!(out.contains("test_gauge 42\n"));
}

#[test]
fn metric_render_counter() {
    let mut out = String::new();
    Metric::new("test_counter", "Count help", MetricType::Counter).render(99, &mut out);
    assert!(out.contains("# TYPE test_counter counter\n"));
    assert!(out.contains("test_counter 99\n"));
}

#[test]
fn metric_render_labeled() {
    let mut out = String::new();
    let m = Metric::new("labeled", "help", MetricType::Gauge);
    m.render_header(&mut out);
    m.render_labeled("{app=\"test\"}", 5, &mut out);
    assert!(out.contains("labeled{app=\"test\"} 5\n"));
}

// --- route tests ---

#[test]
fn route_root_returns_html() {
    let (status, ct, body) = route("/", "http://localhost:8090");
    assert_eq!(status, "200 OK");
    assert!(ct.contains("html"));
    assert!(body.contains("SEMS Exporter"));
    assert!(body.contains("/metrics"));
}

#[test]
fn route_404() {
    let (status, _, body) = route("/unknown", "http://localhost:8090");
    assert_eq!(status, "404 Not Found");
    assert!(body.contains("Not Found"));
}

#[test]
fn route_metrics_with_unreachable_sems() {
    // When SEMS is not running, /metrics should still return 200 with error comments
    let (status, ct, body) = route("/metrics", "http://127.0.0.1:19999");
    assert_eq!(status, "200 OK");
    assert!(ct.contains("text/plain"));
    // Should contain error comments for failed queries
    assert!(body.contains("# error") || body.is_empty() || body.contains("sems_"));
}

// --- parse_listen_addr tests ---

fn s(v: &[&str]) -> Vec<String> {
    v.iter().map(|x| x.to_string()).collect()
}

#[test]
fn parse_listen_default() {
    let args = s(&[]);
    assert_eq!(parse_listen_addr(&args), DEFAULT_LISTEN);
}

#[test]
fn parse_listen_custom() {
    let args = s(&["--listen", "127.0.0.1:8080"]);
    assert_eq!(parse_listen_addr(&args), "127.0.0.1:8080");
}

#[test]
fn parse_listen_with_other_args() {
    let args = s(&["--full", "--listen", "0.0.0.0:3000"]);
    assert_eq!(parse_listen_addr(&args), "0.0.0.0:3000");
}
