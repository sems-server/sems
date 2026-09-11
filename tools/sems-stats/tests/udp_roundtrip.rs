//! End-to-end test: spin up a local UDP server mimicking the SEMS stats
//! plug-in and verify that `sems_stats::query` round-trips correctly.

use std::net::{Ipv4Addr, SocketAddrV4, UdpSocket};
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

use sems_stats::{query, MSG_BUF_SIZE};

/// Start a one-shot UDP responder on 127.0.0.1. Returns the bound port and a
/// channel that delivers the request bytes once received. The responder will
/// reply with `reply` (plus a trailing NUL byte, like the real server).
fn spawn_responder(reply: &'static [u8]) -> (u16, mpsc::Receiver<Vec<u8>>) {
    let sock = UdpSocket::bind(SocketAddrV4::new(Ipv4Addr::LOCALHOST, 0))
        .expect("bind responder socket");
    let port = sock.local_addr().unwrap().port();
    let (tx, rx) = mpsc::channel();

    thread::spawn(move || {
        sock.set_read_timeout(Some(Duration::from_secs(5))).ok();
        let mut buf = vec![0u8; MSG_BUF_SIZE];
        if let Ok((n, peer)) = sock.recv_from(&mut buf) {
            let _ = tx.send(buf[..n].to_vec());
            let mut out = Vec::with_capacity(reply.len() + 1);
            out.extend_from_slice(reply);
            out.push(0);
            let _ = sock.send_to(&out, peer);
        }
    });

    (port, rx)
}

#[test]
fn query_sends_command_and_returns_reply() {
    let (port, rx) = spawn_responder(b"Active calls: 42\n");
    let addr = SocketAddrV4::new(Ipv4Addr::LOCALHOST, port);

    let reply = query(addr, "calls", Duration::from_secs(2)).expect("query ok");
    assert_eq!(reply, "Active calls: 42\n");

    let received = rx
        .recv_timeout(Duration::from_secs(2))
        .expect("responder saw request");
    assert_eq!(received, b"calls\n");
}

#[test]
fn query_preserves_command_with_spaces() {
    let (port, rx) = spawn_responder(b"loglevel set to 1.\n");
    let addr = SocketAddrV4::new(Ipv4Addr::LOCALHOST, port);

    let reply = query(addr, "set_loglevel 1", Duration::from_secs(2)).unwrap();
    assert_eq!(reply, "loglevel set to 1.\n");

    let received = rx.recv_timeout(Duration::from_secs(2)).unwrap();
    assert_eq!(received, b"set_loglevel 1\n");
}

#[test]
fn query_times_out_when_no_reply() {
    // Bind a socket but never read from it, so the client's recv must time out.
    let sock = UdpSocket::bind(SocketAddrV4::new(Ipv4Addr::LOCALHOST, 0)).unwrap();
    let port = sock.local_addr().unwrap().port();
    let addr = SocketAddrV4::new(Ipv4Addr::LOCALHOST, port);

    let err = query(addr, "calls", Duration::from_millis(200)).unwrap_err();
    assert!(
        matches!(
            err.kind(),
            std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut
        ),
        "expected timeout, got {:?}",
        err.kind()
    );

    drop(sock);
}
