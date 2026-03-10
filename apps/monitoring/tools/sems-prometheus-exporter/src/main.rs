use sems_monitoring_lib::{call_i64, di, get_cpslimit, value_as_i64, Value};
use std::env;
use std::fmt::Write as FmtWrite;
use std::io::{BufRead, BufReader, Write};
use std::net::TcpListener;
use std::process;
use std::time::Instant;

mod metrics;
use metrics::{Metric, MetricType};

const DEFAULT_LISTEN: &str = "0.0.0.0:9090";

fn main() {
    let args: Vec<String> = env::args().collect();
    let (sems_url, rest) = sems_monitoring_lib::parse_url_arg(&args);
    let listen_addr = parse_listen_addr(&rest);

    if rest.iter().any(|a| a == "--help" || a == "-h") {
        eprintln!(
            "usage: {} [--url <sems-xmlrpc-url>] [--listen <addr:port>]",
            args[0]
        );
        eprintln!();
        eprintln!(
            "  --url     SEMS XMLRPC endpoint (default: {})",
            sems_monitoring_lib::default_url()
        );
        eprintln!(
            "  --listen  Prometheus listen address (default: {})",
            DEFAULT_LISTEN
        );
        process::exit(1);
    }

    eprintln!("sems-prometheus-exporter: listening on {}", listen_addr);
    eprintln!("sems-prometheus-exporter: scraping SEMS at {}", sems_url);

    let listener = match TcpListener::bind(&listen_addr) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("error: failed to bind {}: {}", listen_addr, e);
            process::exit(1);
        }
    };

    for stream in listener.incoming() {
        let mut stream = match stream {
            Ok(s) => s,
            Err(e) => {
                eprintln!("warn: accept failed: {}", e);
                continue;
            }
        };

        let mut reader = BufReader::new(&stream);
        let mut request_line = String::new();
        if reader.read_line(&mut request_line).is_err() {
            continue;
        }

        let path = request_line
            .split_whitespace()
            .nth(1)
            .unwrap_or("/")
            .to_string();

        // Drain remaining headers
        let mut header = String::new();
        loop {
            header.clear();
            if reader.read_line(&mut header).is_err() || header.trim().is_empty() {
                break;
            }
        }

        let (status, content_type, body) = route(&path, &sems_url);

        let resp = format!(
            "HTTP/1.1 {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
            status,
            content_type,
            body.len(),
            body
        );
        let _ = stream.write_all(resp.as_bytes());
    }
}

fn route(path: &str, sems_url: &str) -> (&'static str, &'static str, String) {
    match path {
        "/metrics" => {
            let start = Instant::now();
            let body = collect_metrics(sems_url);
            let elapsed = start.elapsed();
            eprintln!(
                "info: scraped {} bytes in {:.1}ms",
                body.len(),
                elapsed.as_secs_f64() * 1000.0
            );
            ("200 OK", "text/plain; version=0.0.4; charset=utf-8", body)
        }
        "/" | "" => (
            "200 OK",
            "text/html; charset=utf-8",
            "<html><body><h1>SEMS Exporter</h1>\
             <p><a href=\"/metrics\">Metrics</a></p>\
             </body></html>"
                .to_string(),
        ),
        _ => ("404 Not Found", "text/plain", "Not Found\n".to_string()),
    }
}

fn parse_listen_addr(args: &[String]) -> String {
    let mut i = 0;
    while i < args.len() {
        if args[i] == "--listen" {
            if i + 1 < args.len() {
                return args[i + 1].clone();
            }
            eprintln!("error: --listen requires a value");
            process::exit(1);
        }
        i += 1;
    }
    DEFAULT_LISTEN.to_string()
}

/// Collect all available metrics from SEMS and format as Prometheus exposition text.
pub fn collect_metrics(url: &str) -> String {
    let mut out = String::with_capacity(4096);
    collect_core_metrics(url, &mut out);
    collect_monitoring_counts(url, &mut out);
    collect_monitoring_sessions(url, &mut out);
    out
}

