// This library is adapted from https://github.com/rxi/uuid4 by rxi
// Which is licensed as follows:

/*
Copyright (c) 2018 rxi

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// The modified version used here is under the same (BSD) license as the rest of the Dialog compiler; see LICENSE file for details

#include <stdio.h>
#include <stdint.h>

static uint8_t randomness[16];

// On success, returns 0 and fills randomness with 128 random bits
// On failure, returns 1 and the contents of randomness are unspecified
static int fill_randomness();

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)

static int fill_randomness() {
	int res;
	FILE *fp = fopen("/dev/urandom", "rb");
	if(!fp) {
		return 1;
	}
	res = fread(randomness, 1, sizeof(randomness), fp);
	fclose(fp);
	if(res != sizeof(randomness)) {
		return 1;
	}
	return 0;
}

#elif defined(_WIN32)

#include <windows.h>
#include <wincrypt.h>

static int fill_randomness() {
	int res;
	HCRYPTPROV hCryptProv;
	res = CryptAcquireContext(
		&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	if(!res) {
		return 1;
	}
	res = CryptGenRandom(hCryptProv, (DWORD) sizeof(randomness), (PBYTE) randomness);
	CryptReleaseContext(hCryptProv, 0);
	if(!res) {
		return 1;
	}
	return 0;
}

#else

// No portable way to get enough randomness, just fail
static int fill_randomness() {
	return 1;
}

#endif

// On success, returns 0 and fills dest (which must be at least 37 entries long) with a valid randomly-generated IFID
// On failure, returns 1 and the contents of dest are unspecified
int generate_ifid(char *dest) {
	const char *p = "xxxxxxxx-xxxx-4xxx-Yxxx-xxxxxxxxxxxx";
	const char *chars = "0123456789ABCDEF";
	int i = 0, n;
	if(fill_randomness()) {
		return 1;
	}
	while(*p) {
		n = randomness[i >> 1];
		n = (i & 1) ? (n >> 4) : (n & 0xf);
		switch(*p) {
			case 'x': *dest = chars[n];             i++; break;
			case 'Y': *dest = chars[(n & 0x3) + 8]; i++; break;
			default : *dest = *p;
		}
		dest++, p++;
	}
	*dest = 0;
	return 0;
}
