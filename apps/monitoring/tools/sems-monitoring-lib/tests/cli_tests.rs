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
