/* compress_amg.cpp --

   This file is part of the UPX executable compressor.

   Copyright (C) 1996-2018 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1996-2018 Laszlo Molnar
   All Rights Reserved.

   UPX and the UCL library are free software; you can redistribute them
   and/or modify them under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Markus F.X.J. Oberhumer              Laszlo Molnar
   <markus@oberhumer.com>               <ezerotven+github@gmail.com>
 */


#include <iostream>
#include <random>
#include "conf.h"
#include "compress.h"
#if !(WITH_UCL)
extern int compress_amg_dummy;
int compress_amg_dummy = 0;
#else

#if 1 && !(UCL_USE_ASM) && (ACC_ARCH_I386)
#  if (ACC_CC_CLANG || ACC_CC_GNUC || ACC_CC_INTELC || ACC_CC_MSC || ACC_CC_WATCOMC)
#    define UCL_USE_ASM 1
#  endif
#endif
#if (UCL_NO_ASM)
#  undef UCL_USE_ASM
#endif
#if (ACC_CFG_NO_UNALIGNED)
#  undef UCL_USE_ASM
#endif
#if 1 && (UCL_USE_ASM)
#  include "../ucl/ucl_asm.h"
#  define ucl_nrv2b_decompress_safe_8       ucl_nrv2b_decompress_asm_safe_8
#  define ucl_nrv2b_decompress_safe_le16    ucl_nrv2b_decompress_asm_safe_le16
#  define ucl_nrv2b_decompress_safe_le32    ucl_nrv2b_decompress_asm_safe_le32
#  define ucl_nrv2d_decompress_safe_8       ucl_nrv2d_decompress_asm_safe_8
#  define ucl_nrv2d_decompress_safe_le16    ucl_nrv2d_decompress_asm_safe_le16
#  define ucl_nrv2d_decompress_safe_le32    ucl_nrv2d_decompress_asm_safe_le32
#  define ucl_nrv2e_decompress_safe_8       ucl_nrv2e_decompress_asm_safe_8
#  define ucl_nrv2e_decompress_safe_le16    ucl_nrv2e_decompress_asm_safe_le16
#  define ucl_nrv2e_decompress_safe_le32    ucl_nrv2e_decompress_asm_safe_le32
#endif


 /*************************************************************************
 //
 **************************************************************************/


#define N		 4096	/* size of ring buffer */
#define F		   18	/* upper limit for match_length */
#define THRESHOLD	2   /* encode string into position and length
 if match_length is greater than this */
#define NIL			N	/* index for root of binary search trees */

unsigned long int
textsize = 0,	/* text size counter */
codesize = 0,	/* code size counter */
printcount = 0;	/* counter for reporting progress every 1K bytes */
unsigned char enc_text_buf[N + F - 1];	/* ring buffer of size N,
										with extra F-1 bytes to facilitate string comparison */
int		match_position, match_length,  /* of longest match.  These are
									   set by the InsertNode() procedure. */
	lson[N + 1], rson[N + 257], dad[N + 1];  /* left & right children &
											 parents -- These constitute binary search trees. */

void InitTree(void)  /* initialize trees */
{
	int  i;

	/* For i = 0 to N - 1, rson[i] and lson[i] will be the right and
	left children of node i.  These nodes need not be initialized.
	Also, dad[i] is the parent of node i.  These are initialized to
	NIL (= N), which stands for 'not used.'
	For i = 0 to 255, rson[N + i + 1] is the root of the tree
	for strings that begin with character i.  These are initialized
	to NIL.  Note there are 256 trees. */

	for (i = N + 1; i <= N + 256; i++) rson[i] = NIL;
	for (i = 0; i < N; i++) dad[i] = NIL;
}

void InsertNode(int r)
/* Inserts string of length F, enc_text_buf[r..r+F-1], into one of the
trees (enc_text_buf[r]'th tree) and returns the longest-match position
and length via the global variables match_position and match_length.
If match_length = F, then removes the old node in favor of the new
one, because the old one will be deleted sooner.
Note r plays double role, as tree node and position in buffer. */
{
	int  i, p, cmp;
	unsigned char  *key;

	cmp = 1;  key = &enc_text_buf[r];  p = N + 1 + key[0];
	rson[r] = lson[r] = NIL;  match_length = 0;
	for (; ; ) {
		if (cmp >= 0) {
			if (rson[p] != NIL) p = rson[p];
			else { rson[p] = r;  dad[r] = p;  return; }
		}
		else {
			if (lson[p] != NIL) p = lson[p];
			else { lson[p] = r;  dad[r] = p;  return; }
		}
		for (i = 1; i < F; i++)
			if ((cmp = key[i] - enc_text_buf[p + i]) != 0)  break;
		if (i > match_length) {
			match_position = p;
			if ((match_length = i) >= F)  break;
		}
	}
	dad[r] = dad[p];  lson[r] = lson[p];  rson[r] = rson[p];
	dad[lson[p]] = r;  dad[rson[p]] = r;
	if (rson[dad[p]] == p) rson[dad[p]] = r;
	else                   lson[dad[p]] = r;
	dad[p] = NIL;  /* remove p */
}

