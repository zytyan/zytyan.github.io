#include "lib.h"

void memcpy(void *dest, const void *src, intptr_t n)
{
	for (intptr_t i = 0; i < n; i++) {
		((char *)dest)[i] = ((const char *)src)[i];
	}
}

void memset(void *dest, int value, intptr_t n)
{
	for (intptr_t i = 0; i < n; i++) {
		((unsigned char *)dest)[i] = (unsigned char)value;
	}
}

intptr_t strlen(const char *str)
{
	intptr_t len = 0;

	while (str[len])
		len++;

	return len;
}

int strncmp(const char *s1, const char *s2, intptr_t n)
{
	for (intptr_t i = 0; i < n; i++) {
		unsigned char c1 = (unsigned char)s1[i];
		unsigned char c2 = (unsigned char)s2[i];

		if (c1 != c2 || c1 == '\0' || c2 == '\0')
			return c1 - c2;
	}

	return 0;
}

void debug_puts(const char *str)
{
	while (*str) {
		__asm__ volatile("outb %0, %1" : : "a"(*str), "Nd"(0xE9));
		str++;
	}
}
