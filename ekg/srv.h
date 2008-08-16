/*
 *  (C) Copyright 2004-2005 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __EKG_SRV_H
#define __EKG_SRV_H

typedef struct _gim_host gim_host;

char *ekg_inet_ntostr(int family,  void *buf);

int srv_resolver(gim_host **hostlist, const char *hostname, const int proto_port, const int port, const int proto);
LIST_ADD_COMPARE(gim_host_cmp, gim_host* );

int resolve_missing_entries(gim_host **hostlist);
int basic_resolver(gim_host **hostlist, const char *hostname, int port);

void write_out_and_destroy_list(int fd, gim_host *hostlist);

#define DNS_SRV_MAX_PRIO 0xffff
extern const int DNS_NS_MAXDNAME;

#endif /* __EKG_SRV_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