void DeleteNode(int p)  /* deletes node p from tree */
{
	int  q;

	if (dad[p] == NIL) return;  /* not in tree */
	if (rson[p] == NIL) q = lson[p];
	else if (lson[p] == NIL) q = rson[p];
	else {
		q = lson[p];
		if (rson[q] != NIL) {
			do { q = rson[q]; } while (rson[q] != NIL);
			rson[dad[q]] = lson[q];  dad[lson[q]] = dad[q];
			lson[q] = lson[p];  dad[lson[p]] = q;
		}
		rson[q] = rson[p];  dad[rson[p]] = q;
	}
	dad[q] = dad[p];
	if (rson[dad[p]] == p) rson[dad[p]] = q;  else lson[dad[p]] = q;
	dad[p] = NIL;
}

void LzssEncode(char *in, int inlen, char *out, int *outlen)
{
	int  i, c, len, r, s, last_match_length, code_buf_ptr;
	unsigned char  code_buf[17], mask;

	int ii = 0; // index of in
	int oo = 0; // index of out

	InitTree();  /* initialize trees */
	code_buf[0] = 0;  /* code_buf[1..16] saves eight units of code, and
					  code_buf[0] works as eight flags, "1" representing that the unit
					  is an unencoded letter (1 byte), "0" a position-and-length pair
					  (2 bytes).  Thus, eight units require at most 16 bytes of code. */
	code_buf_ptr = mask = 1;
	s = 0;  r = N - F;
	for (i = s; i < r; i++) enc_text_buf[i] = ' ';  /* Clear the buffer with
													any character that will appear often. */
	for (len = 0; len < F && ii < inlen; len++)
	{
		c = in[ii++];

		enc_text_buf[r + len] = c;  /* Read F bytes into the last F bytes of
									the buffer */
	}

	if ((textsize = len) == 0)
	{
		*outlen = oo;
		return;  /* text of size zero */
	}
	for (i = 1; i <= F; i++) InsertNode(r - i);  /* Insert the F strings,
												 each of which begins with one or more 'space' characters.  Note
												 the order in which these strings are inserted.  This way,
												 degenerate trees will be less likely to occur. */
	InsertNode(r);  /* Finally, insert the whole string just read.  The
					global variables match_length and match_position are set. */
	do {
		if (match_length > len) match_length = len;  /* match_length
													 may be spuriously long near the end of text. */
		if (match_length <= THRESHOLD) {
			match_length = 1;  /* Not long enough match.  Send one byte. */
			code_buf[0] |= mask;  /* 'send one byte' flag */
			code_buf[code_buf_ptr++] = enc_text_buf[r];  /* Send uncoded. */
		}
		else {
			code_buf[code_buf_ptr++] = (unsigned char)match_position;
			code_buf[code_buf_ptr++] = (unsigned char)
				(((match_position >> 4) & 0xf0)
					| (match_length - (THRESHOLD + 1)));  /* Send position and
														  length pair. Note match_length > THRESHOLD. */
		}
		if ((mask <<= 1) == 0) {  /* Shift mask left one bit. */
			for (i = 0; i < code_buf_ptr; i++)  /* Send at most 8 units of */
				out[oo++] = code_buf[i];
			//putc(code_buf[i], outfile);     /* code together */
			codesize += code_buf_ptr;
			code_buf[0] = 0;  code_buf_ptr = mask = 1;
		}
		last_match_length = match_length;
		for (i = 0; i < last_match_length && ii < inlen; i++) {
			c = in[ii++];
			DeleteNode(s);		/* Delete old strings and */
			enc_text_buf[s] = c;	/* read new bytes */
			if (s < F - 1) enc_text_buf[s + N] = c;  /* If the position is
													 near the end of buffer, extend the buffer to make
													 string comparison easier. */
			s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
			/* Since this is a ring buffer, increment the position
			modulo N. */
			InsertNode(r);	/* Register the string in enc_text_buf[r..r+F-1] */
		}
		if ((textsize += i) > printcount) {
			printf("%12ld\r", textsize);  printcount += 1024;
			/* Reports progress each time the textsize exceeds
			multiples of 1024. */
		}
		while (i++ < last_match_length) {	/* After the end of text, */
			DeleteNode(s);					/* no need to read, but */
			s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
			if (--len) InsertNode(r);		/* buffer may not be empty. */
		}
	} while (len > 0);	/* until length of string to be processed is zero */
	if (code_buf_ptr > 1) {		/* Send remaining code. */
		for (i = 0; i < code_buf_ptr; i++)
			out[oo++] = code_buf[i]; //putc(code_buf[i], outfile);
		codesize += code_buf_ptr;
	}
	*outlen = oo;
}

