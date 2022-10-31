use std::env;
use std::fmt;
use std::net::Ipv4Addr;
use std::str::FromStr;
use std::net::AddrParseError;
use std::num::ParseIntError;

#[derive(Debug)]
enum NvmeError {
    Parse(String),
    NoArgument(String),
    InvalidOption(String),
    InvalidAddr(AddrParseError),
    ParseInt(ParseIntError),
}

impl From<ParseIntError> for NvmeError {
    fn from(err: ParseIntError) -> Self {
        NvmeError::ParseInt(err)
    }
}

impl From<AddrParseError> for NvmeError {
    fn from(err: AddrParseError) -> Self {
	NvmeError::InvalidAddr(err)
    }
}

impl fmt::Display for NvmeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
       match *self {
           NvmeError::Parse(ref err) => write!(f, "Parse error: {}", err),
           NvmeError::NoArgument(ref err) => write!(f, "No argument for option '{}'", err),
           NvmeError::InvalidAddr(ref err) => write!(f, "Invalid IPv4 address '{}'", err),
           NvmeError::ParseInt(ref err) => write!(f, "Cannot parse integer, {}", err),
           NvmeError::InvalidOption(ref err) => write!(f, "Invalid option '{}'", err),
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
	match s {
	    "RDMA" => { Ok(NvmeTrType::RDMA) },
	    "FC" => { Ok(NvmeTrType::FC) },
	    "TCP" => { Ok(NvmeTrType::TCP) },
	    "Loop" => { Ok(NvmeTrType::LOOP) },
	    _ => { Err(NvmeError::Parse(s.to_string())) },
	}
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
	match s {
	    "IPv4" => { Ok(NvmeAdrFam::IP4) },
	    "IPv6" => { Ok(NvmeAdrFam::IP6) },
	    "IB" => { Ok(NvmeAdrFam::IB) },
	    "FC" => { Ok(NvmeAdrFam::FC) },
	    "Loop" => { Ok(NvmeAdrFam::LOOP) },
	    _ => { Err(NvmeError::Parse(s.to_string())) },
	}
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
		    self.nqn = args_iter.next()
			.ok_or(NvmeError::NoArgument(arg.to_string()))?.to_string();
		},
		"--traddr" => {
		    self.traddr = args_iter.next()
			.ok_or(NvmeError::NoArgument(arg.to_string()))?.parse()?;
		},
		"--trsvcid" => {
		    self.trsvcid = args_iter.next()
			.ok_or(NvmeError::NoArgument(arg.to_string()))?.parse()?;
		},
		"--trtype" => {
		    self.trtype = args_iter.next()
			.ok_or(NvmeError::NoArgument(arg.to_string()))?.parse()?;
		},
		"--adrfam" => {
		    self.adrfam = args_iter.next()
			.ok_or(NvmeError::NoArgument(arg.to_string()))?.parse()?;
		},
		_ => {
		    return Err(NvmeError::InvalidOption(arg.to_string()));
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

    match drec.parse(args) {
	Ok(a) => a,
	Err(e) => { return eprintln!("Error: {}", e.to_string()); }
    }
    println!("{} {} {}", drec.nqn, drec.traddr, drec.trsvcid);
}
