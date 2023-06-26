#include <locale.h>
#include <string.h>
#include <limits.h>

struct lconv current_locale_data;
char current_locale_string[64] = "C";

struct lconv* localeconv(void) {
    current_locale_data.decimal_point = ".";
    current_locale_data.thousands_sep = "";
    current_locale_data.grouping = "";
    current_locale_data.int_curr_symbol = "";
    current_locale_data.currency_symbol = "";
    current_locale_data.mon_decimal_point = "";
    current_locale_data.mon_thousands_sep = "";
    current_locale_data.mon_grouping = "";
    current_locale_data.positive_sign = "";
    current_locale_data.negative_sign = "";
    current_locale_data.int_frac_digits = -1;
    current_locale_data.frac_digits = -1;
    current_locale_data.p_cs_precedes = -1;
    current_locale_data.p_sep_by_space = -1;
    current_locale_data.n_cs_precedes = -1;
    current_locale_data.n_sep_by_space = -1;
    current_locale_data.p_sign_posn = -1;
    current_locale_data.n_sign_posn = -1;
    
    return &current_locale_data;
}

char* setlocale(int category, const char* locale) {
    if (locale == NULL) {
        /*
        * "If locale is NULL, the current locale is only queried, not modified."
        */
        return current_locale_string;
    }

    /*
    * We only use the category if "locale is not NULL"
    */
    switch (category) {
        case LC_ALL:
        case LC_COLLATE:
        case LC_CTYPE:
        case LC_MESSAGES:
        case LC_MONETARY:
        case LC_NUMERIC:
        case LC_TIME:
            break;
        default:
            return NULL;
    }

    if (!strcmp(locale, "") || !strcmp(locale, "C") || !strcmp(locale, "POSIX")) {
        /*
        * This is where we would actually modifiy the locale. 
        * However, as we only support the default locale, we don't need to do anything.
        */
       
        
        if (!strcmp(locale, "")) {
            /*
            * An empty locale string means to use the environment to determine the locale.
            * We only support the default locale, so we will return "C".
            */
            strcpy(current_locale_string, "C");

        } else {
            /*
            * This won't overflow, because we just checked for the string contents.
            */
            strcpy(current_locale_string, locale);
        }

        return current_locale_string;
    }

    /*
    * "If its value is not a valid locale specification, the locale is
    * unchanged, and setlocale() returns NULL."
    */
    return NULL;
}