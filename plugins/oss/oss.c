#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include <ekg/audio.h>
#include <ekg/debug.h>
#include <ekg/commands.h>
#include <ekg/plugins.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

char *config_audio_device = NULL;

PLUGIN_DEFINE(oss, PLUGIN_AUDIO, NULL);
AUDIO_DEFINE(oss);

typedef struct {
	char *path;
	int fd;
	int freq;
	int sample;
	int channels;
	int bufsize;

	string_t buf;

	watch_t *read_watch, *write_watch;
	int read_usage, write_usage;
} oss_device_t;

list_t oss_devices;

/* mhhh. what about /dev/mixer? */

typedef struct {
	oss_device_t *dev;
} oss_private_t;


WATCHER(oss_read) {
	oss_device_t *dev       = data;
	char *buf;
	int len;

	string_clear(dev->buf);

	if (type) return -1;

	buf		= xmalloc(dev->bufsize);
	len		= read(fd, buf, dev->bufsize);
	
	debug("oss_read read: %d bytes\n", len);
	string_append_raw(dev->buf, buf, len);
	xfree(buf);

	return len;
}

#if 0
WATCHER(oss_write) {
	oss_device_t *dev       = data;
	/* mix */
	return -1;
}
#endif

WATCHER_AUDIO(oss_audio_read) {
	oss_private_t *priv	= data;
	oss_device_t *dev	= priv->dev;

	if (type == 1) 
		return 0;

	if (!data) return -1;
	if (!dev->buf->str || !dev->buf->len) { 
		return 0;
	}

	string_append_raw(buf, dev->buf->str, dev->buf->len);

	return dev->buf->len;
}

WATCHER_AUDIO(oss_audio_write) {
	oss_private_t *priv	= data;
	oss_device_t *dev	= priv->dev;

	int len, maxlen;
	if (type == 1) return 0;

	if (!data) return -1;
	maxlen = dev->bufsize;
	if (maxlen > buf->len) maxlen = buf->len;

	len = write(fd, buf->str, maxlen);

	return len;
}

oss_device_t *oss_device_find(const char *path, int way, int freq, int sample, int channels) {
	list_t l;

	for (l = oss_devices; l; l = l->next) {
		oss_device_t *dev = l->data;

		if (!xstrcmp(dev->path, path) && dev->freq == freq && dev->sample == sample && dev->channels == channels)
			return dev;
	}
	return NULL;
}

int oss_device_free(oss_device_t *dev, int way) {
	if (!dev) return -1;

	if (way == AUDIO_READ)	dev->read_usage--;
	if (way == AUDIO_WRITE) dev->write_usage--;

	if (!dev->read_usage) 	{ watch_free(dev->read_watch); dev->read_watch = NULL; }
	if (!dev->write_usage)	{ watch_free(dev->write_watch); dev->write_watch = NULL; } 

	if (!dev->read_watch && !dev->write_watch) {
		string_free(dev->buf, 1);
		close(dev->fd);
		xfree(dev->path);
		list_remove(&oss_devices, dev, 1);

		return 0;
	}
	
	return dev->read_usage + dev->write_usage;
}

oss_device_t *oss_device_new(const char *path, int way, int freq, int sample, int channels) {
	oss_device_t *dev;
	int value, voice_fd, ret;

	if ((voice_fd = open(path, O_RDWR)) == -1) return NULL;

	dev = xmalloc(sizeof(oss_device_t));
	dev->buf	= string_init(NULL);

	dev->path	= xstrdup(path);
	dev->fd		= voice_fd;
	dev->freq	= freq;
	dev->sample	= sample;
	dev->channels	= channels;

	ret = ioctl(voice_fd, SNDCTL_DSP_SPEED, &(freq));
	debug("oss_device_new() ioctl SNDCTL_DSP_SPEED freq: %d (%d) ret: %d\n", freq, dev->freq, ret);
	ret = ioctl(voice_fd, SNDCTL_DSP_SAMPLESIZE, &(sample));
	debug("oss_device_new() ioctl SNDCTL_DSP_SPEED sample: %d (%d) ret: %d\n", sample, dev->sample, ret);
	ret = ioctl(voice_fd, SNDCTL_DSP_CHANNELS, &(channels));
	debug("oss_device_new() ioctl SNDCTL_DSP_SPEED chan: %d (%d) ret: %d\n", channels, dev->channels, ret);

	value = AFMT_S16_LE;
	ioctl(voice_fd, SNDCTL_DSP_SETFMT, &value);

	if ((ioctl(voice_fd, SNDCTL_DSP_GETBLKSIZE, &(dev->bufsize)) == -1)) {
		/* to mamy problem.... */
		dev->bufsize = 4096;
	}
//	dev->bufsize = 3200;

	list_add(&oss_devices, dev, 0);
	return dev;
}

