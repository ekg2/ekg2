/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __EKG_OBJECTS_H
#define __EKG_OBJECTS_H

#include "char.h"
#include "xmalloc.h"

#define PROPERTY_INT_GET(object,property,type) \
	\
	type object##_##property##_get(object##_t *o) \
	{ \
		return (o) ? o->property : -1; \
	}

#define PROPERTY_INT_SET(object,property,type) \
	\
	int object##_##property##_set(object##_t *o, type v) \
	{ \
		if (!o) \
			return -1; \
		\
		o->property = v; \
		\
		return 0; \
	}

#define PROPERTY_INT(object,property,type) \
	\
	PROPERTY_INT_GET(object,property,type) \
	PROPERTY_INT_SET(object,property,type)



#define PROPERTY_STRING_GET(object,property) \
	\
	const char *object##_##property##_get(object##_t *o) \
	{ \
		return (o) ? o->property : NULL; \
	}


#define PROPERTY_CHART_GET(object,property) \
	\
	const CHAR_T *object##_##property##_get(object##_t *o) \
	{ \
		return (o) ? o->property : NULL; \
	}

#define PROPERTY_STRING_SET(object,property) \
	\
	int object##_##property##_set(object##_t *o, const char *v) \
	{ \
		if (!o) \
			return -1; \
		\
		xfree(o->property); \
		o->property = xstrdup(v); \
		\
		return 0; \
	}

#define PROPERTY_CHART_SET(object,property) \
	\
	int object##_##property##_set(object##_t *o, const CHAR_T *v) \
	{ \
		if (!o) \
			return -1; \
		\
		xfree(o->property); \
		o->property = xwcsdup(v); \
		\
		return 0; \
	}

#define PROPERTY_CHART(object,property) \
	\
	PROPERTY_CHART_SET(object, property) \
	PROPERTY_CHART_GET(object, property)

#define PROPERTY_STRING(object,property) \
\
PROPERTY_STRING_SET(object, property) \
PROPERTY_STRING_GET(object, property)


#define PROPERTY_PRIVATE_GET(object) \
	\
	void *object##_private_get(object##_t *o) \
	{ \
		return (o) ? o->priv : NULL; \
	}

#define PROPERTY_PRIVATE_SET(object) \
	\
	int object##_private_set(object##_t *o, void *v) \
	{ \
		if (!o) \
			return -1; \
		\
		o->priv = v; \
		\
		return 0; \
	}

#define PROPERTY_PRIVATE(object) \
	\
	PROPERTY_PRIVATE_GET(object) \
	PROPERTY_PRIVATE_SET(object)


#define PROPERTY_DATA_GET(object) \
	\
	void *object##_data_get(object##_t *o) \
	{ \
		return (o) ? o->data : NULL; \
	}

#define PROPERTY_DATA_SET(object) \
	\
	int object##_data_set(object##_t *o, void *d) \
	{ \
		if (!o) \
			return -1; \
		\
		o->data = d; \
		\
		return 0; \
	}

#define PROPERTY_DATA(object) \
	\
	PROPERTY_DATA_GET(object) \
	PROPERTY_DATA_SET(object)


#define PROPERTY_MISC_GET(object,property,type,null) \
	\
	type object##_##property##_get(object##_t *o) \
	{ \
		return (o) ? o->property : null; \
	}

#define PROPERTY_MISC_SET(object,property,type) \
	\
	int object##_##property##_set(object##_t *o, type v) \
	{ \
		if (!o) \
			return -1; \
		\
		o->property = v; \
		\
		return 0; \
	}

#define PROPERTY_MISC(object,property,type,null) \
	\
	PROPERTY_MISC_GET(object,property,type,null) \
	PROPERTY_MISC_SET(object,property,type)



#endif /* __EKG_OOP_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
