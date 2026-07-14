#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <string.h>

char *get_default_location(void)
{
    char *home = getenv("HOME");
    if (home)
        return strdup(home);
    return strdup("/");
}
