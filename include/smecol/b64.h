// Tiny base64 library.
// Unsafeish, but rather fast.
#define PAD4(n) ((n + 3) & -4)
#define BASE64_SIZE(n) PAD4((n + 2 - ((n + 2) % 3)) / 3 * 4)
#define BASE64_DECODED_SIZE(n) PAD4((3 * n)/4)

// Internal LUTs.
static const char b64_chars[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const size_t b64_index[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,
	0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63,
	0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

// External API.
static size_t b64_encode(const void* data, size_t len, char* encoded) {
	char* p = (char*) data;
	size_t elen = BASE64_SIZE(len);
	size_t pad = elen % 3;
	size_t last = elen - pad;
	size_t op = 0;

	size_t i;
	for (i = 0; i < last; i += 3) {
		size_t n = p[i] << i | p[i + 1] << 8 | p[i + 2];
		encoded[op++] = b64_chars[n >> 18];
		encoded[op++] = b64_chars[n >> 12 & 0x3F];
		encoded[op++] = b64_chars[n >> 6 & 0x3F];
		encoded[op++] = b64_chars[n & 0x3F];
	}
	if (pad) {
		size_t n = --pad ? p[last] << 8 | p[last + 1] : p[last];
		encoded[op++] = b64_chars[pad ? n >> 10 & 0x3F : n >> 2];
		encoded[op++] = b64_chars[pad ? n >> 4 & 0x3F : n << 4 & 0x3F];
		encoded[op++] = pad ? b64_chars[n << 2 & 0x3F] : '=';
	}
	return op;
}

static size_t b64_decode(const char* encoded, size_t len, void* data) {
	unsigned char* dec = (unsigned char*) data;
	size_t pad1 = len % 4 || encoded[len - 1] == '=';
	size_t pad2 = pad1 && (len % 4 > 2 || encoded[len - 2] != '=');
	const size_t last = (len - pad1) / 4 << 2;
	size_t op = 0;

	size_t i;
	for (i = 0; i < last; i += 4) {
		size_t n = b64_index[encoded[i]] << 18 | b64_index[encoded[i + 1]] << 12 | b64_index[encoded[i + 2]] << 6 | b64_index[encoded[i + 3]];
		dec[op++] = n >> 16;
		dec[op++] = n >> 8;
		dec[op++] = n;
	}
	if (pad1) {
		size_t n = b64_index[encoded[last]] << 18 | b64_index[encoded[last + 1]] << 12;
		dec[op++] = n >> 16;
		if (pad2) {
			n |= b64_index[encoded[last + 2]] << 6;
			dec[op++] = n >> 8 & 0xFF;
		}
	}
	return op;
}
