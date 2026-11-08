
/**********************************************************/

/*
 *
 * printk.c
 *
 * printk for bootbox
 *
 * support format:
 *  %c
 *  %s
 *  %[[0]n]x
 *  %[[0]n]X
 *  %[[0]n]d
 *
 */

#include <stdarg.h>
#include <limits.h>
#include "main.h"


/* At most 11 octal digits for a 32-bit word; sign and NUL are separate. */
static char *itostr(char *end, int in_data, int base, int upper, int sign)
{
	unsigned int data = in_data;
	if(base==10 && sign && in_data<0)
		data = 0u-data;

	do{
		unsigned int digit = data%base;
		data /= base;
		*--end = digit<10 ? digit+'0' : digit+(upper ? 'A' : 'a')-10;
	}while(data);
	return end;
}

/*
 * vsnprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @args: Arguments for the format string
 * Custom return value: stored bytes including NUL, or zero for invalid input.
 * This preserves the original API rather than adopting libc printf semantics.
 */

#define OUT_C(c) \
	if(str<end){ \
		*str++ = (c); \
	}

int vsnprintf(char *buf, int size, char *fmt, va_list args)
{
	char ch, *s, *str, *end, *sstr;
	char digital_buf[11];
	_Static_assert(UINT_MAX == 0xffffffffu, "formatter requires 32-bit words");
	int zero_pad, left_adj, add_sign, field_width, sign;
	int i, base, upper, len;

	if(!buf || !fmt || size<=0){
		return 0;
	}

	str = buf;
	end = buf+size-1;

	while(str<end && *fmt){
		if(*fmt!='%'){
			if(*fmt=='\n')
				OUT_C('\r');
			OUT_C(*fmt++);
			continue;
		}

		/* skip '%' */
		sstr = fmt;
		fmt++;

		/* %% */
		if(*fmt=='%'){
			OUT_C(*fmt++);
			continue;
		}

		/* get flag */
		zero_pad = ' ';
		left_adj = 0;
		add_sign = 0;
		while((ch=*fmt)!=0){

			if(*fmt=='0'){
				zero_pad = '0';
			}else if(*fmt=='-'){
				left_adj = 1;
			}else if(*fmt=='#'){
			}else if(*fmt==' '){
				if(add_sign!='+')
					add_sign = ' ';
			}else if(*fmt=='+'){
				add_sign = '+';
			}else{
				break;
			}
			fmt++;
		}

		/* get field width: m.n */
		field_width = 0;
		/* get m */
		while(*fmt && *fmt>='0' && *fmt<='9'){
			int digit = *fmt-'0';
			if(field_width > INT_MAX/10 ||
			   (field_width == INT_MAX/10 && digit > INT_MAX%10))
				field_width = INT_MAX;
			else
				field_width = field_width*10+digit;
			fmt++;
		}
		if(*fmt && *fmt=='.'){
			fmt++;
			/* skip n */
			while(*fmt && *fmt>='0' && *fmt<='9'){
				fmt++;
			}
		}

		/* get format char */
		upper = 0;
		base = 0;
		sign = 0;
		len = 0;
		s = digital_buf;
		while((ch=*fmt)!=0){
			fmt++;
			switch(ch){
			/* hexadecimal */
			case 'p':
			case 'X':
				upper = 1;
			case 'x':
				base = 16;
				break;

			/* decimal */
			case 'd':
			case 'i':
				sign = 1;
			case 'u':
				base = 10;
				break;

			/* octal */
			case 'o':
				base = 8;
				break;

			/* character */
			case 'c':
				digital_buf[0] = (unsigned char) va_arg(args, int);
				len = 1;
				break;

			/* string */
			case 's':
				s = va_arg(args, char *);
				if(!s) s = "<NUL>";
				len = strlen(s);
				break;

			/* float format, skip it */
			case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'a': case 'A':
				(void)(va_arg(args, double));
				s = 0;
				break;

			/* length modifier */
			case 'l': case 'L': case 'h': case 'j': case 'z': case 't':
				/* skip it */
				continue;

			/* bad format */
			default:
				s = sstr;
				len = fmt-sstr;
				break;
			}
			break;
		}

		if(base){
			i = va_arg(args, int);
			if(base==10 && sign){
				if(i<0){
					add_sign = '-';
				}
			}else{
				add_sign = 0;
			}

			s = itostr(digital_buf+sizeof(digital_buf), i, base, upper, sign);
			len = digital_buf+sizeof(digital_buf)-s;
		}else{
			zero_pad = ' ';
			add_sign = 0;
		}

		if(s){
			int padding = field_width-len-(add_sign!=0);
			if(!left_adj){
				if(add_sign && zero_pad=='0'){
					OUT_C(add_sign);
					add_sign = 0;
				}
				while(padding>0 && str<end){
					OUT_C(zero_pad);
					padding--;
				}
			}
			if(add_sign){ OUT_C(add_sign); }
			for(i=0; i<len && str<end; i++){ OUT_C(*s++); }
			while(padding>0 && str<end){
				OUT_C(' ');
				padding--;
			}
		}
	}

	*str = 0;
	return str-buf+1;
}

/**********************************************************/

void (*printk_putc)(int ch) = NULL;
void (*printk_puts)(char *) = NULL;
void stm32_puts(char *str);
int to_stm32 = 0;


int printk(char *fmt, ...)
{
	va_list args;
	int printed_len;
	char printk_buf[128];
	char *p;

	/* Emit the output into the temporary buffer */
	va_start(args, fmt);
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), fmt, args);
	va_end(args);
	if(!printed_len) return 0;

	if(to_stm32){
		stm32_puts(printk_buf);
	}else if(printk_puts){
		printk_puts(printk_buf);
	}else if(printk_putc){
		for (p = printk_buf; *p; p++) {
			printk_putc(*p);
		}
	}

	return printed_len;
}


int snprintf(char *buf, int size, char *fmt, ...)
{
	va_list args;
	int printed_len;

	va_start(args, fmt);
	printed_len = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return printed_len;
}


/**********************************************************/


