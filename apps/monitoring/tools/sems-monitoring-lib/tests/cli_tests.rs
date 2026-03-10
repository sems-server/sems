use std::io::{BufRead, BufReader, Write};
use std::net::TcpStream;
use std::process::Command;

fn cargo_bin(name: &str) -> Command {
    // Find the binary in the workspace target directory
    let mut path = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.pop(); // up from sems-monitoring-lib to workspace root
    path.push("target");
    path.push("debug");
    path.push(name);
    Command::new(path)
}

// --- sems-list-calls ---

#[test]
fn list_calls_help_exits_with_1() {
    let output = cargo_bin("sems-list-calls")
        .arg("--help")
        .output()
        .expect("failed to run sems-list-calls");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("usage:") && stderr.contains("[--full]"),
        "help should show usage with --full option, got: {}",
        stderr
    );
}

#[test]
fn list_calls_no_server_fails_gracefully() {
    let output = cargo_bin("sems-list-calls")
        .output()
        .expect("failed to run sems-list-calls");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("Error"),
        "should print error when no server is running, got: {}",
        stderr
    );
}

// --- sems-list-active-calls ---

#[test]
fn list_active_calls_help_exits_with_1() {
    let output = cargo_bin("sems-list-active-calls")
        .arg("--help")
        .output()
        .expect("failed to run sems-list-active-calls");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("usage:") && stderr.contains("[--full]"),
        "help should show usage with --full option, got: {}",
        stderr
    );
}

#[test]
fn list_active_calls_no_server_fails_gracefully() {
    let output = cargo_bin("sems-list-active-calls")
        .output()
        .expect("failed to run sems-list-active-calls");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("Error"),
        "should print error when no server is running, got: {}",
        stderr
    );
}

// --- sems-list-finished-calls ---

#[test]
fn list_finished_calls_help_exits_with_1() {
    let output = cargo_bin("sems-list-finished-calls")
        .arg("--help")
        .output()
        .expect("failed to run sems-list-finished-calls");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("usage:"),
        "help should show usage, got: {}",
        stderr
    );
}

#[test]
fn list_finished_calls_no_server_fails_gracefully() {
    let output = cargo_bin("sems-list-finished-calls")
        .output()
        .expect("failed to run sems-list-finished-calls");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("Error"),
        "should print error when no server is running, got: {}",
        stderr
    );
}

// --- sems-get-callproperties ---

#[test]
fn get_callproperties_no_args_shows_usage() {
    let output = cargo_bin("sems-get-callproperties")
        .output()
        .expect("failed to run sems-get-callproperties");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("usage:") && stderr.contains("<ltag/ID of call to list>"),
        "no-args should show usage with callid argument, got: {}",
        stderr
    );
}

#[test]
fn get_callproperties_help_shows_usage() {
    let output = cargo_bin("sems-get-callproperties")
        .arg("--help")
        .output()
        .expect("failed to run sems-get-callproperties");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("usage:"),
        "help should show usage, got: {}",
        stderr
    );
}

#[test]
fn get_callproperties_no_server_fails_gracefully() {
    let output = cargo_bin("sems-get-callproperties")
        .arg("some-call-id")
        .output()
        .expect("failed to run sems-get-callproperties");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("Error"),
        "should print error when no server is running, got: {}",
        stderr
    );
}

// --- sems-prometheus-exporter ---

#[test]
fn prometheus_exporter_help_exits_with_1() {
    let output = cargo_bin("sems-prometheus-exporter")
        .arg("--help")
        .output()
        .expect("failed to run sems-prometheus-exporter");
    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("usage:") && stderr.contains("--listen"),
        "help should show usage with --listen option, got: {}",
        stderr
    );
}

#[test]
fn prometheus_exporter_serves_root() {
    // Start exporter on a random available port
    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    drop(listener);

    let addr = format!("127.0.0.1:{}", port);
    let mut child = cargo_bin("sems-prometheus-exporter")
        .args(&["--listen", &addr, "--url", "http://127.0.0.1:19999"])
        .spawn()
        .expect("failed to start exporter");

    // Wait for the server to be ready
    let mut ready = false;
    for _ in 0..50 {
        std::thread::sleep(std::time::Duration::from_millis(50));
        if TcpStream::connect(&addr).is_ok() {
            ready = true;
            break;
        }
    }
    assert!(ready, "exporter did not start listening in time");

    // GET /
    let response = http_get(&addr, "/");
    assert!(response.contains("SEMS Exporter"), "root should contain title, got: {}", response);

    child.kill().unwrap();
    let _ = child.wait();
}

#[test]
fn prometheus_exporter_serves_metrics() {
    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    drop(listener);

    let addr = format!("127.0.0.1:{}", port);
    let mut child = cargo_bin("sems-prometheus-exporter")
        .args(&["--listen", &addr, "--url", "http://127.0.0.1:19999"])
        .spawn()
        .expect("failed to start exporter");

    let mut ready = false;
    for _ in 0..50 {
        std::thread::sleep(std::time::Duration::from_millis(50));
        if TcpStream::connect(&addr).is_ok() {
            ready = true;
            break;
        }
    }
    assert!(ready, "exporter did not start listening in time");

    // GET /metrics — SEMS not running so we get error comments
    let response = http_get(&addr, "/metrics");
    assert!(
        response.contains("# error") || response.contains("sems_"),
        "metrics endpoint should return prometheus-formatted output, got: {}",
        response
    );

    child.kill().unwrap();
    let _ = child.wait();
}

#[test]
fn prometheus_exporter_returns_404() {
    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let port = listener.local_addr().unwrap().port();
    drop(listener);

    let addr = format!("127.0.0.1:{}", port);
    let mut child = cargo_bin("sems-prometheus-exporter")
        .args(&["--listen", &addr, "--url", "http://127.0.0.1:19999"])
        .spawn()
        .expect("failed to start exporter");

    let mut ready = false;
    for _ in 0..50 {
        std::thread::sleep(std::time::Duration::from_millis(50));
        if TcpStream::connect(&addr).is_ok() {
            ready = true;
            break;
        }
    }
    assert!(ready, "exporter did not start listening in time");

    let response = http_get_raw(&addr, "/nonexistent");
    assert!(
        response.contains("404 Not Found"),
        "unknown path should return 404, got: {}",
        response
    );

    child.kill().unwrap();
    let _ = child.wait();
}

/// Minimal HTTP GET, returns the response body.
fn http_get(addr: &str, path: &str) -> String {
    let raw = http_get_raw(addr, path);
    // Split off headers
    if let Some(pos) = raw.find("\r\n\r\n") {
        raw[pos + 4..].to_string()
    } else {
        raw
    }
}

/// Minimal HTTP GET, returns the full raw response including headers.
fn http_get_raw(addr: &str, path: &str) -> String {
    let mut stream = TcpStream::connect(addr).expect("failed to connect");
    write!(stream, "GET {} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", path, addr).unwrap();
    stream.flush().unwrap();
    let mut response = String::new();
    let mut reader = BufReader::new(&stream);
    loop {
        let mut line = String::new();
        match reader.read_line(&mut line) {
            Ok(0) | Err(_) => break,
            Ok(_) => response.push_str(&line),
        }
    }
    response
}
