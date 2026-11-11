
/* reed_solomon library, come from linux kernel */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef unsigned char u8;


/******************************************************************************/
// 参数: GF(2^6), PX = X^6+X+1


#define mm  6           /* RS code over GF(2**mm) - change to suit */
#define nn  ((1<<mm)-1) /* nn=2**mm -1   length of codeword */
#define gfpoly 0x43
#define prim 1
#define iprim 1
#define fcr 0


static u8 alpha_to[nn+1];
static u8 index_of[nn+1];


typedef struct {
	int nroots;
	u8  gpoly[8];
}RSCTRL;

RSCTRL rs20, rs2;


static inline int rs_modnn(int x)
{
    while (x >= nn) {
        x -= nn;
        x = (x >> mm) + (x & nn);
    }
    return x;
}


void init_gpoly(RSCTRL *rs, int tt)
{
	u8 *genpoly = rs->gpoly;
	int nroots = tt*2;
	int i, j, root;

	rs->nroots = nroots;

	genpoly[0] = 1;
	for(i=0, root=fcr*prim; i<nroots; i++, root+=prim){
		genpoly[i+1] = 1;
		for(j=i; j>0; j--){
			if(genpoly[j]!=0){
				genpoly[j] = genpoly[j-1] ^ alpha_to[ rs_modnn(index_of[genpoly[j]] +root) ];
			}else{
				genpoly[j] = genpoly[j-1];
			}
		}
		genpoly[0] = alpha_to[ rs_modnn(index_of[genpoly[0]] + root) ];
	}

	for(i=0; i<nroots+1; i++){
		genpoly[i] = index_of[genpoly[i]];
	}
}


void init_rs(void)
{
	int i, sr;

	sr = 1;
	index_of[0] = nn;
	alpha_to[nn] = 0;
	for(i=0; i<nn; i++){
		index_of[sr] = i;
		alpha_to[i] = sr;
		sr <<= 1;
		if(sr & (1<<mm))
			sr ^= gfpoly;
		sr &= nn;
	}

	init_gpoly(&rs20, 2);
	init_gpoly(&rs2, 1);
}


/******************************************************************************/


void encode_rs(RSCTRL *rs, u8 *data, int len, u8 *par)
{
	u8 *genpoly = rs->gpoly;
	int nroots = rs->nroots;
	int i,j;

	memset(par, 0, nroots);

	for (i=0; i<len; i++) {
		u8 fb = index_of[(data[i]&nn) ^ par[0]];
		if(fb != nn) {
			for (j=1; j<nroots; j++)
				par[j] ^= alpha_to[ rs_modnn( fb + genpoly[nroots-j] ) ];
		}
		memmove(par, par+1, nroots-1);
		if(fb != nn) {
			par[nroots-1] = alpha_to[ rs_modnn( fb + genpoly[0] ) ];
		}else{
			par[nroots-1] = 0;
		}
	}
}


