#include <stdio.h>
#include <stdlib.h>
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
	icreq.hdr.flags = NVME_TCP_F_KDCONN;
	icreq.pfv = htole16(NVME_TCP_PFV_1_0);
	len = write(sfd, &icreq, sizeof(icreq));
	if (len < sizeof(icreq)) {
		perror("send icreq");
		return len;
	}
	len = read(sfd, buf, sizeof(buf));
	if (len > 0) {
		struct nvme_tcp_icresp_pdu *icresp;

		icresp = (struct nvme_tcp_icresp_pdu *)buf;
		if (icresp->hdr.type != nvme_tcp_icresp) {
			fprintf(stderr, "Not an icresp PDU\n");
			return -1;
		}
		if (le32toh(icresp->hdr.plen) != sizeof(*icresp)) {
			fprintf(stderr, "Invalid icresp PDU len\n");
			return -1;
		}
		if (icresp->pfv != NVME_TCP_PFV_1_0) {
			fprintf(stderr, "Unhandled icresp PFV %d\n",
				icresp->pfv);
			return -1;
		}
	}
	return len;
}

int kdreq(int sfd, const char **reg, int numreg)
{
	char buf[1024], *ptr;
	struct nvme_tcp_kdreq_pdu *kdreq;
	struct nvme_tcp_kickstart_rec *krec;
	struct nvme_tcp_kdresp_pdu *kdresp;
	unsigned int krec_offset, kdreq_len = sizeof(*kdreq);
	int i;
	ssize_t len;

	krec_offset = kdreq_len;
	memset(buf, 0, sizeof(buf));
	kdreq = (struct nvme_tcp_kdreq_pdu *)buf;
	kdreq->hdr.type = nvme_tcp_kdreq;
	kdreq->hdr.hlen = krec_offset;
	kdreq->hdr.flags = (1 << 6);
	kdreq->hdr.pdo = krec_offset;
	kdreq_len += sizeof(*krec) * numreg;
	kdreq->hdr.plen = htole16(kdreq_len);
	kdreq->numkr = htole16(numreg);
	kdreq->numdie = htole16(1);
	for (i = 0; i < numreg; i++) {
		const char *reg_addr = reg[i];
		const char *reg_port = "8009";
		size_t reg_addr_size = strlen(reg_addr);

		ptr = strrchr(reg_addr, ':');
		if (ptr) {
			reg_addr_size = ptr - reg_addr;
			reg_port = ptr + 1;
		}
		krec = (struct nvme_tcp_kickstart_rec *)(buf + krec_offset);
		memset(krec, 0, sizeof(*krec));
		krec->trtype = NVMF_TRTYPE_TCP;
		krec->adrfam = reg_addr[0] == '[' ?
			NVMF_ADDR_FAMILY_IP6 : NVMF_ADDR_FAMILY_IP4;
		memcpy(krec->trsvcid, reg_port, strlen(reg_port));
		memcpy(krec->traddr, reg_addr, reg_addr_size);
		krec_offset += sizeof(*krec);
	}
	len = write(sfd, kdreq, kdreq_len);
	if (len < kdreq_len) {
		perror("send kdreq");
		return len;
	}
	memset(buf, 0, sizeof(buf));
	len = read(sfd, buf, sizeof(buf));
	if (len < 0) {
		perror("read kdresp");
		return len;
	}
	if (!len) {
		fprintf(stderr, "Connection closed by peer\n");
		return len;
	}
	kdresp = (struct nvme_tcp_kdresp_pdu *)buf;
	if (kdresp->hdr.type != nvme_tcp_kdresp) {
		fprintf(stderr, "Invalid kdresp PDU type %d\n",
			kdresp->hdr.type);
		return -1;
	}
	if (kdresp->hdr.hlen != 10) {
		fprintf(stderr, "Invalid kdresp PDU hdr len %d\n",
			kdresp->hdr.hlen);
		return -1;
	}
	if (le32toh(kdresp->hdr.plen) != 274) {
		fprintf(stderr, "Invalid kdresp PDU len %d\n",
		       le32toh(kdresp->hdr.plen));
		return -1;
	}
	if (kdresp->ksstat != 0) {
		fprintf(stderr, "Kickstart failed, reason %d\n",
			kdresp->failrsn);
		return 0;
	}
	printf("%s\n", buf + 10);
	return 0;
}

int open_socket(char *cdc_addr, char *cdc_port)
{
	struct addrinfo hints, *result, *rp;
	int err, sfd = -1;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	err = getaddrinfo(cdc_addr, cdc_port, &hints, &result);
	if (err) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
		return -1;
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
			     rp->ai_protocol);
		if (sfd == -1) {
			fprintf(stderr, "failed to create %s socket\n",
				rp->ai_family == AF_INET ? "IPv4" : "IPv6");
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;
		close(sfd);
		sfd = -1;
	}

	return sfd;
}

int main(int argc, char **argv)
{
	char *cdc_addr = NULL, *cdc_port = "8009", *ptr;
	const char **reg = NULL;
	int opt, err, sfd = -1;
	int numreg = 0;

	while ((opt = getopt(argc, argv, "c:r:h")) != -1) {
		switch (opt) {
		case 'c':
			cdc_addr = strdup(optarg);
			ptr = strrchr(cdc_addr, ':');
			if (ptr) {
				*ptr = '\0';
				cdc_port = ptr + 1;
			}
			break;
		case 'r':
			reg = realloc(reg, sizeof(const char *) * (numreg + 1));
			if (!reg) {
				perror("realloc");
				return 1;
			}
			reg[numreg] = strdup(optarg);
			if (!reg[numreg]) {
				perror("strdup");
				return 1;
			}
			numreg++;
			break;
		case 'h':
			printf("Usage: %s -c <address[:port]> -r <address[:port]>\n", argv[0]);
			return 0;
			break;
		default:
			fprintf(stderr, "%s: Invalid argument -%c\n",
				argv[0], opt);
			return 1;
		}
	}
	if (!cdc_addr) {
		fprintf(stderr, "%s: no CDC address specified\n", argv[0]);
		return 1;
	}
	sfd = open_socket(cdc_addr, cdc_port);
	if (sfd < 0) {
		fprintf(stderr, "Failed to connect to %s\n", cdc_addr);
		return 1;
	}
	err = icreq(sfd);
	if (err > 0)
		err = kdreq(sfd, reg, numreg);
	close(sfd);

	return 0;
}
