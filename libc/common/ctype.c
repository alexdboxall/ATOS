#include <ctype.h>

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        c -= 32;
    }
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        c += 32;
    }
    return c;
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isalpha(int c) {
    return isupper(c) || islower(c);
}

int iscntrl(int c) {
    return c < 32 || c == 127;
}

int isblank(int c) {
    return c == '\t' || c == ' ';
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isgraph(int c) {
    return isalnum(c) || ispunct(c);
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int isprint(int c) {
    return isgraph(c) || c == ' ';
}

int ispunct(int c) {
    const char* punct = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    for (int i = 0; punct[i]; ++i) {
        if (c == punct[i]) {
            return 1;
        }
    }
    return 0;
}

int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int isxdigit(int c) {
    return isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}