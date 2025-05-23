/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 *
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fraunhofer FOKUS Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 *
 * NB: A lot of this code was originally part of OpenIMSCore,
 * FhG Fokus.
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "dlist.h"
#include <stdlib.h> /* abort */
#include <string.h> /* strlen, memcmp */
#include <stdio.h>	/* printf */
#include "../../core/ut.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/dprint.h"
#include "../../core/ip_addr.h"
#include "../../core/socket_info.h"
#include "udomain.h" /* new_udomain, free_udomain */
#include "usrloc.h"
#include "utime.h"
#include "ims_usrloc_pcscf_mod.h"
#include "pcontact.h"

dlist_t *root = 0;

static inline int find_dlist(str *_n, dlist_t **_d)
{
	dlist_t *ptr;

	ptr = root;
	while(ptr) {
		if((_n->len == ptr->name.len) && !memcmp(_n->s, ptr->name.s, _n->len)) {
			*_d = ptr;
			return 0;
		}

		ptr = ptr->next;
	}

	return 1;
}

/*!
 * \brief Get all contacts from the usrloc, in partitions if wanted
 *
 * Return list of all contacts for all currently registered
 * users in all domains. The caller must provide buffer of
 * sufficient length for fitting all those contacts. In the
 * case when buffer was exhausted, the function returns
 * estimated amount of additional space needed, in this
 * case the caller is expected to repeat the call using
 * this value as the hint.
 *
 * Information is packed into the buffer as follows:
 *
 * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
 * |received host.len|received host.s|spi_uc|spi_us|spi_pc|spi_ps|port_uc|port_us|port_pc|port_ps|
 * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
 * |received host.len|received host.s|spi_uc|spi_us|spi_pc|spi_ps|port_uc|port_us|port_pc|port_ps|
 * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
 * |.............................................................................................|
 * +-----------------+---------------+------+------+------+------+-------+-------+-------+-------+
 * |0000|
 * +----+
 *
 * \param buf target buffer
 * \param len length of buffer
 * \param flags contact flags
 * \param part_idx part index
 * \param part_max maximal part
 * \return 0 on success, positive if buffer size was not sufficient, negative on failure
 */
static inline int get_all_mem_ucontacts(void *buf, int len, unsigned int flags,
		unsigned int part_idx, unsigned int part_max)
{
	dlist_t *p;
	pcontact_t *c;
	void *cp;
	int shortage;
	int needed;
	int i = 0;
	cp = buf;
	shortage = 0;

	/* Reserve space for terminating 0000 */
	len -= sizeof(int);

	for(p = root; p != NULL; p = p->next) {
		for(i = 0; i < p->d->size; i++) {
			if((i % part_max) != part_idx) {
				continue;
			}

			lock_ulslot(p->d, i);
			if(p->d->table[i].n <= 0) {
				unlock_ulslot(p->d, i);
				continue;
			}

			for(c = p->d->table[i].first; c != NULL; c = c->next) {
				if(c->received_host.s && c->security_temp
						&& c->security_temp->type == SECURITY_IPSEC) {
					needed = (int)(sizeof(c->received_host.len)
								   + c->received_host.len
								   + sizeof(
										   c->security_temp->data.ipsec->spi_uc)
								   + sizeof(
										   c->security_temp->data.ipsec->spi_us)
								   + sizeof(
										   c->security_temp->data.ipsec->spi_pc)
								   + sizeof(
										   c->security_temp->data.ipsec->spi_ps)
								   + sizeof(c->security_temp->data.ipsec
													->port_uc)
								   + sizeof(c->security_temp->data.ipsec
													->port_us)
								   + sizeof(c->security_temp->data.ipsec
													->port_pc)
								   + sizeof(c->security_temp->data.ipsec
													->port_ps));

					if(len >= needed) {
						memcpy(cp, &c->received_host.len,
								sizeof(c->received_host.len));
						cp = (char *)cp + sizeof(c->received_host.len);
						memcpy(cp, c->received_host.s, c->received_host.len);
						cp = (char *)cp + c->received_host.len;

						memcpy(cp, &c->security_temp->data.ipsec->spi_uc,
								sizeof(c->security_temp->data.ipsec->spi_uc));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->spi_uc);

						memcpy(cp, &c->security_temp->data.ipsec->spi_us,
								sizeof(c->security_temp->data.ipsec->spi_us));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->spi_us);

						memcpy(cp, &c->security_temp->data.ipsec->spi_pc,
								sizeof(c->security_temp->data.ipsec->spi_pc));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->spi_pc);

						memcpy(cp, &c->security_temp->data.ipsec->spi_ps,
								sizeof(c->security_temp->data.ipsec->spi_ps));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->spi_ps);

						memcpy(cp, &c->security_temp->data.ipsec->port_uc,
								sizeof(c->security_temp->data.ipsec->port_uc));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->port_uc);

						memcpy(cp, &c->security_temp->data.ipsec->port_us,
								sizeof(c->security_temp->data.ipsec->port_us));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->port_us);

						memcpy(cp, &c->security_temp->data.ipsec->port_pc,
								sizeof(c->security_temp->data.ipsec->port_pc));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->port_pc);

						memcpy(cp, &c->security_temp->data.ipsec->port_ps,
								sizeof(c->security_temp->data.ipsec->port_ps));
						cp = (char *)cp
							 + sizeof(c->security_temp->data.ipsec->port_ps);

						len -= needed;
					} else {
						shortage += needed;
					}
				}
			}
			unlock_ulslot(p->d, i);
		}
	}
	/* len < 0 is possible, if size of the buffer < sizeof(c->c.len) */
	if(len >= 0)
		memset(cp, 0, sizeof(int));

	/* Shouldn't happen */
	if(shortage > 0 && len > shortage) {
		abort();
	}

	shortage -= len;

	return shortage > 0 ? shortage : 0;
}

