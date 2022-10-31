use std::{fs, io};
use std::net::TcpStream;

struct NvmeTcpHdr {
  hdrtype: u8,
  flags: u8,
  hlen: u8,
  pdo: u8,
  plen: u32,
}

struct NvmeTcpICReqPDU {
  hdr: struct NvmeTcpHdr,
  pfv: u16,
  hpda: u8,
  digest: u8,
  maxr2t: u32,
}

struct NvmeTcpICRespPDU {
       hdr: struct NvmeTcpHdr,
	pfv: u16,
	cpda: u8,
	digest: u8,
	maxdata: u32,
}

struct DiscRecord {
   trsvcid: String,
   trtype: String,
   traddr: String,
   adrfam: String,
}

fn main() -> io::Result<()> {
   for entry in fs::read_dir("/sys/kernel/config/nvmet/ports")? {
       let dir = entry?;
       for port_entry in fs::read_dir(dir.path())? {
       	   let port = port_entry?;
	   if let Some(result) = port.path().to_str()
			.expect("Error converting filename").find("addr_") {
	   	   println!("Reading from {:?}", port.file_name());
	   	   let attr_value = fs::read_to_string(port.path())
	       	       .expect("Failed to read attr");
       	   	   println!("{:?}: {:?}", result, attr_value);
           }
       }
    }
    Ok(())
}
