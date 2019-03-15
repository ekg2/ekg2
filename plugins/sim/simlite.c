/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		       Piotr Domagalski <szalik@szalik.net>
 *		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *
 *  Idea and concept from SIM by Michal J. Kubski available at
 *  http://gg.wha.la/crypt/. Original source code can be found
 *  at http://gg.wha.la/sim.tar.gz
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/sha.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "simlite.h"

char *sim_key_path = NULL;
int sim_errno = 0;

/*
 * sim_seed_prng()
 */
static int sim_seed_prng()
{
	char rubbish[1024];
	struct {
		time_t time;
		void * foo;
		void * foo2;
	} data;

	data.time = time(NULL);
	data.foo = (void *) &data;
	data.foo2 = (void *) rubbish;

	RAND_seed((void *) &data, sizeof(data));
	RAND_seed((void *) rubbish, sizeof(rubbish));

	return sizeof(data) + sizeof(rubbish);
}

/*
 * sim_key_generate()
 *
 * tworzy par� kluczy i zapisuje je na dysku.
 *
 *  - uid - numer, dla kt�rego generujemy klucze.
 *
 * 0/-1
 */
int sim_key_generate(const char *uid)
{
	char path[PATH_MAX];
	RSA *keys = NULL;
	int res = -1;
	FILE *f = NULL;

	if (!RAND_status())
		sim_seed_prng();

	if (!(keys = RSA_generate_key(1024, RSA_F4, NULL, NULL))) {
		sim_errno = SIM_ERROR_RSA;
		goto cleanup;
	}

	snprintf(path, sizeof(path), "%s/%s.pem", sim_key_path, uid);

	if (!(f = fopen(path, "w"))) {
		sim_errno = SIM_ERROR_PUBLIC;
		goto cleanup;
	}

	if (!PEM_write_RSAPublicKey(f, keys)) {
		sim_errno = SIM_ERROR_PUBLIC;
		goto cleanup;
	}

	fclose(f);
	f = NULL;

	snprintf(path, sizeof(path), "%s/private-%s.pem", sim_key_path, uid);

	if (!(f = fopen(path, "w"))) {
		sim_errno = SIM_ERROR_PRIVATE;
		goto cleanup;
	}

	if (!PEM_write_RSAPrivateKey(f, keys, NULL, NULL, 0, NULL, NULL)) {
		sim_errno = SIM_ERROR_PUBLIC;
		goto cleanup;
	}

	fclose(f);
	f = NULL;

	res = 0;
	
cleanup:
	if (keys)
		RSA_free(keys);
	if (f)
		fclose(f);

	return res;
}

/*
 * sim_key_read()
 *
 * wczytuje klucz RSA podanego numer. klucz prywatny mo�na wczyta�, je�li
 * zamiasr numeru poda si� 0.
 *
 *  - uid - numer klucza.
 *
 * zaalokowany klucz RSA, kt�ry nale�y zwolni� RSA_free()
 */
static RSA *sim_key_read(const char *uid, const char *session)
{
	char path[PATH_MAX];
	FILE *f;
	RSA *key;
	unsigned long err;

	if (uid)
		snprintf(path, sizeof(path), "%s/%s.pem", sim_key_path, uid);
	else
		snprintf(path, sizeof(path), "%s/private-%s.pem", sim_key_path, session);

	if (!(f = fopen(path, "r")))
		return NULL;
	
	if (uid)
		key = PEM_read_RSAPublicKey(f, NULL, NULL, NULL);
	else
		key = PEM_read_RSAPrivateKey(f, NULL, NULL, NULL);

	if (!key) {
	
		err = ERR_get_error();

		debug("Error reading Private Key = %s\n",ERR_reason_error_string(err));
	}	
	fclose(f);

	return key;
}

/*
 * sim_key_fingerprint()
 *
 * zwraca fingerprint danego klucza.
 *
 *  - uid - numer posiadacza klucza.
 *
 * zaalokowany bufor.
 */
char *sim_key_fingerprint(const char *uid)
{
	RSA *key = sim_key_read(uid, NULL);
	unsigned char md_value[EVP_MAX_MD_SIZE], *buf, *newbuf;
	char *result = NULL;
	EVP_MD_CTX *ctx;
	unsigned int md_len;
	int size, i;

	if (!key) {
		debug("out (%s)\n", uid);
		return NULL;
	}

	if (uid)
		size = i2d_RSAPublicKey(key, NULL);
	else
		size = i2d_RSAPrivateKey(key, NULL);

	if (!(newbuf = buf = malloc(size))) {
		sim_errno = SIM_ERROR_MEMORY;
		goto cleanup;
	}

	if (uid)
		size = i2d_RSAPublicKey(key, &newbuf);
	else
		size = i2d_RSAPrivateKey(key, &newbuf);

	if (!(ctx = EVP_MD_CTX_new())) {
		free(buf);
		sim_errno = SIM_ERROR_MEMORY;
		goto cleanup;
	}
	EVP_DigestInit(ctx, EVP_sha1());
	EVP_DigestUpdate(ctx, buf, size);
	EVP_DigestFinal(ctx, md_value, &md_len);
	EVP_MD_CTX_free(ctx);

	free(buf);

	if (!(result = malloc(md_len * 3))) {
		sim_errno = SIM_ERROR_MEMORY;
		goto cleanup;
	}

	for (i = 0; i < md_len; i++)
		sprintf(result + i * 3, (i != md_len - 1) ? "%.2x:" : "%.2x", md_value[i]);

cleanup:
	RSA_free(key);

	return result;
}

