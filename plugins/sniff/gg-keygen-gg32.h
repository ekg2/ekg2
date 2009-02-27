/* stolen from libgadu, cache created by me. */
static unsigned int gg_login_hash() {
	static unsigned int saved_y;
	static unsigned char *saved_pos;
	static unsigned char saved_zn;

	unsigned int y;
	unsigned char *password;

	if (saved_pos && *saved_pos == saved_zn) {
		y = saved_y;
		password = saved_pos + 1;
	} else {
		y = SEED;
		password = realpass;
	}

/* I have no idea, how to crack/optimize this algo. Maybe someone? */
	do {
		register unsigned char zn = *password;
		register unsigned char z;

		y ^= zn;
		y += zn;

		y ^= (zn << 8);
		y -= (zn << 16);
		y ^= (zn << 24);

		z = y & 0x1F;
		y = (y << z) | (y >> (32 - z));

		if (*(++password)) {
			y ^= (zn << 24);
			y += (zn << 24);

			if (password[1] == '\0') {
				saved_y = y;
				saved_zn = zn;
				saved_pos = &password[-1];
			}
		}
	} while (*password);

#if ULTRA_DEBUG
	printf("%s -> 0x%x\n", realpass, y);
#endif
	return y;
}