AUDIO_CONTROL(oss_audio_control) {
	va_list ap;

	if (type == AUDIO_CONTROL_INIT) {
		audio_codec_t *co;
		audio_io_t *out;
		char *directory = NULL;

		va_start(ap, aio);
		co	= va_arg(ap, audio_codec_t *);
		out	= va_arg(ap, audio_io_t *);
		va_end(ap);

		if (way == AUDIO_READ)	directory = "__input";
		if (way == AUDIO_WRITE) directory = "__output";

	/* :) */
		if (co) 
			co->c->control_handler(AUDIO_CONTROL_SET, AUDIO_RDWR, co, directory, "oss");
			out->a->control_handler(AUDIO_CONTROL_SET, AUDIO_WRITE, out, directory, "oss");

		return (void *) 1;
	}

	if ((type == AUDIO_CONTROL_SET && !aio) || type == AUDIO_CONTROL_GET) {
		char *attr;
		const char *device = NULL;
		int freq = 0, sample = 0, channels = 0;

		const char *pathname;
		int voice_fd;
		oss_private_t	*priv = NULL; 
		oss_device_t	*dev  = NULL;
		
		if (type == AUDIO_CONTROL_GET) {
			if (!aio) return NULL;
			priv = aio->private;
			dev = priv->dev;

		} else	{
			priv = xmalloc(sizeof(oss_private_t));
		}

		va_start(ap, aio);
		while ((attr = va_arg(ap, char *))) {
			if (type == AUDIO_CONTROL_GET) {
				char **value = va_arg(ap, char **);
				debug("[oss_audio_control AUDIO_CONTROL_GET] attr: %s poi: 0x%x\n", attr, value);

				if (!xstrcmp(attr, "freq"))		*value = xstrdup(itoa(dev->freq));
				else if (!xstrcmp(attr, "sample"))	*value = xstrdup(itoa(dev->sample));
				else if (!xstrcmp(attr, "channels"))	*value = xstrdup(itoa(dev->channels));
				else if (!xstrcmp(attr, "format"))	*value = xstrdup("pcm");
				else					*value = NULL;
			} else { 
				char *val = va_arg(ap, char *);
				int v;
				debug("[oss_audio_control AUDIO_CONTROL_INIT] attr: %s value: %s\n", attr, val);

				if (!xstrcmp(attr, "device"))		device = val;
				else if (!xstrcmp(attr, "freq"))	{ v = atoi(val);	freq = v;	} 
				else if (!xstrcmp(attr, "sample"))	{ v = atoi(val);	sample = v;	} 
				else if (!xstrcmp(attr, "channels"))	{ v = atoi(val);	channels = v;	} 
			}
		}
		va_end(ap);

		if (type == AUDIO_CONTROL_GET) return aio;

		pathname = (device) ? device : config_audio_device;

		if (!freq)	freq = 8000;
		if (!sample)	sample = 16;
		if (!channels)	channels = 1;

				dev = oss_device_find(pathname, way, freq, sample, channels);
		if (!dev) 	dev = oss_device_new(pathname, way, freq, sample, channels);
		if (!dev)	return NULL;

		voice_fd = dev->fd;

		if (way == AUDIO_READ && !dev->read_watch) 	{ dev->read_usage++;	dev->read_watch		= watch_add(&oss_plugin, voice_fd, WATCH_READ, oss_read, dev);		} 
//		if (way == AUDIO_WRITE && !dev->write_watch)	{ dev->write_usage++;	dev->write_watch	= watch_add(&oss_plugin, voice_fd, WATCH_WRITE_LINE, oss_write, dev);	}

		priv->dev 	= dev;

		aio		= xmalloc(sizeof(audio_io_t));
		aio->a  	= &oss_audio;
		aio->fd 	= voice_fd;
		aio->private 	= priv;

	} else if (type == AUDIO_CONTROL_DEINIT && aio) {
		oss_private_t *priv = aio->private;
		oss_device_free(priv->dev, way);
		xfree(priv);
		aio = NULL;
	} else if (type == AUDIO_CONTROL_HELP) {
		static char *arr[] = { 
			"-oss",		"",	/* bidirectional, no required params	*/
			"-oss:device",	"*",	/* bidirectional, name of device	*/
			"-oss:freq",	"*",	/* bidirectional, freq.			*/
			"-oss:sample",	"*",	/* bidirectional, sample		*/
			"-oss:channels","*",	/* bidirectional, number of channels	*/
			NULL, };
		return arr;
	}

	return aio;
}