int decode_rs(RSCTRL *rs, u8 *data, int len, u8 *par)
{
	int nroots = rs->nroots;
	int deg_lambda, el, deg_omega;
	int i, j, r, k, pad, syn_error;
	u8  q, tmp, num1, num2, den, discr_r;
	int count = 0;
	int num_corrected;

	u8 lambda[nroots+1];
	u8 s[nroots+1];
	u8 b[nroots+1];
	u8 t[nroots+1];
	u8 omega[nroots+1];
	u8 root[nroots+1];
	u8 reg[nroots+1];
	u8 loc[nroots+1];

	pad = nn-nroots-len;

	/* form the syndromes; i.e., evaluate data(x) at roots of g(x) */
	for (i = 0; i < nroots; i++)
		s[i] = data[0] & nn;
	for (j = 1; j < len; j++) {
		for (i = 0; i < nroots; i++) {
			if (s[i] == 0) {
				s[i] = data[j] & nn;
			} else {
				s[i] = (data[j]&nn) ^ alpha_to[rs_modnn(index_of[s[i]] + (fcr+i)*prim)];
			}
		}
	}
	for (j = 0; j < nroots; j++) {
		for (i = 0; i < nroots; i++) {
			if (s[i] == 0) {
				s[i] = par[j] & nn;
			} else {
				s[i] = (par[j]&nn) ^ alpha_to[rs_modnn(index_of[s[i]] + (fcr+i)*prim)];
			}
		}
	}

	/* Convert syndromes to index form, checking for nonzero condition */
	syn_error = 0;
	for (i = 0; i < nroots; i++) {
		syn_error |= s[i];
		s[i] = index_of[s[i]];
	}
	if (!syn_error) {
		return 0;
	}



	memset(&lambda[1], 0, nroots * sizeof(lambda[0]));
	lambda[0] = 1;

	for (i = 0; i < nroots + 1; i++)
		b[i] = index_of[lambda[i]];

	/* Begin Berlekamp-Massey algorithm to determine error+erasure locator polynomial */
	r = 0;
	el = 0;
	while (++r <= nroots) {	/* r is the step number */
		/* Compute discrepancy at the r-th step in poly-form */
		discr_r = 0;
		for (i = 0; i < r; i++) {
			if ((lambda[i] != 0) && (s[r - i - 1] != nn)) {
				discr_r ^= alpha_to[rs_modnn(index_of[lambda[i]] + s[r - i - 1])];
			}
		}
		discr_r = index_of[discr_r];	/* Index form */
		if (discr_r == nn) {
			/* 2 lines below: B(x) <-- x*B(x) */
			memmove (&b[1], b, nroots * sizeof (b[0]));
			b[0] = nn;
		} else {
			/* 7 lines below: T(x) <-- lambda(x)-discr_r*x*b(x) */
			t[0] = lambda[0];
			for (i = 0; i < nroots; i++) {
				if (b[i] != nn) {
					t[i + 1] = lambda[i + 1] ^ alpha_to[rs_modnn(discr_r + b[i])];
				} else
					t[i + 1] = lambda[i + 1];
			}
			if (2 * el <= r - 1) {
				el = r - el;
				/* 2 lines below: B(x) <-- inv(discr_r) * lambda(x) */
				for (i = 0; i <= nroots; i++) {
					b[i] = (lambda[i] == 0) ? nn : rs_modnn(index_of[lambda[i]] - discr_r + nn);
				}
			} else {
				/* 2 lines below: B(x) <-- x*B(x) */
				memmove(&b[1], b, nroots * sizeof(b[0]));
				b[0] = nn;
			}
			memcpy(lambda, t, (nroots + 1) * sizeof(t[0]));
		}
	}

	/* Convert lambda to index form and compute deg(lambda(x)) */
	deg_lambda = 0;
	for (i = 0; i < nroots + 1; i++) {
		lambda[i] = index_of[lambda[i]];
		if (lambda[i] != nn)
			deg_lambda = i;
	}
	if (deg_lambda == 0) {
		/* deg(lambda) is zero even though the syndrome is non-zero => uncorrectable error detected */
		return -1;
	}

	/* Find roots of error+erasure locator polynomial by Chien search */
	memcpy(&reg[1], &lambda[1], nroots * sizeof(reg[0]));
	count = 0;		/* Number of roots of lambda(x) */
	for (i = 1, k = iprim - 1; i <= nn; i++, k = rs_modnn(k + iprim)) {
		q = 1;		/* lambda[0] is always 0 */
		for (j = deg_lambda; j > 0; j--) {
			if (reg[j] != nn) {
				reg[j] = rs_modnn(reg[j] + j);
				q ^= alpha_to[reg[j]];
			}
		}
		if (q != 0)
			continue;	/* Not a root */

		if (k < pad) {
			/* Impossible error location. Uncorrectable error. */
			return -2;
		}

		/* store root (index-form) and error location number */
		root[count] = i;
		loc[count] = k;
		/* If we've already found max possible roots, abort the search to save time */
		if (++count == deg_lambda)
			break;
	}
	if (deg_lambda != count) {
		/* deg(lambda) unequal to number of roots => uncorrectable error detected */
		return -3;
	}
	/* Compute err+eras evaluator poly omega(x) = s(x)*lambda(x) (modulo x**nroots). in index form. Also find deg(omega). */
	deg_omega = deg_lambda - 1;
	for (i = 0; i <= deg_omega; i++) {
		tmp = 0;
		for (j = i; j >= 0; j--) {
			if ((s[i - j] != nn) && (lambda[j] != nn))
				tmp ^= alpha_to[rs_modnn(s[i - j] + lambda[j])];
		}
		omega[i] = index_of[tmp];
	}

	/*
	 * Compute error values in poly-form. num1 = omega(inv(X(l))), num2 =
	 * inv(X(l))**(fcr-1) and den = lambda_pr(inv(X(l))) all in poly-form
	 * Note: we reuse the buffer for b to store the correction pattern
	 */
	num_corrected = 0;
	for (j = count - 1; j >= 0; j--) {
		num1 = 0;
		for (i = deg_omega; i >= 0; i--) {
			if (omega[i] != nn)
				num1 ^= alpha_to[rs_modnn(omega[i] + i * root[j])];
		}

		if (num1 == 0) {
			/* Nothing to correct at this position */
			b[j] = 0;
			continue;
		}

		num2 = alpha_to[rs_modnn(root[j] * (fcr - 1) + nn)];
		den = 0;

		/* lambda[i+1] for i even is the formal derivative lambda_pr of lambda[i] */
		int tmp = (deg_lambda<(nroots-1))? deg_lambda : (nroots-1);
		for (i = tmp & ~1; i >= 0; i -= 2) {
			if (lambda[i + 1] != nn) {
				den ^= alpha_to[rs_modnn(lambda[i + 1] + i * root[j])];
			}
		}

		b[j] = alpha_to[rs_modnn(index_of[num1] + index_of[num2] + nn - index_of[den])];
		num_corrected++;
	}

	/* We compute the syndrome of the 'error' and check that it matches the syndrome of the received word */
	for (i = 0; i < nroots; i++) {
		tmp = 0;
		for (j = 0; j < count; j++) {
			if (b[j] == 0)
				continue;

			k = (fcr + i) * prim * (nn-loc[j]-1);
			tmp ^= alpha_to[rs_modnn(index_of[b[j]] + k)];
		}

		if (tmp != alpha_to[s[i]])
			return -4;
	}

	if (data && par) {
		/* Apply error to data and parity */
		for (i = 0; i < count; i++) {
			if (loc[i] < (nn - nroots))
				data[loc[i] - pad] ^= b[i];
			else
				par[loc[i] - pad - len] ^= b[i];
		}
	}

	return  num_corrected;
}

