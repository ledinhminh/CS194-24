/* cs194-24 Lab 1 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "work.h"

int main(int argc, char **argv)
{
    char t[1024], ct[1024], rt[1024];
    struct timeval tv, ctv, rtv;
    struct tm *tm, *ctm, *rtm;

    gettimeofday(&tv, NULL);
    memcpy(&ctv, &tv, sizeof(ctv));
    memcpy(&rtv, &tv, sizeof(rtv));

#if defined(MINUTE_CLOCK)
    tv.tv_sec = (tv.tv_sec / 60) * 60;
#elif defined(HOUR_CLOCK)
    tv.tv_sec = (tv.tv_sec / 3600) * 3600;
#else
    #error "Decide on minute or hour resolution"
#endif
    ctv.tv_sec = (ctv.tv_sec / 60) * 60 + 60;
    tm = gmtime(&tv.tv_sec);
    strftime(t, 1024, "%a, %d %b %Y %T %z", tm);
    ctm = gmtime(&ctv.tv_sec);
    strftime(ct, 1024, "%a, %d %b %Y %T %z", ctm);
    rtm = gmtime(&rtv.tv_sec);
    strftime(rt, 1024, "%a, %d %b %Y %T %z", rtm);

    fprintf(stdout,
	    "HTTP/1.1 200 OK\r\n"
	    "Content-Type: text/html\r\n"
	    "Date: %s\r\n"
	    "Expires: %s\r\n"
	    "ETag: \"%lx\"\r\n"
	    "\r\n",
	    rt,
	    ct,
	    tv.tv_sec
	);

    do_work();

    fprintf(stdout,
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
