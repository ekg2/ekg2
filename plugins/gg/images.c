/* $id */

/*
 *  (C) Copyright 2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *  		  2006 Adam Mikuta <adamm@ekg2.org>
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

#include "ekg2-config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <libgadu.h>

#include "images.h"
#include "gg.h"

list_t images = NULL;
int gg_config_image_size;
int gg_config_get_images;
char *gg_config_images_dir;

static image_t *image_add_queue(const char *filename, char *data, uint32_t size, uint32_t crc32);

/* 
 * gg_changed_images()
 *
 * called when some images_* variables are changed
 */
void gg_changed_images(const char *var)
{
	if (gg_config_image_size > 255) {
		gg_config_image_size = 255;
	} else
		if (gg_config_image_size < 20)
			gg_config_image_size = 20;

	if (!in_autoexec) 
		print("config_must_reconnect");
}



COMMAND(gg_command_image)
{
	gg_private_t *g = session_private_get(session);
	FILE *f;
	uint32_t size, crc32;
	int i;
	const char *filename	= params[1];
	char *data, *uid;

        struct gg_msg_richtext_format_img {
                struct gg_msg_richtext rt;
                struct gg_msg_richtext_format f;
                struct gg_msg_richtext_image image;
        } msg;

	if (!(uid = get_uid(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}
	if (!(f = fopen(filename, "r"))) {
		printq("file_doesnt_exist", filename);
		return -1;
	}
	
	/* finding size of file by seeking to the end and then 
	   checking where we are */
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	data = xmalloc(size);

	for (i = 0; !feof(f); i++) {
		data[i] = fgetc(f);
	}
	fclose(f);

	crc32 = gg_crc32(0, (unsigned char *) data, size);
	
        msg.rt.flag=2;
        msg.rt.length=13;
        msg.f.position=0;
        msg.f.font=GG_FONT_IMAGE;
        msg.image.unknown1=0x0109;
        msg.image.size=size;
        msg.image.crc32=crc32;

	image_add_queue(filename, data, size, crc32); 

        if (gg_send_message_richtext(g->sess, GG_CLASS_MSG, atoi(uid + 3), (const unsigned char *) "", (const unsigned char *) &msg, sizeof(msg)) == -1) {
		printq("gg_image_error_send");
                return -1;
        }
		
	printq("gg_image_ok_send");

	return 0;
}

/* 
 * image_add_queue()
 * 
 * data should be given as already allocated pointer 
 */
static image_t *image_add_queue(const char *filename, char *data, uint32_t size, uint32_t crc32)
{
	image_t *i = xmalloc(sizeof(image_t));

	i->filename = xstrdup(filename);
	i->data = data;
	i->size = size;
	i->crc32 = crc32; 

	return list_add(&images, i);
}

void image_remove_queue(image_t *i)
{
	debug("image_remove_queue( %d)\n", i->size);
	xfree(i->filename);
	xfree(i->data);

	list_remove(&images, i, 1);
}

void image_flush_queue()
{
	list_t l;

	if (!images)
		return;

	for (l = images; l; l = l->next) {
		image_t *i = l->data;

	        xfree(i->filename);
	        xfree(i->data);
	}

        list_destroy(images, 1);
        images = NULL;
}