/******************************************************************************/




void hex_dump(char *str, const void *buf, int size)
{
	int i, j;
	const uint8_t *ubuf = buf;

	if(str)
		printf("%s:\n", str);

	for(i=0; i<size; i+=16){
		printf("%4x: ", i);
		for(j=0; j<16; j++){
			if((i+j)<size){
				printf(" %02x", ubuf[j]);
			}else{
				printf("   ");
			}
		}
		printf("  ");
		for(j=0; j<16&&(i+j)<size; j++){
			if(ubuf[j]>=0x20 && ubuf[j]<=0x7f){
				printf("%c", ubuf[j]);
			}else{
				printf(".");
			}
		}
		printf("\n");
		ubuf += 16;
	}
	printf("\n");
}


/******************************************************************************/


enum { FRAME_BYTES = 96, PACK_BYTES = 24, PACK_SPAN = 192 };

static const u8 interleave[PACK_BYTES] = {
    0x00, 0x42, 0x7d, 0xbf, 0x64, 0x32, 0x96, 0xaf,
    0x08, 0x21, 0x3a, 0x53, 0x6c, 0x85, 0x9e, 0xb7,
    0x10, 0x29, 0x19, 0x5b, 0x74, 0x8d, 0xa6, 0x4b
};

/* Each symbol occupies one bit in each R-W channel. */
static u8 raw_symbol(const u8 *raw, size_t index)
{
    size_t offset = index / FRAME_BYTES * FRAME_BYTES + index % FRAME_BYTES / 8;
    unsigned shift = 7 - index % 8, value = 0;
    for (unsigned channel = 2; channel < 8; ++channel)
        value = (value << 1) | ((raw[offset + channel * 12] >> shift) & 1);
    return (u8)value;
}

