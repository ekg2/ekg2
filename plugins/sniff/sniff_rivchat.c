static char *build_rivchatport_name(const connection_t *hdr) {
	static char buf[50];

	sprintf(buf, "sniff:rivchat:%d", hdr->srcport);
	return buf;
}

SNIFF_HANDLER(sniff_rivchat_info, rivchat_packet_rcinfo) {
	char *hostname	= gg_cp_to_iso(xstrndup(pkt->host, sizeof(pkt->host)));
	char *os	= gg_cp_to_iso(xstrndup(pkt->os, sizeof(pkt->os)));
	char *program	= gg_cp_to_iso(xstrndup(pkt->prog, sizeof(pkt->prog)));
	char *username	= gg_cp_to_iso(xstrndup(pkt->user, sizeof(pkt->user)));

	char program_ver[8];	/* ddd '.' ddd */

	sprintf(program_ver, "%d.%d", pkt->version[0], pkt->version[1]);

	print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
			"sniff_rivchat_rcinfo", inet_ntoa(hdr->srcip),

			username, hostname, os, program, program_ver);

	/* not used: away, master, slowa, kod, plec, online, filetransfer, pisze */

	xfree(hostname);
	xfree(os);
	xfree(program);
	xfree(username);
	return 0;
}

SNIFF_HANDLER(sniff_rivchat, rivchat_packet) {
	int type = pkt->type;		/* pkt->type is LE, if you're using BE you know what to do */

	char *nick;

	CHECK_LEN(sizeof(rivchat_packet))

	nick = gg_cp_to_iso(xstrndup(pkt->nick, sizeof(pkt->nick)));

	debug("UDP RIVCHAT PACKET [SIZE: %d FROMID: %d TOID: %d TYPE: %x NICK: %s\n", pkt->size, pkt->fromid, pkt->toid, pkt->type, nick);

	tcp_print_payload((u_char *) pkt->format, sizeof(pkt->format));

	switch (type) {
		char *data;

		case RIVCHAT_ME:
			debug_function("sniff_rivchat() RIVCHAT_ME\n");

			data = gg_cp_to_iso(xstrndup(pkt->data, sizeof(pkt->data)));
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_me", inet_ntoa(hdr->srcip),
				nick,
				data);

			xfree(data);
			break;

		case RIVCHAT_MESSAGE:
			debug_error("sniff_rivchat() RIVCHAT_MESSAGE\n");
			tcp_print_payload((u_char *) pkt->data, sizeof(pkt->data));

			/* XXX secure, not secure */

			data = gg_cp_to_iso(xstrndup(pkt->data, sizeof(pkt->data)));
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_message", inet_ntoa(hdr->srcip),
				nick,
				data);

			xfree(data);
			break;

		case RIVCHAT_INIT:
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_init", inet_ntoa(hdr->srcip));
			/* no break */
		case RIVCHAT_PING:
			debug_function("sniff_rivchat() RIVCHAT_PING\n");
			sniff_rivchat_info(s, hdr, (rivchat_packet_rcinfo *) pkt->data, sizeof(pkt->data));
			break;

		case RIVCHAT_AWAY:
		case RIVCHAT_PINGAWAY:
			debug_function("sniff_rivchat() RIVCHAT_AWAY/RIVCHAT_PINGAWAY\n");

			data = gg_cp_to_iso(xstrndup(pkt->data, sizeof(pkt->data)));
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				type == RIVCHAT_AWAY ? "sniff_rivchat_away" : "sniff_rivchat_pingaway", inet_ntoa(hdr->srcip),

				data);
			xfree(data);
			break;

		case RIVCHAT_QUIT:
			debug_function("sniff_rivchat() RIVCHAT_QUIT\n");
			
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_quit", inet_ntoa(hdr->srcip));
				/* data discarded, however in some time maybe there'd be reason */

			break;

		default:
			debug_error("sniff_rivchat() unknown type: %x\n", pkt->type);
			tcp_print_payload((u_char *) pkt->data, sizeof(pkt->data));
	}
	xfree(nick);
	return 0;
}

