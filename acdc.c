#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <linux/types.h>

#include "nvme-tcp.h"

int icreq(int sfd)
{
	struct nvme_tcp_icreq_pdu icreq;
	ssize_t len;
	char buf[1024];

	memset(&icreq, 0, sizeof(icreq));
	icreq.hdr.type = nvme_tcp_icreq;
	icreq.hdr.hlen = sizeof(icreq);
	icreq.hdr.plen = htole32(sizeof(icreq));
	icreq.pfv = htole16(NVME_TCP_PFV_1_0);
	len = write(sfd, &icreq, sizeof(icreq));
	if (len < sizeof(icreq)) {
		perror("send icreq");
		return len;
	}
	len = read(sfd, buf, sizeof(buf));
	if (len > 0) {
		struct nvme_tcp_icresp_pdu *icresp;

		printf("Read %ld icresp bytes\n", len);
		icresp = (struct nvme_tcp_icresp_pdu *)buf;
		if (icresp->hdr.type != nvme_tcp_icresp) {
			printf("Not an icresp PDU\n");
			return -1;
		}
		if (le32toh(icresp->hdr.plen) != sizeof(*icresp)) {
			printf("Invalid icresp PDU len\n");
			return -1;
		}
		if (icresp->pfv != NVME_TCP_PFV_1_0) {
			printf("Unhandled icresp PFV %d\n", icresp->pfv);
			return -1;
		}
		printf("Valid icresp received\n");
	}
	return len;
}

int main(int argc, char **argv)
{
	const char *addr;
	const char *port;
	struct addrinfo hints, *result, *rp;
	int opt, err, sfd = -1;

	while ((opt = getopt(argc, argv, "a:p:h")) != -1) {
		switch (opt) {
		case 'a':
			addr = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		default:
			return 1;
		}
	}
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	err = getaddrinfo(addr, port, &hints, &result);
	if (err) {
		printf("getaddrinfo: %s\n", gai_strerror(err));
		return 1;
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
			     rp->ai_protocol);
		if (sfd == -1) {
			printf("failed to create socket %d/%d/%d\n",
			       rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;
		close(sfd);
		sfd = -1;
	}
	if (sfd == -1) {
		printf("Failed to connect\n");
		return 1;
	}
	printf("connected\n");
	icreq(sfd);
	close(sfd);

	return 0;
}