void LzssDecode(char *in, int inlen, char *out, int *outlen)	/* Just the reverse of Encode(). */
{
	int  i, j, k, r, c;
	unsigned int  flags;

	int ii = 0; // index of in
	int oo = 0; // index of out
	unsigned char dec_text_buf[N + F - 1];

	for (i = 0; i < N - F; i++) dec_text_buf[i] = ' ';
	r = N - F;  flags = 0;
	for (; ; ) {
		if (((flags >>= 1) & 256) == 0) {
			if (ii >= inlen) break;
			c = in[ii++] & 0xFF;
			flags = c | 0xff00;		/* uses higher byte cleverly */
		}							/* to count eight */
		if (flags & 1) {
			if (ii >= inlen) break;
			c = in[ii++] & 0xFF;;
			out[oo++] = c;  dec_text_buf[r++] = c;  r &= (N - 1);
		}
		else {
			//if ((i = getc(infile)) == EOF) break;
			if (ii >= inlen) break;
			i = in[ii++] & 0xFF;
			//if ((j = getc(infile)) == EOF) break;
			if (ii >= inlen) break;
			j = in[ii++] & 0xFF;
			i |= ((j & 0xf0) << 4);  j = (j & 0x0f) + THRESHOLD;
			for (k = 0; k <= j; k++) {
				c = dec_text_buf[(i + k) & (N - 1)];
				out[oo++] = c;  dec_text_buf[r++] = c;  r &= (N - 1);
			}
		}
	}
	*outlen = oo;
}

void rc4_init(unsigned char *s, unsigned char *key, unsigned long Len) //初始化函数 s[256]
{
	int i = 0, j = 0;
	char k[256] = { 0 };
	unsigned char tmp = 0;
	for (i = 0; i<256; i++) {
		s[i] = i;
		k[i] = key[i%Len];
	}
	for (i = 0; i<256; i++) {
		j = (j + s[i] + k[i]) % 256;
		tmp = s[i];
		s[i] = s[j]; //交换s[i]和s[j]
		s[j] = tmp;
	}
}

void rc4_crypt(unsigned char *s, unsigned char *Data, unsigned long Len) //加解密
{
	int i = 0, j = 0, t = 0;
	unsigned long k = 0;
	unsigned char tmp;
	for (k = 0; k<Len; k++) {
		i = (i + 1) % 256;
		j = (j + s[i]) % 256;
		tmp = s[i];
		s[i] = s[j]; //交换s[x]和s[y]
		s[j] = tmp;
		t = (s[i] + s[j]) % 256;
		Data[k] ^= s[t];
	}
}

void DecrypteAndDecompress(char *in, int inlen, char *out, int *outlen)
{
	unsigned char s[256];

	rc4_init(s, (unsigned char*)in, 8);
	rc4_crypt(s, (unsigned char*)in + 8, inlen - 8);
	LzssDecode(in + 8, inlen - 8, out, outlen);
}

void CompressAndEncrypt(char *in, int inlen, char *out, int *outlen)
{
	unsigned char s[256];
	//unsigned char key[8];

	std::random_device rd;
	std::mt19937 mt(rd());

	//8 bytes key
	for (int i = 0; i < 8; ++i)
	{
		out[i] = mt();
	}

	LzssEncode(in, inlen, out + 8, outlen);
	rc4_init(s, (unsigned char*)out, 8);
	rc4_crypt(s, (unsigned char*)out + 8, *outlen);
	*outlen += 8;
}

/*************************************************************************
//
**************************************************************************/


//extern "C" {
//static void __UCL_CDECL wrap_nprogress_ucl(ucl_uint a, ucl_uint b, int state, ucl_voidp user)
//{
//    if (state != -1 && state != 3) return;
//    upx_callback_p cb = (upx_callback_p) user;
//    if (cb && cb->nprogress)
//        cb->nprogress(cb, a, b);
//}}


