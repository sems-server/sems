#![forbid(unsafe_code)]

use std::io;
use std::net::SocketAddrV4;
use std::process::ExitCode;
use std::time::Duration;

use sems_stats::{parse_args, query, resolve_server, usage, ArgError};

fn main() -> ExitCode {
    let argv: Vec<String> = std::env::args().collect();
    let progname = argv
        .first()
        .cloned()
        .unwrap_or_else(|| "sems-stats".to_string());

    let args = match parse_args(&argv) {
        Ok(a) => a,
        Err(e) => {
            eprintln!("{}: {}", progname, e);
            eprint!("{}", usage(&progname));
            return ExitCode::from(1);
        }
    };

    if args.help {
        print!("{}", usage(&progname));
        return ExitCode::from(1);
    }

    let ip = match resolve_server(&args.server) {
        Ok(ip) => ip,
        Err(ArgError::InvalidServer(s)) => {
            eprintln!("server '{}' is an invalid IP address", s);
            return ExitCode::from(1);
        }
        Err(e) => {
            eprintln!("{}", e);
            return ExitCode::from(1);
        }
    };
    let addr = SocketAddrV4::new(ip, args.port);

    if !args.quiet {
        println!("sending '{}\\n' to {}:{}", args.cmd, args.server, args.port);
    }

    match query(addr, &args.cmd, Duration::from_secs(args.timeout_secs)) {
        Ok(reply) => {
            if args.quiet {
                // Emit the reply with a single trailing newline so shell
                // captures (`$(sems-stats -q ...)`) produce one clean line.
                println!("{}", reply.trim_end_matches('\n'));
            } else {
                println!("received:");
                print!("{}", reply);
                if !reply.ends_with('\n') {
                    println!();
                }
            }
            ExitCode::from(0)
        }
        Err(e) => match e.kind() {
            io::ErrorKind::WouldBlock | io::ErrorKind::TimedOut => {
                eprintln!("read timeout!");
                ExitCode::from(1)
            }
            _ => {
                eprintln!("socket error: {}", e);
                ExitCode::from(2)
            }
        },
    }
}
