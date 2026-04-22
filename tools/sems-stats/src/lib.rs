//! Pure helpers for sems-stats: argument parsing, reply trimming, and the
//! UDP roundtrip. No global state, no unsafe.

#![forbid(unsafe_code)]

use std::fmt;
use std::io;
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4, UdpSocket};
use std::time::Duration;

pub const DEFAULT_SERVER: &str = "127.0.0.1";
pub const DEFAULT_PORT: u16 = 5040;
pub const DEFAULT_CMD: &str = "calls";
pub const DEFAULT_TIMEOUT_SECS: u64 = 5;
pub const MSG_BUF_SIZE: usize = 2048;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Args {
    pub server: String,
    pub port: u16,
    pub cmd: String,
    pub timeout_secs: u64,
    pub quiet: bool,
    pub help: bool,
}

impl Default for Args {
    fn default() -> Self {
        Args {
            server: DEFAULT_SERVER.to_string(),
            port: DEFAULT_PORT,
            cmd: DEFAULT_CMD.to_string(),
            timeout_secs: DEFAULT_TIMEOUT_SECS,
            quiet: false,
            help: false,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ArgError {
    MissingValue(String),
    Unknown(String),
    InvalidPort(String),
    InvalidTimeout(String),
    InvalidServer(String),
}

impl fmt::Display for ArgError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ArgError::MissingValue(flag) => {
                write!(f, "missing argument for parameter '{}'", flag)
            }
            ArgError::Unknown(arg) => write!(f, "unknown parameter '{}'", arg),
            ArgError::InvalidPort(s) => write!(f, "port '{}' is not a valid port number", s),
            ArgError::InvalidTimeout(s) => write!(f, "timeout '{}' not understood", s),
            ArgError::InvalidServer(s) => write!(f, "server '{}' is an invalid IP address", s),
        }
    }
}

impl std::error::Error for ArgError {}

/// Parse argv into `Args`. The first element is treated as the program name
/// and skipped, matching the usual Unix convention.
pub fn parse_args<I, S>(argv: I) -> Result<Args, ArgError>
where
    I: IntoIterator<Item = S>,
    S: AsRef<str>,
{
    let mut args = Args::default();
    let mut it = argv.into_iter();
    let _progname = it.next();

    while let Some(raw) = it.next() {
        let arg = raw.as_ref();
        match arg {
            "-h" | "--help" => args.help = true,
            "-q" | "--quiet" => args.quiet = true,
            "-s" => {
                let v = it
                    .next()
                    .ok_or_else(|| ArgError::MissingValue("-s".to_string()))?;
                args.server = v.as_ref().to_string();
            }
            "-p" => {
                let v = it
                    .next()
                    .ok_or_else(|| ArgError::MissingValue("-p".to_string()))?;
                let s = v.as_ref();
                let port = s
                    .parse::<u16>()
                    .map_err(|_| ArgError::InvalidPort(s.to_string()))?;
                if port == 0 {
                    return Err(ArgError::InvalidPort(s.to_string()));
                }
                args.port = port;
            }
            "-c" => {
                let v = it
                    .next()
                    .ok_or_else(|| ArgError::MissingValue("-c".to_string()))?;
                args.cmd = v.as_ref().to_string();
            }
            "-t" => {
                let v = it
                    .next()
                    .ok_or_else(|| ArgError::MissingValue("-t".to_string()))?;
                let s = v.as_ref();
                args.timeout_secs = s
                    .parse::<u64>()
                    .map_err(|_| ArgError::InvalidTimeout(s.to_string()))?;
            }
            other => return Err(ArgError::Unknown(other.to_string())),
        }
    }

    Ok(args)
}

/// Resolve a user-supplied server string as an IPv4 literal. The SEMS stats
/// server binds an IPv4 socket, so hostnames and IPv6 are intentionally not
/// accepted here (matching the C++ client).
pub fn resolve_server(server: &str) -> Result<Ipv4Addr, ArgError> {
    server
        .parse::<Ipv4Addr>()
        .map_err(|_| ArgError::InvalidServer(server.to_string()))
}

/// Render help text. Kept as a pure function so it can be asserted on in tests.
pub fn usage(progname: &str) -> String {
    format!(
        "SIP Express Media Server stats query\n\
         \n\
         Syntax: {p} [<options>]\n\
         \n\
         where <options>:\n\
         \u{0020}-s <server>  : server name|ip (default: {server})\n\
         \u{0020}-p <port>    : server port (default: {port})\n\
         \u{0020}-c <cmd>     : command (default: {cmd})\n\
         \u{0020}-t <seconds> : timeout (default: {timeout}s)\n\
         \u{0020}-q           : quiet: print only the server reply\n\
         \u{0020}-h           : show this help\n\
         \n\
         Tips:\n\
         \u{0020}o quote the command if it has arguments (e.g. {p} -c \"set_loglevel 1\")\n\
         \u{0020}o \"which\" prints available commands\n",
        p = progname,
        server = DEFAULT_SERVER,
        port = DEFAULT_PORT,
        cmd = DEFAULT_CMD,
        timeout = DEFAULT_TIMEOUT_SECS,
    )
}

