/* cs194-24 Lab 1 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "work.h"

int main(int argc, char **argv)
{
    char t[1024], ct[1024];
    struct timeval tv, ctv;
    struct tm *tm, *ctm;

    gettimeofday(&tv, NULL);
    memcpy(&ctv, &tv, sizeof(ctv));

    ctv.tv_sec = (ctv.tv_sec / 60) * 60 + 60 + 15;
    tm = gmtime(&tv.tv_sec);
    strftime(t, 1024, "%a, %d %b %Y %T %z", tm);
    ctm = gmtime(&ctv.tv_sec);
    strftime(ct, 1024, "%a, %d %b %Y %T %z", ctm);

    fprintf(stderr,
	    "HTTP/1.1 200 OK\r\n"
	    "Content-Type: text/html\r\n"
	    "Date: %s\r\n"
	    "Expires: %s\r\n"
	    "ETag: \"%lx\"\r\n"
	    "\r\n",
	    t,
	    ct,
	    tv.tv_sec
	);

    do_work();

    fprintf(stderr,
	    "<HTML>\r\n"
	    "<HEAD>\r\n"
	    "  <TITLE>A Simple CGI Test</TITLE>\r\n"
	    "</HEAD>\r\n"
	    "<BODY>\r\n"
	    "%s<BR/>\r\n"
	    "</BODY>\r\n"
	    "</HTML>\r\n",
	    t
	);

    return 0;
}
