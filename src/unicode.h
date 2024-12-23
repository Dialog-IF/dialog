
int utf8_to_unicode(uint16_t *dest, int ndest, uint8_t *src);
int utf8_to_unicode_n(uint16_t *dest, int ndest, uint8_t *src, int nsrc);
int unicode_to_utf8(uint8_t *dest, int ndest, uint16_t *src);
