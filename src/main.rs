use std::env;
use std::net::Ipv4Addr;
use std::str::FromStr;
use std::net::AddrParseError;
use std::num::ParseIntError;

#[derive(Debug)]
enum NvmeError {
    Parse(String),
    NoArgument,
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
	    _ => { val = Err(NvmeError::Parse(s.to_string())); },
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
	    _ => { val = Err(NvmeError::Parse(s.to_string())); },
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
		    self.nqn = args_iter.next()
			.ok_or(NvmeError::NoArgument)?.to_string();
		},
		"--traddr" => {
		    self.traddr = args_iter.next()
			.ok_or(NvmeError::NoArgument)?.parse()?;
		},
		"--trsvcid" => {
		    self.trsvcid = args_iter.next()
			.ok_or(NvmeError::NoArgument)?.parse()?;
		},
		"--trtype" => {
		    self.trtype = args_iter.next()
			.ok_or(NvmeError::NoArgument)?.parse()?;
		},
		"--adrfam" => {
		    self.adrfam = args_iter.next()
			.ok_or(NvmeError::NoArgument)?.parse()?;
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

    drec.parse(args).expect("Argument error");
    println!("{} {} {}", drec.nqn, drec.traddr, drec.trsvcid);
}