static void set_raw_symbol(u8 *raw, size_t index, u8 value)
{
    size_t offset = index / FRAME_BYTES * FRAME_BYTES + index % FRAME_BYTES / 8;
    unsigned shift = 7 - index % 8, mask = 1u << shift;
    for (unsigned channel = 2; channel < 8; ++channel) {
        u8 *byte = raw + offset + channel * 12;
        *byte = (*byte & ~mask) | (((value >> (7 - channel)) & 1u) << shift);
    }
}


/* P also protects the header covered by Q. Commit only a complete repair. */
static int correct_packet(u8 packet[PACK_BYTES])
{
    u8 candidate[PACK_BYTES], parity[4];
    memcpy(candidate, packet, sizeof(candidate));
    if (decode_rs(&rs20, candidate, 20, candidate + 20) < 0)
        return -1;
    int q = decode_rs(&rs2, candidate, 2, candidate + 2);
    if (q < 0)
        return -1;
    if (q) {
        encode_rs(&rs20, candidate, 20, parity);
        for (unsigned i = 0; i < sizeof(parity); ++i)
            candidate[20 + i] = (candidate[20 + i] & 0xc0) | parity[i];
    }
    if (!memcmp(candidate, packet, sizeof(candidate)))
        return 0;
    memcpy(packet, candidate, sizeof(candidate));
    return 1;
}


int main(int argc, char *argv[])
{
	FILE *fp;
	u8 *fbuf;
	int flen;

	init_rs();


	fp = fopen(argv[1], "rb");
	if(fp==NULL)
		return -1;

	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if(flen <= 0 || !(fbuf = malloc(flen))){
		fclose(fp);
		return 1;
	}
	if(fread(fbuf, 1, flen, fp) != (size_t)flen){
		fclose(fp);
		free(fbuf);
		return 1;
	}
	fclose(fp);

	int raw = fbuf[0] == 0xff;
	if(raw && flen % FRAME_BYTES){
		free(fbuf);
		return 1;
	}

	int count_ok = 0;
	int count_bad = 0;
	size_t span = raw ? PACK_SPAN : PACK_BYTES;
	for(size_t p=0; (size_t)flen-p >= span; p+=PACK_BYTES){
		u8 packet[PACK_BYTES];
		u8 *data = fbuf+p;
		if(raw){
			for(unsigned j=0; j<PACK_BYTES; ++j)
				packet[j] = raw_symbol(fbuf, p+interleave[j]);
			data = packet;
		}

		int result = correct_packet(data);
		if(result < 0){
			count_bad++;
		}else if(result){
			count_ok++;
			if(raw){
				for(unsigned j=0; j<PACK_BYTES; ++j)
					set_raw_symbol(fbuf, p+interleave[j], packet[j]);
			}
		}
	}

	if(count_ok){
		printf("%d packs fixed!\n", count_ok);
	}
	if(count_bad){
		printf("%d packs failed!\n", count_bad);
	}

	if(count_ok==0){
		if(count_bad==0){
			printf("No error found!\n");
		}
		goto _exit;
	}


	fp = fopen(argv[1], "wb");
	fwrite(fbuf, 1, flen, fp);
	fclose(fp);

	printf("Writing done.\n");

_exit:
	free(fbuf);
	system("pause");
	return 0;
}


/******************************************************************************/


