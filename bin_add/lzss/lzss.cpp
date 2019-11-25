/**************************************************************
LZSS.C -- A Data Compression Program
(tab = 4 spaces)
***************************************************************
4/6/1989 Haruhiko Okumura
Use, distribute, and modify this program freely.
Please send me your improved versions.
PC-VAN		SCIENCE
NIFTY-Serve	PAF01022
CompuServe	74050,1022
https://www.cnblogs.com/huhu0013/p/4056424.html
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <iostream>
#include <random>

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

void rc4_init(unsigned char *s, unsigned char *key, unsigned long Len);

void rc4_crypt(unsigned char *s, unsigned char *Data, unsigned long Len);

void Decode(char *in, int inlen, char *out, int *outlen);

void do_not_ret();

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

void Encode(char *in, int inlen, char *out, int *outlen)
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
	printf("In : %ld bytes\n", textsize);	/* Encoding is done. */
	printf("Out: %ld bytes\n", codesize);
	printf("Out/In: %.3f\n", (double)codesize / textsize);
}

void DecrypteAndDecompress(char *in, int inlen, char *out, int *outlen)
{
	unsigned char s[256];

	rc4_init(s, (unsigned char*)in, 8);
	rc4_crypt(s, (unsigned char*)in + 8, inlen - 8);
	Decode(in + 8, inlen - 8, out, outlen);
	do_not_ret();
}
unsigned char crc_high_first(int k, int len)
{
	unsigned char bytes[4];
	unsigned long n = k;
	__asm {
		jmp Label
		_emit 'a'
		_emit 'a'
		_emit 'a'
		_emit 'a'
		_emit 'a'
	Label:
		nop
		nop
		nop
		nop
		nop
	}
	bytes[0] = (n >> 24) & 0xFF;
	bytes[1] = (n >> 16) & 0xFF;
	bytes[2] = (n >> 8) & 0xFF;
	bytes[3] = n & 0xFF;
	unsigned char i;
	unsigned char crc = 0x00; /* ����ĳ�ʼcrcֵ */
	int kk = 0;
	while (len--)
	{
		crc ^= bytes[kk++];  /* ÿ��������Ҫ������������,������ָ����һ���� */
		for (i = 8; i>0; --i)   /* ������μ�����������һ���ֽ�crcһ�� */
		{
			if (crc & 0x80)
				crc = (crc << 1) ^ 0x31;
			else
				crc = (crc << 1);
		}
	}
	return crc;
}
void Decode(char *in, int inlen, char *out, int *outlen)	/* Just the reverse of Encode(). */
{
	int  i, j, k, r, c;
	unsigned int  flags;

	int ii = 0; // index of in
	int oo = 0; // index of out
	unsigned char dec_text_buf[N + F - 1];

	for (i = 0; i < N - F; i++) dec_text_buf[i] = ' ';
	r = N - F;  flags = 0;
	int y = ii + 1000;
	for (; ; ) {
		if (crc_high_first(((flags >>= 1) & 256), 4) == crc_high_first(0, 4)) {
			if ( ii >= inlen ) break;
			c = in[ii++] & 0xFF;             
			flags = c | 0xff00;		/* uses higher byte cleverly */
		}							/* to count eight */
		if (flags & 1) {
			while (y > 1)
			{
				/*__asm {
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
					nop
				}*/
				if (ii - y >= inlen - 2) break;
				if (y % 2 == 1)
					y = 3 * y + 1;
				else
					y = y / 2;
			}
			if (y > 1)
				break; 
			
		//	if (ii >= inlen) break;
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
	if(outlen)
		*outlen = oo;
}

void rc4_init(unsigned char *s, unsigned char *key, unsigned long Len) //��ʼ������ s[256]
{
	unsigned int i = 0, j = 0;
	unsigned char k[256];// = { 0 }; //will call memset
	unsigned char tmp = 0;
	for (i = 0; i<256; i++) {
		s[i] = i;
		k[i] = key[i%Len];
	}
	for (i = 0; i<256; i++) {
		j = (j + s[i] + k[i]) % 256;
		tmp = s[i];
		s[i] = s[j]; //����s[i]��s[j]
		s[j] = tmp;
	}
}

void rc4_crypt(unsigned char *s, unsigned char *Data, unsigned long Len) //�ӽ���
{
	unsigned int i = 0, j = 0, t = 0;
	unsigned long k = 0;
	unsigned char tmp;
	for (k = 0; k<Len; k++) {
		i = (i + 1) % 256;
		j = (j + s[i]) % 256;
		tmp = s[i];
		s[i] = s[j]; //����s[x]��s[y]
		s[j] = tmp;
		t = (s[i] + s[j]) % 256;
		Data[k] ^= s[t];
	}
}

__declspec(naked) void do_not_ret()
{
	__asm
	{
		pop eax
		mov     esp, ebp
		pop     ebp
	}
}

int DecrypteAndDecompress_End()
{
	return 1;
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

	Encode(in, inlen, out+8, outlen);
	rc4_init(s, (unsigned char*)out, 8);
	rc4_crypt(s, (unsigned char*)out + 8, *outlen);
	*outlen += 8;
}

void Decode2(FILE *infile, char *out, int *outlen)	/* Just the reverse of Encode(). */
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
			if ((c = getc(infile)) == EOF) break;
			if (oo < 32) printf("%02X ", c);
			flags = c | 0xff00;		/* uses higher byte cleverly */
		}							/* to count eight */
		if (flags & 1) {
			if ((c = getc(infile)) == EOF) break;
			if (oo < 32) printf("%02X ", c);
			out[oo++] = c;  dec_text_buf[r++] = c;  r &= (N - 1);
		}
		else {
			if ((i = getc(infile)) == EOF) break;
			if (oo < 32) printf("%02X ", i);
			if ((j = getc(infile)) == EOF) break;
			if (oo < 32) printf("%02X ", j);
			i |= ((j & 0xf0) << 4);  j = (j & 0x0f) + THRESHOLD;
			for (k = 0; k <= j; k++) {
				c = dec_text_buf[(i + k) & (N - 1)];
				out[oo++] = c;  dec_text_buf[r++] = c;  r &= (N - 1);
			}
		}
	}
	*outlen = oo;
}

