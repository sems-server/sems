use sems_monitoring_lib::{calls, di, parse_url_arg, pprint};
use std::env;
use std::process;

fn main() {
    let args: Vec<String> = env::args().collect();
    let (url, rest) = parse_url_arg(&args);

    if rest.len() == 1 && rest[0] == "--help" {
        eprintln!("usage: {} [--url <url>]", args[0]);
        process::exit(1);
    }

    let count = match calls(&url) {
        Ok(n) => n,
        Err(e) => {
            eprintln!("Error calling calls(): {}", e);
            process::exit(1);
        }
    };
    println!("Active calls: {}", count);

    let result = match di(&url, &["monitoring", "listFinished"]) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error calling di(monitoring, listFinished): {}", e);
            process::exit(1);
        }
    };

    println!("{}", pprint(&result));
}
