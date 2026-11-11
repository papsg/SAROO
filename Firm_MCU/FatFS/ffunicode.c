/*------------------------------------------------------------------------*/
/* Unicode Handling Functions for FatFs R0.13 and Later                   */
/*------------------------------------------------------------------------*/
/* This module will occupy a huge memory in the .rodata section when the  */
/* FatFs is configured for LFN with DBCS. If the system has a Unicode     */
/* library for the code conversion, this module should be modified to use */
/* it to avoid silly memory consumption.                                  */
/*------------------------------------------------------------------------*/
/*
/ Copyright (C) 2022, ChaN, all right reserved.
/
/ FatFs module is an open source software. Redistribution and use of FatFs in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
*/


#include "ff.h"

#if FF_USE_LFN != 0	/* This module will be blanked if in non-LFN configuration */

#define MERGE2(a, b) a ## b
#define CVTBL(tbl, cp) MERGE2(tbl, cp)


/*------------------------------------------------------------------------*/
/* Code Conversion Tables                                                 */
/*------------------------------------------------------------------------*/

#if FF_CODE_PAGE == 437 || FF_CODE_PAGE == 0
static const WCHAR uc437[] = {	/*  CP437(U.S.) to Unicode conversion table */
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
	0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
	0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};
#endif


/*------------------------------------------------------------------------*/
/* OEM <==> Unicode Conversions for Static Code Page Configuration with   */
/* SBCS Fixed Code Page                                                   */
/*------------------------------------------------------------------------*/

#if FF_CODE_PAGE != 0 && FF_CODE_PAGE < 900
WCHAR ff_uni2oem (	/* Returns OEM code character, zero on error */
	DWORD	uni,	/* UTF-16 encoded character to be converted */
	WORD	cp		/* Code page for the conversion */
)
{
	WCHAR c = 0;
	const WCHAR* p = CVTBL(uc, FF_CODE_PAGE);


	if (uni < 0x80) {	/* ASCII? */
		c = (WCHAR)uni;

	} else {			/* Non-ASCII */
		if (uni < 0x10000 && cp == FF_CODE_PAGE) {	/* Is it in BMP and valid code page? */
			for (c = 0; c < 0x80 && uni != p[c]; c++) ;
			c = (c + 0x80) & 0xFF;
		}
	}

	return c;
}

WCHAR ff_oem2uni (	/* Returns Unicode character in UTF-16, zero on error */
	WCHAR	oem,	/* OEM code to be converted */
	WORD	cp		/* Code page for the conversion */
)
{
	WCHAR c = 0;
	const WCHAR* p = CVTBL(uc, FF_CODE_PAGE);


	if (oem < 0x80) {	/* ASCII? */
		c = oem;

	} else {			/* Extended char */
		if (cp == FF_CODE_PAGE) {	/* Is it a valid code page? */
			if (oem < 0x100) c = p[oem - 0x80];
		}
	}

	return c;
}

#endif



/*------------------------------------------------------------------------*/
/* Unicode Up-case Conversion                                             */
/*------------------------------------------------------------------------*/

/* Exact pinned FatFs case mapping, represented as sorted sparse ranges.
 * A low descriptor is a singleton target. A high descriptor contains a
 * span-minus-one and a delta selector; selector zero denotes case pairs.
 * Generated from the pinned tables, with all BMP values checked exactly. */
