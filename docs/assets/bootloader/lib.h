#ifndef LIB_H
#define LIB_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef long intptr_t;

#define NULL ((void *)0)

static inline uint16_t rd16(const unsigned char *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t rd32(const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void wr16(unsigned char *p, uint16_t v)
{
	p[0] = v & 0xff;
	p[1] = v >> 8;
}

static inline void wr32(unsigned char *p, uint32_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = (v >> 24) & 0xff;
}

static inline void wr64(unsigned char *p, uint64_t v)
{
	wr32(p, (uint32_t)v);
	wr32(p + 4, (uint32_t)(v >> 32));
}

void memcpy(void *dest, const void *src, intptr_t n);
void memset(void *dest, int value, intptr_t n);
intptr_t strlen(const char *str);
int strncmp(const char *s1, const char *s2, intptr_t n);
void debug_puts(const char *str);

#endif // LIB_H
