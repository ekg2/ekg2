				case GG_SESSION_DCC:
				case GG_SESSION_DCC_GET:
				case GG_SESSION_DCC_SEND:
				{
					struct in_addr addr;
					unsigned short port = d->remote_port;
					char *tmp;
			
					addr.s_addr = d->remote_addr;

					if (d->peer_uin) {
						struct userlist *u = userlist_find(d->peer_uin, NULL);
						if (!addr.s_addr && u) {
							addr.s_addr = u->ip.s_addr;
							port = u->port;
						}
						tmp = saprintf("%s (%s:%d)", xstrdup(format_user(d->peer_uin)), inet_ntoa(addr), port);
					} else 
						tmp = saprintf("%s:%d", inet_ntoa(addr), port);
					print("dcc_timeout", tmp);
					xfree(tmp);
					remove_transfer(d);
					list_remove(&watches, d, 0);
					gg_free_dcc(d);
					break;
				}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
