#ifndef LITHUANIAN_CHARS_H
#define LITHUANIAN_CHARS_H

#include <linux/kernel.h>
#include <linux/types.h>

struct lith_char {
    u8 utf8_first;
    u8 utf8_second;
    u8 fallback_ascii;
    bool prefer_cgram;
    u8 bitmap[8];
};

static const struct lith_char lithuanian_chars[18] = {
    /* lowercase: a, c, e, e-dot, i, s, u-ogonek, u-macron, z */
    { 0xC4, 0x85, 'a', false, { 0b00000, 0b01110, 0b00001, 0b01111, 0b10001, 0b01111, 0b00010, 0b00100 } }, /* ą */
    { 0xC4, 0x8D, 'c', true,  { 0b01010, 0b00100, 0b01110, 0b10000, 0b10000, 0b10000, 0b01110, 0b00000 } }, /* č */
    { 0xC4, 0x99, 'e', false, { 0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b01110, 0b00010, 0b00100 } }, /* ę */
    { 0xC4, 0x97, 'e', true,  { 0b00100, 0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b01110, 0b00000 } }, /* ė */
    { 0xC4, 0xAF, 'i', false, { 0b00100, 0b00000, 0b01100, 0b00100, 0b00100, 0b00100, 0b01110, 0b00010 } }, /* į */
    { 0xC5, 0xA1, 's', true,  { 0b01010, 0b00100, 0b01111, 0b10000, 0b01110, 0b00001, 0b11110, 0b00000 } }, /* š */
    { 0xC5, 0xB3, 'u', false, { 0b00000, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101, 0b00010, 0b00100 } }, /* ų */
    { 0xC5, 0xAB, 'u', true,  { 0b01110, 0b00000, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101, 0b00000 } }, /* ū */
    { 0xC5, 0xBE, 'z', true,  { 0b01010, 0b00100, 0b11111, 0b00010, 0b00100, 0b01000, 0b11111, 0b00000 } }, /* ž */

    /* uppercase: A, C, E, E-dot, I, S, U-ogonek, U-macron, Z */
    { 0xC4, 0x84, 'A', false, { 0b00100, 0b01010, 0b10001, 0b11111, 0b10001, 0b10001, 0b00010, 0b00100 } }, /* Ą */
    { 0xC4, 0x8C, 'C', true,  { 0b01010, 0b00100, 0b01110, 0b10001, 0b10000, 0b10001, 0b01110, 0b00000 } }, /* Č */
    { 0xC4, 0x98, 'E', false, { 0b11111, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111, 0b00010, 0b00100 } }, /* Ę */
    { 0xC4, 0x96, 'E', true,  { 0b00100, 0b11111, 0b10000, 0b11110, 0b10000, 0b11111, 0b00000, 0b00000 } }, /* Ė */
    { 0xC4, 0xAE, 'I', false, { 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110, 0b00010, 0b00100 } }, /* Į */
    { 0xC5, 0xA0, 'S', true,  { 0b01010, 0b00100, 0b01111, 0b10000, 0b01110, 0b00001, 0b11110, 0b00000 } }, /* Š */
    { 0xC5, 0xB2, 'U', false, { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110, 0b00010, 0b00100 } }, /* Ų */
    { 0xC5, 0xAA, 'U', true,  { 0b01110, 0b00000, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110, 0b00000 } }, /* Ū */
    { 0xC5, 0xBD, 'Z', true,  { 0b01010, 0b00100, 0b11111, 0b00010, 0b00100, 0b01000, 0b11111, 0b00000 } }, /* Ž */
};

static inline int lith_find_char(u8 first, u8 second)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(lithuanian_chars); i++) {
        if (lithuanian_chars[i].utf8_first == first &&
            lithuanian_chars[i].utf8_second == second)
            return i;
    }

    return -1;
}

#endif