/*************************************************************************
//
**************************************************************************/

int upx_amg_compress       ( const upx_bytep src, unsigned  src_len,
                                   upx_bytep dst, unsigned* dst_len,
                                   upx_callback_p cb_parm,
                                   int method, int level,
                             const upx_compress_config_t *cconf_parm,
                                   upx_compress_result_t *cresult )
{
    assert(level > 0); assert(cresult != NULL);

    COMPILE_TIME_ASSERT(sizeof(ucl_compress_config_t) == sizeof(REAL_ucl_compress_config_t))

    ucl_progress_callback_t cb;
    cb.callback = 0;
    cb.user = NULL;
    //if (cb_parm && cb_parm->nprogress) {
    //    cb.callback = wrap_nprogress_ucl;
    //    cb.user = cb_parm;
    //}

    ucl_compress_config_t cconf; cconf.reset();
    if (cconf_parm)
        memcpy(&cconf, &cconf_parm->conf_ucl, sizeof(cconf)); // cconf = cconf_parm->conf_ucl; // struct copy

    ucl_uint *res = cresult->result_ucl.result;
    // assume no info available - fill in worst case results
    //res[0] = 1;                 // min_offset_found - NOT USED
    res[1] = src_len - 1;       // max_offset_found
    //res[2] = 2;                 // min_match_found - NOT USED
    res[3] = src_len - 1;       // max_match_found
    //res[4] = 1;                 // min_run_found - NOT USED
    res[5] = src_len;           // max_run_found
    res[6] = 1;                 // first_offset_found
    //res[7] = 999999;            // same_match_offsets_found - NOT USED

    // prepare bit-buffer settings
    cconf.bb_endian = 0;
    cconf.bb_size = 0;
    if (method >= M_NRV2B_LE32 && method <= M_NRV2E_LE16)
    {
        static const unsigned char sizes[3] = {32, 8, 16};
        cconf.bb_size = sizes[(method - M_NRV2B_LE32) % 3];
    }
    else {
        throwInternalError("unknown compression method");
        return UPX_E_ERROR;
    }

    // optimize compression parms
    if (level <= 3 && cconf.max_offset == UCL_UINT_MAX)
        cconf.max_offset = 8*1024-1;
    else if (level == 4 && cconf.max_offset == UCL_UINT_MAX)
        cconf.max_offset = 32*1024-1;

	// 1. copy
	//*(int*)dst = src_len;
	//memcpy(dst + sizeof(int), src, src_len);
	//*dst_len = sizeof(int) + src_len;

	// 2. LZSS
	//LzssEncode((char*)src, src_len, (char*)dst + sizeof(int), (int*)dst_len);
	//*(int*)dst = *dst_len;
	//*dst_len += sizeof(int);


	// 3. LZSS and RC4
	// can't alter src
	char *tmp = new char[src_len];
	memcpy(tmp, src, src_len);
	CompressAndEncrypt((char*)tmp, src_len, (char*)dst + sizeof(int), (int*)dst_len);
	*(int*)dst = *dst_len;
	*dst_len += sizeof(int);
	free(tmp);


    // make sure first_offset_found is set
    if (res[6] == 0)
        res[6] = 1;

    return UPX_E_OK;
}


/*************************************************************************
//
**************************************************************************/

int upx_amg_decompress     ( const upx_bytep src, unsigned  src_len,
                                   upx_bytep dst, unsigned* dst_len,
                                   int method,
                             const upx_compress_result_t *cresult )
{
	// 1. copy
	//memcpy(dst, src + sizeof(int), src_len - sizeof(int));
	//*dst_len = src_len - sizeof(int);

	// 2. LZSS
	//LzssDecode((char*)src + sizeof(int), src_len - sizeof(int), (char*)dst, (int*)dst_len);

	// 3. LZSS and RC4
	// can't alter src
	char *tmp = new char[src_len];
	memcpy(tmp, src, src_len);
	DecrypteAndDecompress((char*)tmp + sizeof(int), src_len - sizeof(int), (char*)dst , (int*)dst_len);
	free(tmp);

    UNUSED(cresult);
    return UPX_E_OK;
}


/*************************************************************************
//
**************************************************************************/

int upx_amg_test_overlap   ( const upx_bytep buf,
                             const upx_bytep tbuf,
                                   unsigned  src_off, unsigned src_len,
                                   unsigned* dst_len,
                                   int method,
                             const upx_compress_result_t *cresult )
{
    return UPX_E_OK;
}


#endif /* WITH_UCL */

/* vim:set ts=4 sw=4 et: */
