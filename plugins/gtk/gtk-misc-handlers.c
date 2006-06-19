/* here will be helper-handlers. gg, jabber, irc...
 * for example:
 *    -displayink gg tokens
 *    -displayink jabber register/search dialogs
 *    - etc.. WE WILL DISPLAY HERE EVERYTHINK TO BE MORE FRIENDLY TO USER... YEAAAH ;) 
 */

#include <ekg2-config.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>

extern plugin_t gtk_plugin;

#ifdef HAVE_LIBGADU
QUERY(gtk_gg_display_token) {
#warning "GTK: BUILDING EXPERIMENTAL SUPPORT FOR DISPLAYINK GG TOKENS"
	printf("XXX: gtk_gg_display_token()\n");
	return 0;
}
#endif

int gtk_misc_handlers_init() {
#ifdef HAVE_LIBGADU
	query_connect(&gtk_plugin, "gg-display-token", gtk_gg_display_token, NULL); 
#endif
	return 0;
}