/// Strip a single trailing NUL byte (the SEMS stats server terminates replies
/// with '\0'). Non-UTF-8 bytes are decoded lossily.
pub fn trim_reply(buf: &[u8]) -> String {
    let end = if buf.last() == Some(&0) {
        buf.len() - 1
    } else {
        buf.len()
    };
    String::from_utf8_lossy(&buf[..end]).into_owned()
}

/// Send `cmd` (plus a trailing newline) to the stats server at `addr` and
/// return the decoded reply. `timeout` applies to the receive step only.
pub fn query(addr: SocketAddrV4, cmd: &str, timeout: Duration) -> io::Result<String> {
    let sock = UdpSocket::bind("0.0.0.0:0")?;
    sock.set_read_timeout(Some(timeout))?;

    let mut msg = String::with_capacity(cmd.len() + 1);
    msg.push_str(cmd);
    msg.push('\n');
    sock.send_to(msg.as_bytes(), SocketAddr::V4(addr))?;

    let mut buf = [0u8; MSG_BUF_SIZE];
    let (n, _) = sock.recv_from(&mut buf)?;
    Ok(trim_reply(&buf[..n]))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(v: &[&str]) -> Result<Args, ArgError> {
        parse_args(v.iter().copied())
    }

    #[test]
    fn defaults_when_no_flags() {
        let a = parse(&["sems-stats"]).unwrap();
        assert_eq!(a, Args::default());
        assert_eq!(a.server, DEFAULT_SERVER);
        assert_eq!(a.port, DEFAULT_PORT);
        assert_eq!(a.cmd, DEFAULT_CMD);
        assert_eq!(a.timeout_secs, DEFAULT_TIMEOUT_SECS);
        assert!(!a.quiet);
        assert!(!a.help);
    }

    #[test]
    fn parses_all_flags() {
        let a = parse(&[
            "sems-stats", "-s", "10.0.0.1", "-p", "6000", "-c", "get_callsavg", "-t", "15", "-q",
        ])
        .unwrap();
        assert_eq!(a.server, "10.0.0.1");
        assert_eq!(a.port, 6000);
        assert_eq!(a.cmd, "get_callsavg");
        assert_eq!(a.timeout_secs, 15);
        assert!(a.quiet);
    }

    #[test]
    fn long_flags() {
        let a = parse(&["sems-stats", "--quiet", "--help"]).unwrap();
        assert!(a.quiet);
        assert!(a.help);
    }

    #[test]
    fn missing_value_errors() {
        assert_eq!(parse(&["p", "-s"]), Err(ArgError::MissingValue("-s".into())));
        assert_eq!(parse(&["p", "-p"]), Err(ArgError::MissingValue("-p".into())));
        assert_eq!(parse(&["p", "-c"]), Err(ArgError::MissingValue("-c".into())));
        assert_eq!(parse(&["p", "-t"]), Err(ArgError::MissingValue("-t".into())));
    }

    #[test]
    fn unknown_flag_errors() {
        let err = parse(&["p", "-Z"]).unwrap_err();
        assert_eq!(err, ArgError::Unknown("-Z".into()));
    }

    #[test]
    fn invalid_port_rejected() {
        assert!(matches!(
            parse(&["p", "-p", "abc"]),
            Err(ArgError::InvalidPort(_))
        ));
        assert!(matches!(
            parse(&["p", "-p", "0"]),
            Err(ArgError::InvalidPort(_))
        ));
        assert!(matches!(
            parse(&["p", "-p", "70000"]),
            Err(ArgError::InvalidPort(_))
        ));
    }

    #[test]
    fn invalid_timeout_rejected() {
        assert!(matches!(
            parse(&["p", "-t", "soon"]),
            Err(ArgError::InvalidTimeout(_))
        ));
    }

    #[test]
    fn resolve_server_ipv4() {
        assert_eq!(resolve_server("127.0.0.1").unwrap(), Ipv4Addr::LOCALHOST);
        assert_eq!(resolve_server("10.1.2.3").unwrap(), Ipv4Addr::new(10, 1, 2, 3));
    }

    #[test]
    fn resolve_server_rejects_hostname_and_ipv6() {
        assert!(resolve_server("localhost").is_err());
        assert!(resolve_server("::1").is_err());
    }

    #[test]
    fn trim_reply_drops_trailing_nul_only() {
        assert_eq!(trim_reply(b"Active calls: 0\n\0"), "Active calls: 0\n");
        assert_eq!(trim_reply(b"hello\0"), "hello");
        assert_eq!(trim_reply(b"no-nul"), "no-nul");
        assert_eq!(trim_reply(b""), "");
        assert_eq!(trim_reply(b"\0"), "");
    }

    #[test]
    fn trim_reply_lossy_on_invalid_utf8() {
        let bytes = [0xffu8, b'x', 0];
        let s = trim_reply(&bytes);
        assert!(s.ends_with('x'));
    }

    #[test]
    fn usage_mentions_all_switches() {
        let u = usage("sems-stats");
        for token in ["-s", "-p", "-c", "-t", "-q", "-h"] {
            assert!(u.contains(token), "usage missing `{}`", token);
        }
    }
}