/// Core session and CPS metrics from built-in XMLRPC methods.
fn collect_core_metrics(url: &str, out: &mut String) {
    let core_metrics: &[(&str, &str, MetricType, &str)] = &[
        ("calls", "sems_active_calls", MetricType::Gauge, "Current number of active calls"),
        ("get_sessioncount", "sems_sessions_total", MetricType::Counter, "Total sessions since startup"),
        ("get_callsavg", "sems_calls_avg", MetricType::Gauge, "Average active calls (5s window)"),
        ("get_callsmax", "sems_calls_max", MetricType::Gauge, "Peak active calls since last query"),
        ("get_cpsavg", "sems_cps_avg", MetricType::Gauge, "Average calls per second (5s window)"),
        ("get_cpsmax", "sems_cps_max", MetricType::Gauge, "Peak calls per second since last query"),
        ("get_shutdownmode", "sems_shutdown_mode", MetricType::Gauge, "Whether shutdown mode is active"),
    ];

    for (method, name, mtype, help) in core_metrics {
        match call_i64(url, method) {
            Ok(val) => Metric::new(name, help, *mtype).render(val, out),
            Err(e) => {
                eprintln!("warn: {} failed: {}", method, e);
                writeln!(out, "# error querying {}: {}", name, e).unwrap();
            }
        }
    }

    match get_cpslimit(url) {
        Ok((hard, soft)) => {
            Metric::new(
                "sems_cps_hard_limit",
                "Configured hard CPS limit",
                MetricType::Gauge,
            )
            .render(hard, out);
            Metric::new(
                "sems_cps_soft_limit",
                "Configured soft CPS limit",
                MetricType::Gauge,
            )
            .render(soft, out);
        }
        Err(e) => eprintln!("warn: get_cpslimit failed: {}", e),
    }
}

/// Collect monitoring plugin counter/sample metrics via DI.
fn collect_monitoring_counts(url: &str, out: &mut String) {
    let counts = match di(url, &["monitoring", "getAllCounts"]) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("warn: monitoring.getAllCounts failed: {}", e);
            return;
        }
    };

    if let Value::Struct(map) = &counts {
        if !map.is_empty() {
            let m = Metric::new(
                "sems_monitoring_count",
                "Monitoring counter value",
                MetricType::Gauge,
            );
            m.render_header(out);
            for (name, value) in map {
                if let Ok(v) = value_as_i64(value) {
                    let label = format!("{{name=\"{}\"}}", sanitize_label(name));
                    m.render_labeled(&label, v, out);
                }
            }
        }
    }
}

/// Collect per-session monitoring data: count of active/finished sessions.
fn collect_monitoring_sessions(url: &str, out: &mut String) {
    if let Ok(v) = di(url, &["monitoring", "listActive"]) {
        let count = match &v {
            Value::Array(arr) => arr.len() as i64,
            _ => 0,
        };
        Metric::new(
            "sems_monitoring_active_sessions",
            "Number of active monitored sessions",
            MetricType::Gauge,
        )
        .render(count, out);
    }

    if let Ok(v) = di(url, &["monitoring", "listFinished"]) {
        let count = match &v {
            Value::Array(arr) => arr.len() as i64,
            _ => 0,
        };
        Metric::new(
            "sems_monitoring_finished_sessions",
            "Number of finished monitored sessions (awaiting GC)",
            MetricType::Gauge,
        )
        .render(count, out);
    }

    collect_registration_metrics(url, out);
}

/// Attempt to collect registration-related metrics.
/// The SBC registrar may store data via monitoring — look for known attributes.
fn collect_registration_metrics(url: &str, out: &mut String) {
    let reg_attrs = ["reg_active", "registrations", "registered_uas"];
    for attr in &reg_attrs {
        if let Ok(Value::Array(ref arr)) = di(url, &["monitoring", "getAttribute", attr]) {
            if !arr.is_empty() {
                writeln!(out, "# HELP sems_{} Registration attribute from monitoring", attr).unwrap();
                writeln!(out, "# TYPE sems_{} gauge", attr).unwrap();
                writeln!(out, "sems_{} {}", attr, arr.len()).unwrap();
            }
        }
    }
}

/// Escape characters not allowed in Prometheus label values.
pub fn sanitize_label(s: &str) -> String {
    s.replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('\n', "\\n")
}

#[cfg(test)]
mod tests;
