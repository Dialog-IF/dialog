
uint16_t unicode_to_upper(uint16_t ch);
uint16_t unicode_to_lower(uint16_t ch);
int utf8_to_unicode(uint16_t *dest, int ndest, const uint8_t *src);
int utf8_to_unicode_n(uint16_t *dest, int ndest, const uint8_t *src, int nsrc);
int unicode_to_utf8(uint8_t *dest, int ndest, const uint16_t *src);
int unicode_to_utf8_n(uint8_t *dest, int ndest, const uint16_t *src, int nsrc);
int full_unicode_to_utf8_single(uint8_t *dest, const uint32_t ch, const line_t line);