/*
 * sim_strerror()
 *
 * zamienia kod b��du simlite na komunikat.
 *
 *  - error - kod b��du.
 *
 * zwraca statyczny bufor.
 */
const char *sim_strerror(int error)
{
	const char *result = "Unknown error";
	
	switch (error) {
		case SIM_ERROR_SUCCESS:
			result = "Success";
			break;
		case SIM_ERROR_PUBLIC:
			result = "Unable to read public key";
			break;
		case SIM_ERROR_PRIVATE:
			result = "Unable to read private key";
			break;
		case SIM_ERROR_RSA:
			result = "RSA error";
			break;
		case SIM_ERROR_BF:
			result = "Blowfish error";
			break;
		case SIM_ERROR_RAND:
			result = "Not enough random data";
			break;
		case SIM_ERROR_MEMORY:
			result = "Out of memory";
			break;
		case SIM_ERROR_INVALID:
			result = "Invalid message format (too short, etc.)";
			break;
		case SIM_ERROR_MAGIC:
			result = "Invalid magic value";
			break;
	}

	return result;
}

/*
 * sim_message_encrypt()
 *
 * szyfruje wiadomo�� przeznaczon� dla podanej osoby, zwracaj�c jej
 * zapis w base64.
 *
 *  - message - tre�� wiadomo�ci,
 *  - uid - numer odbiorcy.
 *
 * zaalokowany bufor.
 */