static const struct { WORD start, conversion; } uppercase_ranges[] = {
    {0x0101, 0x9780},
    {0x0133, 0x8280},
    {0x013a, 0x8780},
    {0x014b, 0x9680},
    {0x017a, 0x8280},
    {0x0180, 0x0243},
    {0x0183, 0x8180},
    {0x0188, 0x8080},
    {0x018c, 0x8080},
    {0x0192, 0x8080},
    {0x0195, 0x01f6},
    {0x0199, 0x0198},
    {0x019a, 0x023d},
    {0x019e, 0x0220},
    {0x01a1, 0x8280},
    {0x01a8, 0x8080},
    {0x01ad, 0x8080},
    {0x01b0, 0x8080},
    {0x01b4, 0x8180},
    {0x01b9, 0x8080},
    {0x01bd, 0x8080},
    {0x01bf, 0x01f7},
    {0x01c6, 0x01c4},
    {0x01c9, 0x01c7},
    {0x01cc, 0x01ca},
    {0x01ce, 0x8700},
    {0x01dd, 0x018e},
    {0x01df, 0x8880},
    {0x01f3, 0x01f1},
    {0x01f5, 0x8080},
    {0x01f9, 0x9380},
    {0x0223, 0x8880},
    {0x023a, 0x2c65},
    {0x023c, 0x8080},
    {0x023e, 0x2c66},
    {0x0242, 0x8080},
    {0x0247, 0x8480},
    {0x0253, 0x0181},
    {0x0254, 0x0186},
    {0x0256, 0x8083},
    {0x0259, 0x018f},
    {0x025b, 0x0190},
    {0x0260, 0x0193},
    {0x0263, 0x0194},
    {0x0268, 0x0197},
    {0x0269, 0x0196},
    {0x026b, 0x2c62},
    {0x026f, 0x019c},
    {0x0272, 0x019d},
    {0x0275, 0x019f},
    {0x027d, 0x2c64},
    {0x0280, 0x01a6},
    {0x0283, 0x01a9},
    {0x0288, 0x01ae},
    {0x0289, 0x0244},
    {0x028a, 0x8082},
    {0x028c, 0x0245},
    {0x0292, 0x01b7},
    {0x037b, 0x8112},
    {0x03ac, 0x0386},
    {0x03ad, 0x8107},
    {0x03b1, 0x8808},
    {0x03c2, 0x03a3},
    {0x03c3, 0x8408},
    {0x03cc, 0x038c},
    {0x03cd, 0x8085},
    {0x03d9, 0x8b80},
    {0x03f2, 0x03f9},
    {0x03f8, 0x8080},
    {0x03fb, 0x8080},
    {0x0430, 0x8f88},
    {0x0450, 0x8784},
    {0x0461, 0x9080},
    {0x048b, 0x9a80},
    {0x04c2, 0x8600},
    {0x04cf, 0x04c0},
    {0x04d1, 0xa180},
    {0x0561, 0x9286},
    {0x1d7d, 0x2c63},
    {0x1e01, 0xca80},
    {0x1ea1, 0xac80},
    {0x1f00, 0x838b},
    {0x1f10, 0x828b},
    {0x1f20, 0x838b},
    {0x1f30, 0x838b},
    {0x1f40, 0x828b},
    {0x1f51, 0x1f59},
    {0x1f53, 0x1f5b},
    {0x1f55, 0x1f5d},
    {0x1f57, 0x1f5f},
    {0x1f60, 0x838b},
    {0x1f70, 0x808c},
    {0x1f72, 0x818d},
    {0x1f76, 0x808e},
    {0x1f78, 0x8091},
    {0x1f7a, 0x808f},
    {0x1f7c, 0x8090},
    {0x1f80, 0x838b},
    {0x1f90, 0x838b},
    {0x1fa0, 0x838b},
    {0x1fb0, 0x808b},
    {0x1fb3, 0x1fbc},
    {0x1fcc, 0x1fc3},
    {0x1fd0, 0x808b},
    {0x1fe0, 0x808b},
    {0x1fe5, 0x1fec},
    {0x1ff3, 0x1ffc},
    {0x214e, 0x2132},
    {0x2170, 0x878a},
    {0x2184, 0x8080},
    {0x24d0, 0x8c89},
    {0x2c30, 0x9706},
    {0x2c61, 0x8080},
    {0x2c68, 0x8280},
    {0x2c76, 0x8080},
    {0x2c81, 0xb180},
    {0x2d00, 0x9281},
    {0xff41, 0x8c88},
};
static const short uppercase_deltas[] = {-7264, -217, -205, -80, -63, -48, -37, -32, -26, -16, 8, 74, 86, 100, 112, 126, 128, 130};
DWORD ff_wtoupper(DWORD uni)
{
    /* Basic Latin and Latin-1 never need a table lookup. */
    if (uni < 0x100) {
        if (uni == 0xff) return 0x178;
        if ((uni - 'a' < 26) || (uni >= 0xe0 && uni != 0xf7)) return uni - 32;
        return uni;
    }
    if (uni >= 0x10000) return uni;
    unsigned low = 0, high = sizeof uppercase_ranges / sizeof *uppercase_ranges;
    while (low < high) {
        unsigned middle = low + (high - low) / 2;
        if (uppercase_ranges[middle].start <= uni) low = middle + 1;
        else high = middle;
    }
    if (!low) return uni;
    unsigned start = uppercase_ranges[low - 1].start;
    unsigned conversion = uppercase_ranges[low - 1].conversion;
    unsigned offset = uni - start;
    if (!(conversion & 0x8000)) return offset ? uni : conversion;
    if (offset > ((conversion >> 7) & 255)) return uni;
    unsigned rule = conversion & 127;
    return rule ? uni + uppercase_deltas[rule - 1] : uni - ((offset + 1) & 1);
}


#endif /* #if FF_USE_LFN != 0 */
