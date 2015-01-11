/*
    Copyright (c) 2014
    vurtun <polygone@gmx.net>
    MIT license
*/
#ifndef JSON_H_
#define JSON_H_

struct json_token {
    const unsigned char *str;
    unsigned long len;
    unsigned long children;
};

struct json_iter {
    int depth;
    int err;
    const void **go;
    const unsigned char *src;
    unsigned long len;
};

void json_read(struct json_token*, struct json_iter*);
void json_num(double *, const struct json_token*);

#endif