int get_all_ucontacts(void *buf, int len, unsigned int flags,
		unsigned int part_idx, unsigned int part_max)
{
	return get_all_mem_ucontacts(buf, len, flags, part_idx, part_max);
}

static inline int new_dlist(str *_n, dlist_t **_d)
{
	dlist_t *ptr;

	/* Domains are created before ser forks,
	 * so we can create them using pkg_malloc
	 */
	ptr = (dlist_t *)shm_malloc(sizeof(dlist_t));
	if(ptr == 0) {
		LM_ERR("no more share memory\n");
		return -1;
	}
	memset(ptr, 0, sizeof(dlist_t));

	/* copy domain name as null terminated string */
	ptr->name.s = (char *)shm_malloc(_n->len + 1);
	if(ptr->name.s == 0) {
		LM_ERR("no more memory left\n");
		shm_free(ptr);
		return -2;
	}

	memcpy(ptr->name.s, _n->s, _n->len);
	ptr->name.len = _n->len;
	ptr->name.s[ptr->name.len] = 0;

	if(new_udomain(&(ptr->name), ul_hash_size, &(ptr->d)) < 0) {
		LM_ERR("creating domain structure failed\n");
		shm_free(ptr->name.s);
		shm_free(ptr);
		return -3;
	}

	*_d = ptr;
	return 0;
}

int get_udomain(const char *_n, udomain_t **_d)
{
	dlist_t *d;
	str s;

	s.s = (char *)_n;
	s.len = strlen(_n);

	if(find_dlist(&s, &d) == 0) {
		*_d = d->d;
		return 0;
	}
	*_d = NULL;
	return -1;
}

int register_udomain(const char *_n, udomain_t **_d)
{
	dlist_t *d;
	str s;

	s.s = (char *)_n;
	s.len = strlen(_n);

	if(find_dlist(&s, &d) == 0) {
		*_d = d->d;
		return 0;
	}

	if(new_dlist(&s, &d) < 0) {
		LM_ERR("failed to create new domain\n");
		return -1;
	}

	d->next = root;
	root = d;

	*_d = d->d;
	return 0;
}

void free_all_udomains(void)
{
	dlist_t *ptr;

	while(root) {
		ptr = root;
		root = root->next;

		free_udomain(ptr->d);
		shm_free(ptr->name.s);
		shm_free(ptr);
	}
}

void print_all_udomains(FILE *_f)
{
	dlist_t *ptr;

	ptr = root;

	fprintf(_f, "===Domain list===\n");
	while(ptr) {
		print_udomain(_f, ptr->d);
		ptr = ptr->next;
	}
	fprintf(_f, "===/Domain list===\n");
}

int synchronize_all_udomains(void)
{
	int res = 0;
	dlist_t *ptr;

	get_act_time(); /* Get and save actual time */

	for(ptr = root; ptr; ptr = ptr->next)
		mem_timer_udomain(ptr->d);

	return res;
}

int find_domain(str *_d, udomain_t **_p)
{
	dlist_t *d;

	if(find_dlist(_d, &d) == 0) {
		*_p = d->d;
		return 0;
	}

	return 1;
}

unsigned long get_number_of_contacts(void)
{
	long numberOfUsers = 0;

	dlist_t *current_dlist;

	current_dlist = root;

	while(current_dlist) {
		numberOfUsers += get_stat_val(current_dlist->d->contacts);
		current_dlist = current_dlist->next;
	}

	return numberOfUsers;
}

unsigned long get_number_of_expired(void)
{
	long numberOfExpired = 0;

	dlist_t *current_dlist;

	current_dlist = root;

	while(current_dlist) {
		numberOfExpired += get_stat_val(current_dlist->d->expired);
		current_dlist = current_dlist->next;
	}

	return numberOfExpired;
}

unsigned long get_number_of_impu(void)
{
	long numberOfExpired = 0;

	dlist_t *current_dlist;

	current_dlist = root;

	while(current_dlist) {
		numberOfExpired += get_stat_val(current_dlist->d->expired);
		current_dlist = current_dlist->next;
	}

	return numberOfExpired;
}
