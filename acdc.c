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
		printf("Valid icresp received, cpda %d\n", icresp->cpda);
	}
	return len;
}

int kdreq(int sfd, int adrfam, const char *addr, const char *port)
{
	char buf[1024];
	struct nvme_tcp_kdreq_pdu *kdreq;
	struct nvme_tcp_kickstart_rec *krec;
	struct nvme_tcp_kdresp_pdu *kdresp;
	unsigned int krec_offset, kdreq_len = 12 + 290;
	ssize_t len;

	krec_offset = sizeof(*kdreq);
	memset(buf, 0, sizeof(buf));
	kdreq = (struct nvme_tcp_kdreq_pdu *)buf;
	kdreq->hdr.type = nvme_tcp_kdreq;
	kdreq->hdr.hlen = 12;
	kdreq->hdr.pdo = krec_offset;
	kdreq->hdr.plen = htole16(kdreq_len);
	kdreq->numkr = htole16(1);
	kdreq->numdie = htole16(1);
	krec = (struct nvme_tcp_kickstart_rec *)(buf + krec_offset);
	krec->trtype = NVMF_TRTYPE_TCP;
	krec->adrfam = adrfam;
	memcpy(krec->trsvcid, port, NVMF_TRSVCID_SIZE);
	memcpy(krec->traddr, addr, NVMF_TRADDR_SIZE);

	len = write(sfd, kdreq, kdreq_len);
	if (len < kdreq_len) {
		perror("send kdreq");
		return len;
	}
	len = read(sfd, buf, sizeof(buf));
	if (len < 0) {
		perror("read kdresp");
		return len;
	}
	kdresp = (struct nvme_tcp_kdresp_pdu *)buf;
	if (kdresp->hdr.type != nvme_tcp_kdresp) {
		printf("Invalid kdresp PDU type %d\n", kdresp->hdr.type);
		return -1;
	}
	if (kdresp->hdr.hlen != 10) {
		printf("Invalid kdresp PDU hdr len %d\n", kdresp->hdr.hlen);
		return -1;
	}
	if (le32toh(kdresp->hdr.plen) != 274) {
		printf("Invalid kdresp PDU len %d\n",
		       le32toh(kdresp->hdr.plen));
		return -1;
	}
	if (kdresp->ksstat != 0) {
		printf("Kickstart failed, reason %d\n", kdresp->failrsn);
		return 0;
	}
	printf("CDC NQN: %s\n", buf + 10);
	return 0;
}

int main(int argc, char **argv)
{
	const char *addr;
	const char *port;
	struct addrinfo hints, *result, *rp;
	int opt, err, sfd = -1;
	int adrfam;

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
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			if (rp->ai_family == AF_INET)
				adrfam = NVMF_ADDR_FAMILY_IP4;
			else
				adrfam = NVMF_ADDR_FAMILY_IP6;
			break;
		}
		close(sfd);
		sfd = -1;
	}
	if (sfd == -1) {
		printf("Failed to connect\n");
		return 1;
	}
	printf("connected\n");
	err = icreq(sfd);
	if (err > 0)
		err = kdreq(sfd, adrfam, addr, port);
	close(sfd);

	return 0;
}
