static inline int check_text() {
	unsigned char *text = TEXT;
	unsigned char *password;

#if ULTRA_DEBUG
	printf("%s\n", realpass);
#endif
	for (password = realpass; *password && *text; password++, text++) {
		if (*text != *password)
			return 1;
	}

	return !(*password == '\0' && *text == '\0');
}