static QUERY(oss_setvar_default) {
	xfree(config_audio_device);
	config_audio_device = xstrdup("/dev/dsp");
	return 0;
}

static COMMAND(oss_cmd_record) {
	const char *device 	= config_audio_device;
	const char *freq	= "";
	const char *sample 	= "";
	const char *channels 	= "";
	const char *filename	= "";

	char **array;
	int i;

	if (!params[0] || match_arg(params[0], 'l', "list", 2)) {
		/* XXX, list */

		return 0;
	}
	/* else here, create new. */

	array = array_make(params[0], " ", 0, 1, 1);

	for (i=0; array[i]; i++) {
		if (match_arg(array[i], 'f', "filename", 2) && array[i+1]) {
			filename = array[++i];
			continue;
		}
		if (match_arg(array[i], 'd', "device", 2) && array[i+1]) {
			device = array[++i];
			continue;
		}
		if (match_arg(array[i], 'F', "frequency", 2) && array[i+1]) {
			freq = array[++i];
			continue;
		}
		if (match_arg(array[i], 'c', "channels", 2) && array[i+1]) {
			channels = array[++i];
			continue;
		}
		if (match_arg(array[i], 's', "sample", 2) && array[i+1]) {
			sample = array[++i];
			continue;
		}

		/* if it's last option and it's not command -> filename */
		if (array[i][0] != '-' && !array[i+1]) {
			filename = array[i];
			continue;
		}

		printq("invalid_params", name);
		array_free(array);
		return -1;
	}

	if (!filename) {
		printq("not_enough_params", name);	/* XXX, need better */
		array_free(array);
		return -1;
	}

	stream_create("Recording created by /oss:record",
			__AINIT((&oss_audio), AUDIO_READ, "freq", freq, "sample", sample, "channels", channels),
			NULL,
			__AINIT_F("stream", AUDIO_WRITE, "file", filename, "format", "guess"));
	/* XXX, check for errors */

	array_free(array);

	return 0;
}

EXPORT int oss_plugin_init(int prio) {
	va_list dummy;

	PLUGIN_CHECK_VER("oss");

	plugin_register(&oss_plugin, prio);
	oss_setvar_default(NULL, dummy);
	audio_register(&oss_audio);

	variable_add(&oss_plugin, ("audio_device"), VAR_STR, 1, &config_audio_device, NULL, NULL, NULL);
	query_connect(&oss_plugin, "set-vars-default", oss_setvar_default, NULL);

	command_add(&oss_plugin, "oss:record", "p", oss_cmd_record, 0, 
			"-c --chanels -d --device -f --filename -F --frequency -s --sample -l --list");

	return 0;
}

static int oss_plugin_destroy() {
	audio_unregister(&oss_audio);

	plugin_unregister(&oss_plugin);
	return 0;
}
