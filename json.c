/*
    Copyright (c) 2014
    vurtun <polygone@gmx.net>
    MIT license
*/
#include "json.h"

/* remove gcc warning for static init*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"

static const struct json_iter ITER_NULL;
static const struct json_token TOKEN_NULL;

void
json_read(struct json_token *obj, struct json_iter* iter)
{
    static const void *go_struct[] = {
        [0 ... 255] = &&l_fail,
        ['\t']      = &&l_loop,
        ['\r']      = &&l_loop,
        ['\n']      = &&l_loop,
        [' ']       = &&l_loop,
        ['"']       = &&l_qup,
        [':']       = &&l_sep,
        ['=']       = &&l_sep,
        [',']       = &&l_loop,
        ['[']       = &&l_up,
        [']']       = &&l_down,
        ['{']       = &&l_up,
        ['}']       = &&l_down,
        ['-']       = &&l_bare,
        [48 ... 57] = &&l_bare,
        ['t']       = &&l_bare,
        ['f']       = &&l_bare,
        ['n']       = &&l_bare,
    };
    static const void *go_bare[] = {
        [0 ... 255] = &&l_fail,
        [32 ... 126]= &&l_loop,
        ['\t']      = &&l_unbare,
        ['\r']      = &&l_unbare,
        ['\n']      = &&l_unbare,
        [',']       = &&l_unbare,
        [']']       = &&l_unbare,
        ['}']       = &&l_unbare,
    };
    static const void *go_string[] = {
        [0 ... 31]    = &&l_fail,
        [32 ... 126]  = &&l_loop,
        [127]         = &&l_fail,
        ['\\']        = &&l_esc,
        ['"']         = &&l_qdown,
        [128 ... 191] = &&l_fail,
        [192 ... 223] = &&l_utf8_2,
        [224 ... 239] = &&l_utf8_3,
        [240 ... 247] = &&l_utf8_4,
        [248 ... 255] = &&l_fail,
    };
    static const void *go_utf8[] = {
        [0 ... 127]   = &&l_fail,
        [128 ... 191] = &&l_utf_next,
        [192 ... 255] = &&l_fail
    };
    static const void *go_esc[] = {
        [0 ... 255] = &&l_fail,
        ['"']       = &&l_unesc,
        ['\\']      = &&l_unesc,
        ['/']       = &&l_unesc,
        ['b']       = &&l_unesc,
        ['f']       = &&l_unesc,
        ['n']       = &&l_unesc,
        ['r']       = &&l_unesc,
        ['t']       = &&l_unesc,
        ['u']       = &&l_unesc
    };

    if (!iter || !obj || !iter->src || !iter->len || iter->err) {
        *obj = TOKEN_NULL;
        iter->err = 1;
    }

    *obj = TOKEN_NULL;
    iter->err = 0;
    if (!iter->go)
        iter->go = go_struct;

    unsigned int len = iter->len;
    const unsigned char *cur;
    int utf8_remain = 0;
    for (cur = iter->src; len; cur++, len--) {
        goto *iter->go[*cur];
        l_loop:;
    }

    if (!iter->depth) {
        iter->src = 0;
        iter->len = 0;
        if (obj->str)
            obj->len = (unsigned int)((cur-1) - obj->str);
        return;
    }

l_fail:
    iter->err = 1;
    return;

l_sep:
    if (iter->depth == 2)
        obj->children--;
    goto l_loop;

l_up:
    if (iter->depth == 2)
        obj->children++;
    if (iter->depth++ == 1)
        obj->str = cur;
    goto l_loop;

l_down:
    if (--iter->depth == 1) {
        obj->len = (unsigned int)(cur - obj->str) + 1;
        goto l_yield;
    }
    goto l_loop;

l_qup:
    iter->go = go_string;
    if (iter->depth == 1)
        obj->str = cur;
    if (iter->depth == 2)
        obj->children++;
    goto l_loop;

l_qdown:
    iter->go = go_struct;
    if (iter->depth == 1) {
        obj->len = (unsigned int)(cur - obj->str) + 1;
        goto l_yield;
    }
    goto l_loop;

l_esc:
    iter->go = go_esc;
    goto l_loop;

l_unesc:
    iter->go = go_string;
    goto l_loop;

l_bare:
    if (iter->depth == 1)
        obj->str = cur;
    if (iter->depth == 2)
        obj->children++;
    iter->go = go_bare;
    goto l_loop;

l_unbare:
    iter->go = go_struct;
    if (iter->depth == 1) {
        obj->len = (unsigned int)(cur - obj->str);
        iter->src = cur;
        iter->len = len;
        return;
    }
    goto *iter->go[*cur];

l_utf8_2:
    iter->go = go_utf8;
    utf8_remain = 1;
    goto l_loop;

l_utf8_3:
    iter->go = go_utf8;
    utf8_remain = 2;
    goto l_loop;

l_utf8_4:
    iter->go = go_utf8;
    utf8_remain = 3;
    goto l_loop;

l_utf_next:
    if (!--utf8_remain)
        iter->go = go_string;
    goto l_loop;

l_yield:
    if (iter->depth != 1 || !obj->str)
        goto l_loop;
    iter->src = cur + 1;
    iter->len = len - 1;
    return;
}

double
ipow(int base, unsigned exp)
{
    long long res = 1;
    while (exp) {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        base *= base;
    }
    return (double)res;
}

static double
stoi(struct json_token *tok)
{
    if (!tok->str || !tok->len)
        return 0;
    double n = 0;
    unsigned int i = 0;
    const unsigned int off = (tok->str[0] == '-' || tok->str[0] == '+') ? 1 : 0;
    const unsigned int neg = (tok->str[0] == '-') ? 1 : 0;
    for (i = off; i < tok->len; i++) {
        if ((tok->str[i] >= '0') && (tok->str[i] <= '9'))
            n = (n * 10) + tok->str[i]  - '0';
    }
    return (neg) ? -n : n;
}

static double
stof(struct json_token *tok)
{
    if (!tok->str || !tok->len)
        return 0;
    double n = 0;
    double f = 0.1;
    unsigned int i = 0;
    for (i = 0; i < tok->len; i++) {
        if ((tok->str[i] >= '0') && (tok->str[i] <= '9')) {
            n = n + (tok->str[i] - '0') * f;
            f *= 0.1;
        }
    }
    return n;
}

void
json_num(double *num, const struct json_token *tok)
{
    static const void **go_num[] = {
        [0 ... 255] = &&l_fail,
        [48 ... 57] = &&l_loop,
        ['-'] = &&l_loop,
        ['+'] = &&l_loop,
        ['.'] = &&l_flt,
        ['e'] = &&l_exp,
        ['E'] = &&l_exp,
        [' '] = &&l_break,
        ['\n'] = &&l_break,
        ['\t'] = &&l_break,
        ['\r'] = &&l_break,
    };
    enum {INT, FLT, EXP, TOKS};
    struct json_token nums[TOKS] = {{0}};
    struct json_token *write = &nums[INT];
    write->str = tok->str;

    unsigned int len = tok->len;
    const unsigned char *cur;
    for (cur = tok->str; len; cur++, len--) {
        goto *go_num[*cur];
        l_loop:;
    }
    write->len = (unsigned int)(cur - write->str);

    const double i = stoi(&nums[INT]);
    const double f = stof(&nums[FLT]);
    const double e = stoi(&nums[EXP]);
    double p = ipow(10, (unsigned)((e < 0) ? -e : e));
    if (e < 0)
        p = (1 / p);
    *num = (i + ((i < 0) ? -f : f)) * p;
    return;

l_flt:
    write->len = (unsigned int)(cur - write->str);
    write = &nums[FLT];
    write->str = cur + 1;
    goto l_loop;

l_exp:
    write->len = (unsigned int)(cur - write->str);
    write = &nums[EXP];
    write->str = cur + 1;
    goto l_loop;

l_break:
    len = 1;
    goto l_loop;
l_fail:
    return;
}


#pragma GCC diagnostic pop
