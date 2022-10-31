use std::env;
use std::net::Ipv4Addr;
use std::str::FromStr;
use std::{error::Error, fmt};
use std::net::AddrParseError;
use std::num::ParseIntError;

#[derive(Debug)]
struct NvmeParseError;

impl Error for NvmeParseError {}

impl fmt::Display for NvmeParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Failed to parse")
    }
}

#[derive(Debug)]
struct NvmeNoArgumentError;

impl Error for NvmeNoArgumentError {}

impl fmt::Display for NvmeNoArgumentError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Missing argument")
    }
}

#[derive(Debug)]
enum NvmeError {
    Parse(NvmeParseError),
    NoArgument(NvmeNoArgumentError),
    InvalidAddr(AddrParseError),
    ParseInt(ParseIntError),
}

impl fmt::Display for NvmeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
	match *self {
	    NvmeError::Parse(ref err) => write!(f, "Parse error: {}", err),
	    NvmeError::NoArgument(ref err) => write!(f, "No argument specified: {}", err),
	    NvmeError::InvalidAddr(ref err) => write!(f, "Invalid IPv4 address: {}", err),
	    NvmeError::ParseInt(ref err) => write!(f, "Integer parse error: {}", err),
	}
    }
}

enum NvmeTrType {
    RDMA = 1,
    FC = 2,
    TCP = 3,
    LOOP = 254,
}

impl FromStr for NvmeTrType {
    type Err = NvmeError;
    fn from_str(s: &str) -> Result<Self,Self::Err> {
	let val: Result<NvmeTrType,Self::Err>;
	match s {
	    "RDMA" => { val = Ok(NvmeTrType::RDMA); },
	    "FC" => { val = Ok(NvmeTrType::FC); },
	    "TCP" => { val = Ok(NvmeTrType::TCP); },
	    "Loop" => { val = Ok(NvmeTrType::LOOP); },
	    _ => { val = Err(NvmeError::Parse(NvmeParseError)); },
	}
	val
    }
}

enum NvmeAdrFam {
    IP4 = 1,
    IP6 = 2,
    IB = 3,
    FC = 4,
    LOOP = 254,
}

impl FromStr for NvmeAdrFam {
    type Err = NvmeError;
    fn from_str(s: &str) -> Result<Self,Self::Err> {
	let val: Result<NvmeAdrFam,Self::Err>;
	match s {
	    "IPv4" => { val = Ok(NvmeAdrFam::IP4); },
	    "IPv6" => { val = Ok(NvmeAdrFam::IP6); },
	    "IB" => { val = Ok(NvmeAdrFam::IB); },
	    "FC" => { val = Ok(NvmeAdrFam::FC); },
	    "Loop" => { val = Ok(NvmeAdrFam::LOOP); },
	    _ => { val = Err(NvmeError::Parse(NvmeParseError)); },
	}
	val
    }
}

struct IPV4DiscRecord {
    trsvcid: u32,
    trtype: NvmeTrType,
    traddr: Ipv4Addr,
    adrfam: NvmeAdrFam,
    nqn: String,
}

impl IPV4DiscRecord {
    fn parse(&mut self, args: Vec<String>) -> Result<(),NvmeError> {
	let mut args_iter = args.iter();
	args_iter.next();
	while let Some(arg) = args_iter.next() {
	    match arg.as_str() {
		"--nqn" => {
		    match args_iter.next() {
			Some(a) => { self.nqn = a.to_string(); },
			None => {
			    return Err(NvmeError::NoArgument(NvmeNoArgumentError));
			},
		    }
		},
		"--traddr" => {
		    match args_iter.next() {
			Some(a) => { 
			    self.traddr = a.parse().map_err(|e| NvmeError::InvalidAddr(e))?;
			},
			None => {
			    return Err(NvmeError::NoArgument(NvmeNoArgumentError));
			},
		    }
		},
		"--trsvcid" => {
		    match args_iter.next() {
			Some(a) => {
			    self.trsvcid = a.parse().map_err(|e| NvmeError::ParseInt(e))?;
			},
			None => {
			    return Err(NvmeError::NoArgument(NvmeNoArgumentError));
			},
		    }
		},
		"--trtype" => {
		    match args_iter.next() {
			Some(a) => { self.trtype = a.parse()?; },
			None => {
			    return Err(NvmeError::NoArgument(NvmeNoArgumentError));
			},
		    }
		},
		"--adrfam" => {
		    match args_iter.next() {
			Some(a) => { self.adrfam = a.parse()?; },
			None => {
			    return Err(NvmeError::NoArgument(NvmeNoArgumentError));
			},
		    }
		},
		_ => {
		    panic!("Invalid argument {}", arg);
		},
	    }
	}
	Ok(())
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut drec = IPV4DiscRecord {
        nqn: "nqn.2014-08.org.nvmexpress.discovery".to_string(),
        trtype: NvmeTrType::TCP,
	adrfam: NvmeAdrFam::IP4,
        traddr: "127.0.0.1".parse()
            .expect("Failed to parse default IPv4 Address"),
        trsvcid: 8009,
    };

    drec.parse(args).expect("Argument error");
    println!("{} {} {}", drec.nqn, drec.traddr, drec.trsvcid);
}