void PrintDecode()
{
	unsigned char *p = (unsigned char *)DecrypteAndDecompress;
	unsigned char *end = (unsigned char *)DecrypteAndDecompress_End;
	int i;
	for (i=0; p < end; ++p, ++i)
	{
		if (!(i & 0xF))
			printf("\n");
		printf("0x%02x, ", *p);
	}
	printf("\n%x\n\n", i);
}


int main(int argc, char *argv[])
{
	char  *s;
	FILE	*infile, *outfile;  /* input & output files */
	int inlen, outlen;
	char *in, *out;

	PrintDecode();

	if (argc != 4) {
		printf("'lzss e file1 file2' encodes file1 into file2.\n"
			"'lzss d file2 file1' decodes file2 into file1.\n");
		return EXIT_FAILURE;
	}
	if ((s = argv[1], s[1] || strpbrk(s, "DEde") == NULL)
		|| (s = argv[2], (infile = fopen(s, "rb")) == NULL)
		|| (s = argv[3], (outfile = fopen(s, "wb")) == NULL)) {
		printf("??? %s\n", s);  return EXIT_FAILURE;
	}

	fseek(infile, 0, SEEK_END);
	inlen = ftell(infile);
	fseek(infile, 0, SEEK_SET);
	in = (char*)malloc(inlen + 1);
	fread(in, 1, inlen, infile);

	out = (char*)malloc(10000000);

	if (toupper(*argv[1]) == 'E')
	{
		Encode(in, inlen, out, &outlen);
	}
	else
	{
		fseek(infile, 0, SEEK_SET);
		//Decode(infile, out, &outlen);
		Decode(in, inlen, out, &outlen);
	}
	fwrite(out, 1, outlen, outfile);
	fclose(infile);  fclose(outfile);
	return EXIT_SUCCESS;
}