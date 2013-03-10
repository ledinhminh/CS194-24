/* cs194-24 Lab 2 */

#include "drf.h"
#include "cbs.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

struct drf
{
    cbs_t *threads;
    struct drf_vector res;
    size_t max_work_units;
};

int drf_create(drf_t *drf, struct drf_vector *rv, size_t rv_size,
	       size_t max_work_units, int (*entry)(void *), void **args)
{
    struct drf *d;
    size_t i;

    d = malloc(sizeof(*d));
    if (d == NULL)
	return -1;

    memset(&d->res, 0, sizeof(d->res));
    memcpy(&d->res, rv, MIN(sizeof(d->res), rv_size));

    d->threads = malloc(sizeof(*d->threads) * max_work_units);
    if (d->threads == NULL)
	return -1;

    d->max_work_units = max_work_units;

    for (i = 0; i < max_work_units; i++)
	cbs_create(&d->threads[i], CBS_BW, rv->rv_cpu,
		   &rv->rv_cpu_T, entry, args[i]);

    *drf = d;

    return 0;
}

int drf_join(drf_t *drf, int *code)
{
    int ccode;
    int i;
    struct drf *d;
    d = *drf;

    *code = 0;
    for (i = 0; i < d->max_work_units; i++)
    {
	cbs_join(&d->threads[i], &ccode);
	*code |= ccode;
    }

    return 0;
}

size_t drf_max_work_units(drf_t *drf)
{
    struct drf *d;
    d = *drf;

    return d->max_work_units;
}
