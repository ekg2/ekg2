SNIFF_HANDLER(sniff_dns, DNS_HEADER) {
	CHECK_LEN(DNS_HFIXEDSZ)

	if (hdr->srcport == 53) {
		debug("INCOMING DNS REPLY: [ID: %x]\n", pkt->id);
		/* 1) Check send id request with recv id request. */	/* pkt->id */

		/* 2a, 2b) RES_INSECURE1, RES_INSECURE2 implementation -> ignore */

		if (pkt->rcode == SERVFAIL || pkt->rcode == NOTIMP || pkt->rcode == REFUSED) {
			debug_error("[sniff_dns()] Server rejected query\n");
			goto end;
			return 0;
		}

		if (pkt->rcode == NOERROR && pkt->ancount == 0 && pkt->aa == 0 && pkt->ra == 0 && pkt->arcount == 0) {
			debug_error("[sniff_dns()] Referred query\n");
			return 0;
		}
		
		/* if set RES_IGNTC */
		if (pkt->tc) {
			debug_error("[sniff_dns()] XXX ; truncated answer\n");
			return -1;
		}
	
		if (pkt->rcode != NOERROR || pkt->ancount == 0) {

			switch (pkt->rcode) {
				case NXDOMAIN:	debug_error("[sniff_dns()] NXDOMAIN [%d]\n", ntohs(pkt->ancount));	break;
				case SERVFAIL:	debug_error("[sniff_dns()] SERVFAIL [%d]\n", ntohs(pkt->ancount));	break;
				case NOERROR:	debug_error("[sniff_dns()] NODATA\n");					break;

				case FORMERR:
				case NOTIMP:
				case REFUSED:
				default:
						debug_error("[sniff_dns()] NO_RECORVERY [%d, %d]\n", pkt->rcode, ntohs(pkt->ancount));	break;
			}
			goto end;
			return 0;
		}

		{
			int qdcount = ntohs(pkt->qdcount);			/* question count */
			int ancount = ntohs(pkt->ancount);			/* answer count */
			int i;
			int displayed = 0;

			char *eom = ((char *) pkt) + len;
			char *cur = ((char *) pkt) + sizeof(DNS_HEADER);
			char *beg = ((char *) pkt);

			debug_function("sniff_dns() NOERROR qdcount: %d, ancount: %d\n", qdcount, ancount);

			if (qdcount == 0 || !(cur < eom))
				print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_reply", inet_ntoa(hdr->dstip), "????");

			i = 0;
			while (qdcount > 0 && cur < eom) {
				char host[257];
				int len;

				if ((len = dn_expand(beg, eom, cur, host, sizeof(host))) < 0) {
					debug_error("sniff_dns() dn_expand() fail, on qdcount[%d] item\n", i);
					return 0;
				}

				cur += (len + DNS_QFIXEDSZ);	/* naglowek + payload */

				print_window_w(window_status,  EKG_WINACT_MSG, "sniff_dns_reply", inet_ntoa(hdr->dstip), host);

				qdcount--;
				i++;
			}

			i = 0;
			while (ancount > 0 && cur < eom) {
				char host[257];
				int len;

				if ((len = dn_expand(beg, eom, cur, host, sizeof(host))) < 0) {
					debug_error("sniff_dns() dn_expand() fail on ancount[%d] item\n", i);
					return 0;
				}
				
				cur += len;

				if (cur + 2 + 2 + 4 + 2 >= eom) {
					debug_error("sniff_dns() ancount[%d] no space for header?\n", i);
					return 0;
				}
				/* type [2b], class [2b], ttl [4b], size [2b] */

				int type = (cur[0] << 8 | cur[1]);
				int payload = (cur[8] << 8 | cur[9]);

				cur += 10;		/* skip header */

				if (cur + payload >= eom) {
					debug_error("sniff_dns() ancount[%d] no space for data?\n", i);
					return 0;
				}

				switch (type) {
					char tmp_addr[257];

					case T_A:
						if (payload != 4) {
							debug_error("T_A record but size != 4 [%d]\n", payload);
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_a", 
							host, inet_ntoa(*((struct in_addr *) &cur[0])));

						displayed = 1;
						break;

					case T_AAAA:
						if (payload != 16) {
							debug_error("T_AAAA record but size != 16 [%d]\n", payload);
							break;
						}
						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_aaaa",
							host, _inet_ntoa6(*((struct in6_addr *) &cur[0])));

						displayed = 1;
						break;

					case T_CNAME:
						if ((len = dn_expand(beg, eom, cur, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_CNAME failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_cname",
							host, tmp_addr);
						displayed = 1;
						break;

					case T_PTR:
						if ((len = dn_expand(beg, eom, cur, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_PTR failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_ptr",
							host, tmp_addr);
						displayed = 1;
						break;

					case T_MX: {
						int prio;
						if (payload < 2) {
							debug_error("T_MX record but size < 2 [%d]\n", payload);
							break;
						}
					/* LE STUFF */
						prio = cur[0] << 8 | cur[1];
						
						if ((len = dn_expand(beg, eom, cur + 2, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_MX failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_mx",
							host, tmp_addr, itoa(prio));
						displayed = 1;
						break;
					}

					case T_SRV: {
						int prio, weight, port;
						if (payload < 6) {
							debug_error("T_SRV record but size < 6 [%d]\n", payload);
							break;
						}
					/* LE stuff */
						prio	= cur[0] << 8 | cur[1];
						weight	= cur[2] << 8 | cur[3];
						port	= cur[4] << 8 | cur[5];

						if ((len = dn_expand(beg, eom, cur + 6, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_SRV failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_srv",
							host, tmp_addr, itoa(port), itoa(prio), itoa(weight));
						displayed = 1;
						break;
					}

					default:
						/* print payload */
						debug_error("ancount[%d] %d size: %d\n", i, type, payload);
						tcp_print_payload((u_char *) cur, payload);

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_?", 
							host, itoa(type), itoa(payload));
						displayed = 1;
				}
				cur += payload;		/* skip payload */

				ancount--;
				i++;
			}

			if (!displayed)
				print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_ndisplay");
		}

		return 0;


	} else if (hdr->dstport == 53) {
		debug("INCOMING DNS REQUEST: [ID: %x]\n", pkt->id);

	} else {
		debug_error("sniff_dns() SRCPORT/DSTPORT NOT 53!!!\n");
		return -2;
	}
	return 0;

end:
	tcp_print_payload((u_char *) pkt, len);
	return 0;
}

// vim:syn=c
