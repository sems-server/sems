use sems_monitoring_lib::{calls, di, parse_url_arg, pprint, value_as_string_vec};
use std::env;
use std::process;

fn main() {
    let args: Vec<String> = env::args().collect();
    let (url, rest) = parse_url_arg(&args);

    if rest.len() == 1 && rest[0] == "--help" {
        eprintln!("usage: {} [--full] [--url <url>]", args[0]);
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

    let ids_value = match di(&url, &["monitoring", "list"]) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("Error calling di(monitoring, list): {}", e);
            process::exit(1);
        }
    };

    println!("{}", pprint(&ids_value));

    if rest.len() == 1 && rest[0] == "--full" {
        let ids = value_as_string_vec(&ids_value);
        for callid in &ids {
            let attrs = match di(&url, &["monitoring", "get", callid]) {
                Ok(v) => v,
                Err(e) => {
                    eprintln!("Error calling di(monitoring, get, {}): {}", callid, e);
                    continue;
                }
            };
            println!("----- {} -----", callid);
            println!("{}", pprint(&attrs));
        }
    }
}
