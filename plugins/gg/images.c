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

COMMAND(gg_command_image)
{
	gg_private_t *g = session_private_get(session);

	if (params[0] && params[1]) {
		FILE *f = fopen(params[1], "r");
		uint32_t size, crc32;
		int i;
		char *filename, *data, *uid;

	        struct gg_msg_richtext_format_img {
	                struct gg_msg_richtext rt;
	                struct gg_msg_richtext_format f;
	                struct gg_msg_richtext_image image;
	        } msg;

		if (!(uid = get_uid(session, params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}

		if (!f) {
			printq("file_doesnt_exist", params[1]);
			return -1;
		}
	
		/* finding size of file by seeking to the end and then 
		   checking where we are */
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		filename = xstrdup(params[1]);

		data = xmalloc(size);

		for (i = 0; !feof(f); i++) {
			data[i] = fgetc(f);
		}

		crc32 = gg_crc32(0, data, size);
		
	        msg.rt.flag=2;
	        msg.rt.length=13;
	        msg.f.position=0;
	        msg.f.font=GG_FONT_IMAGE;
	        msg.image.unknown1=0x0109;
	        msg.image.size=size;
	        msg.image.crc32=crc32;

		image_add_queue(filename, data, size, crc32); 

	        if (gg_send_message_richtext(g->sess, GG_CLASS_MSG, atoi(uid + 3), "", (const char *) &msg, sizeof(msg)) == -1) {
			printq("gg_image_error_send");
	                return -1;
	        }
		
		printq("gg_image_ok_send");

		xfree(filename);
		return 0;
	}

	return 0;	
}

/* 
 * image_add_queue()
 * 
 * data should be given as already allocated pointer 
 */
image_t *image_add_queue(char *filename, char *data, uint32_t size, uint32_t crc32)
{
	image_t i, *ip;

	memset(&i, 0, sizeof(i));

	i.filename = xstrdup(filename);
	i.data = data;
	i.size = size;
	i.crc32 = crc32; 

	ip = list_add(&images, &i, sizeof(i));

	return ip;
}

void image_remove_queue(image_t *i)
{
	debug("image_remove_queue( %d)\n", i->size);
	xfree(i->filename);
	xfree(i->data);

	list_remove(&images, i, 1);
}