char *sim_message_encrypt(const unsigned char *message, const char *uid)
{
	sim_message_header head;	/* nag��wek wiadomo�ci */
	unsigned char ivec[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned char bf_key[16];	/* klucz symetryczny Blowfisha */
	unsigned char bf_key_rsa[128];	/* symetryczny szyfrowany RSA */
	BIO *mbio = NULL, *cbio = NULL, *bbio = NULL;
	RSA *public = NULL;
	int res_len;
	char *res = NULL, *tmp;

	/* wczytaj klucz publiczny delikwenta */
	if (!(public = sim_key_read(uid, NULL))) {
		sim_errno = SIM_ERROR_PUBLIC;
		goto cleanup;
	}

	/* trzeba nakarmi� potwora? */
	if (!RAND_status())
		sim_seed_prng();

	/* wylosuj klucz symetryczny */
	if (RAND_bytes(bf_key, sizeof(bf_key)) != 1) {
		sim_errno = SIM_ERROR_RAND;
		goto cleanup;
	}

	/* teraz go szyfruj kluczem publiczym */
	if (RSA_public_encrypt(sizeof(bf_key), bf_key, bf_key_rsa, public, RSA_PKCS1_OAEP_PADDING) == -1) {
		sim_errno = SIM_ERROR_RSA;
		goto cleanup;
	}

	/* przygotuj zawarto�� pakietu do szyfrowania blowfishem */
	memset(&head, 0, sizeof(head));
#ifndef WORDS_BIGENDIAN
	head.magic = SIM_MAGIC_V1;
#else
	head.magic = SIM_MAGIC_V1_BE;
#endif

	if (RAND_bytes(head.init, sizeof(head.init)) != 1) {
		sim_errno = SIM_ERROR_RAND;
		goto cleanup;
	}

	/* przygotuj base64 */
	mbio = BIO_new(BIO_s_mem());
	bbio = BIO_new(BIO_f_base64());
	BIO_set_flags(bbio, BIO_FLAGS_BASE64_NO_NL);
	BIO_push(bbio, mbio);

	/* mamy ju� klucz symetryczny szyfrowany przez rsa, wi�c mo�emy
	 * go wrzuci� do base64 */
	BIO_write(bbio, bf_key_rsa, sizeof(bf_key_rsa));

	/* teraz b�dziemy szyfrowa� blowfishem */
	cbio = BIO_new(BIO_f_cipher());
	BIO_set_cipher(cbio, EVP_bf_cbc(), bf_key, ivec, 1);

	BIO_push(cbio, bbio);

	BIO_write(cbio, &head, sizeof(head));
	BIO_write(cbio, message, xstrlen((char*) message));
		(void) BIO_flush(cbio);

	/* zachowaj wynik */
	res_len = BIO_get_mem_data(mbio, (unsigned char*) &tmp);

	if (!(res = malloc(res_len + 1))) {
		sim_errno = SIM_ERROR_MEMORY;
		goto cleanup;
	}
	memcpy(res, tmp, res_len);
	res[res_len] = 0;

	sim_errno = SIM_ERROR_SUCCESS;

cleanup:
	/* zwolnij pami�� */
	if (bbio)
		BIO_free(bbio);
	if (mbio)
		BIO_free(mbio);
	if (cbio)
		BIO_free(cbio);
	if (public)
		RSA_free(public);

	return res;
}

/*
 * sim_message_decrypt()
 *
 * odszyfrowuje wiadomo�� od podanej osoby.
 *
 *  - message - tre�� wiadomo�ci,
 *  - uid - numer nadawcy.
 *
 * zaalokowany bufor.
 */
char *sim_message_decrypt(const unsigned char *message, const char *uid)
{
	sim_message_header head;	/* nag��wek wiadomo�ci */
	unsigned char ivec[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned char bf_key[16];	/* klucz symetryczny Blowfisha */
	unsigned char bf_key_rsa[128];	/* symetryczny szyfrowany RSA */
	BIO *mbio = NULL, *cbio = NULL, *bbio = NULL;
	RSA *private_key = NULL;
	unsigned char *buf = NULL, *data;
	char *res = NULL;
	int len;

	/* je�li wiadomo�� jest kr�tsza ni� najkr�tsza zaszyfrowana,
	 * nie ma sensu si� bawi� w pr�by odszyfrowania. */
	if (xstrlen((char*) message) < 192) {
		sim_errno = SIM_ERROR_INVALID;
		goto cleanup;
	}
	
	/* wczytaj klucz prywatny */
	if (!(private_key = sim_key_read(0, uid))) {
		sim_errno = SIM_ERROR_PRIVATE;
		goto cleanup;
	}
	
	mbio = BIO_new(BIO_s_mem());
	bbio = BIO_new(BIO_f_base64());
	BIO_set_flags(bbio, BIO_FLAGS_BASE64_NO_NL);
	BIO_push(bbio, mbio);
	BIO_write(mbio, message, xstrlen((char*) message));
		(void) BIO_flush(mbio);

	if (BIO_read(bbio, bf_key_rsa, sizeof(bf_key_rsa)) < sizeof(bf_key_rsa)) {
		sim_errno = SIM_ERROR_INVALID;
		goto cleanup;
	}

	if (RSA_private_decrypt(sizeof(bf_key_rsa), bf_key_rsa, bf_key, private_key, RSA_PKCS1_OAEP_PADDING) == -1) {
		sim_errno = SIM_ERROR_RSA;
		goto cleanup;
	}

	len = BIO_pending(bbio);

	if (!(buf = malloc(len))) {
		sim_errno = SIM_ERROR_MEMORY;
		goto cleanup;
	}

	if (len < sizeof(head)) {
		sim_errno = SIM_ERROR_INVALID;
		goto cleanup;
	}
	
	if ((len = BIO_read(bbio, buf, len)) == -1) {
		sim_errno = SIM_ERROR_INVALID;
		goto cleanup;
	}

	BIO_free(bbio);
	bbio = NULL;
	BIO_free(mbio);
	mbio = NULL;

	/* odszyfruj blowfisha */
	mbio = BIO_new(BIO_s_mem());
	cbio = BIO_new(BIO_f_cipher());
	BIO_set_cipher(cbio, EVP_bf_cbc(), bf_key, ivec, 0);
	BIO_push(cbio, mbio);

	BIO_write(cbio, buf, len);
		(void) BIO_flush(cbio);

	free(buf);
	buf = NULL;

	len = BIO_get_mem_data(mbio, (unsigned char*) &data);

	memcpy(&head, data, sizeof(head));

	/* XXX: Why isn't it working */
	/* if (head.magic != SIM_MAGIC_V1 || head.magic != SIM_MAGIC_V1_BE) {
		sim_errno = SIM_ERROR_MAGIC;
		goto cleanup;
	} */

	len -= sizeof(head);

	if (!(res = malloc(len + 1))) {
		sim_errno = SIM_ERROR_MEMORY;
		goto cleanup;
	}
	
	memcpy(res, data + sizeof(head), len);
	res[len] = 0;

cleanup:
	if (cbio)
		BIO_free(cbio);
	if (mbio)
		BIO_free(mbio);
	if (bbio)
		BIO_free(bbio);
	if (private_key)
		RSA_free(private_key);
	if (buf)
		free(buf);
	return ekg_cp_to_core(res);	/* XXX, what if message isn't encoded in cp-1250? */
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */


