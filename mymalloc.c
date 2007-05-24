
/*
 * Copyright (c) 1998  Dustin Sallings
 *
 * $Id: mymalloc.c,v 1.8 2002/06/05 19:26:57 dustin Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

/* #define NUM_MEMBUCKETS 16369 */
/* #define NUM_MEMBUCKETS 24571 */
#define NUM_MEMBUCKETS 12289

#ifdef MYMALLOC
static struct memories {
	char   *p;
	size_t  size;
	struct memories *next;
	char   *file;
	int     line;
}     **memories = NULL;

static int *_mem_stats = NULL;

void    _mdebug_dump(void);

#ifdef MYMALLOC_VERBOSE
# define mmverbose_print(a) printf a
#else
# define mmverbose_print(a)
#endif /* MYMALLOC_VERBOSE */

static void
_init_memories()
{
	if (memories == NULL) {
		memories = calloc(NUM_MEMBUCKETS, sizeof(struct memories *));
		assert(memories);
		_mem_stats = calloc(NUM_MEMBUCKETS, sizeof(int));
		assert(_mem_stats);
	}
}

static int 
_getmemindex(void *p)
{
	return ((int) p % NUM_MEMBUCKETS);
}

static void 
_register_mem(void *p, size_t size, char *file, int line)
{
	struct memories *m, *c;
	int     index;

	assert(p);
	m = malloc(sizeof(struct memories));
	assert(m);
	m->p = p;
	m->size = size;
	m->next = NULL;
	m->file = file;
	m->line = line;

	_init_memories();

	index = _getmemindex(p);

	/* Gather statistics */
	_mem_stats[index]++;

	c = memories[index];

	if (c == NULL) {
		memories[index] = m;
	} else {
		for (; c->next != NULL; c = c->next);
		c->next = m;
	}
}

struct memories   *
_lookup_mem(void *p)
{
	struct memories *c;
	int     index;
	index = _getmemindex(p);
	assert(memories);
	for (c = memories[index]; c && c->p != p; c = c->next);
	return (c);
}

static void 
_unregister_mem(void *p)
{
	struct memories *c, *tmp;
	int     index;

	index = _getmemindex(p);
	assert(memories);

	/* Special case for first thingy */
	if (memories[index]->p == p) {
		if (memories[index]->next) {
			tmp = memories[index]->next;
			free(memories[index]);
			memories[index] = tmp;
		} else {
			free(memories[index]);
			memories[index] = NULL;
		}
	} else {
		for (c = memories[index]; c && c->next->p != p; c = c->next);
		assert(c);
		tmp = c->next;
		c->next = c->next->next;
		free(tmp);
	}
}

void 
_mdebug_dump(void)
{
	struct memories *m;
	int     i, count = 0, min, max, total, empty;

	_init_memories();

	assert(memories);

	for (i = 0; i < NUM_MEMBUCKETS; i++) {
		for (m = memories[i]; m; m = m->next) {
			char printme[32];
			int c=0;

			for(c=0; c<m->size && c<sizeof(printme); c++) {
				if(isprint(m->p[c])) {
					printme[c]=m->p[c];
				} else {
					printme[c]='?';
				}
			}
			printme[c]=0x00;
			fprintf(stderr, "Found memory at %d (%p) \"%s\"\n",
				i, (void *) m, printme);
			count++;
			fprintf(stderr, "MEM:  %p is %lu bytes\n", (void *) m->p,
				(long)m->size);
			fprintf(stderr, "\t%s line %d\n", m->file, m->line);
		}
	}

	if (count == 0) {
		fprintf(stderr, "No registered memory\n");
	}

	max = 0;
	min = INT_MAX;
	total = 0;
	empty = 0;
	for (i = 0; i < NUM_MEMBUCKETS; i++) {
		if (_mem_stats[i] == 0)
			empty++;
		if (_mem_stats[i] > max)
			max = _mem_stats[i];
		if (_mem_stats[i] < min)
			min = _mem_stats[i];
		total += _mem_stats[i];
	}

	fprintf(stderr, "Hash size was %d buckets\n"
		"Chunks in use:  %d\n"
	    "Highest:        %d\n"
	    "Lowest:         %d\n"
	    "Average:        %f\n"
		"Total alloc:    %d\n"
	    "Empty:          %d\n",
	    NUM_MEMBUCKETS, count, max, min,
			((float)total / (float)NUM_MEMBUCKETS), total, empty);
}

void 
_mdebug_long_stats(void)
{
	int     i;
	for (i = 0; i < NUM_MEMBUCKETS; i++) {
		if (_mem_stats[i] > 0)
			fprintf(stderr, "MEM:  %d got %d hits\n", i, _mem_stats[i]);
	}
}

void   *
_my_malloc(size_t size, char *file, int line)
{
	void   *p;
	mmverbose_print(("+++ Mallocing at %s:%d\n", file, line));
	p = malloc(size);
	assert(p);
	_register_mem(p, size, file, line);
	return (p);
}

void   *
_my_calloc(size_t n, size_t size, char *file, int line)
{
	void   *p;
	mmverbose_print(("+++ Callocing at %s:%d\n", file, line));
	p = calloc(n, size);
	assert(p);
	_register_mem(p, size * n, file, line);
	return (p);
}

char   *
_my_strdup(const char *str, char *file, int line)
{
	char   *p;
	mmverbose_print(("+++ Duping %s at %s:%d\n", str, file, line));
	p = strdup(str);
	_register_mem(p, strlen(p), file, line);
	return (p);
}

void   *
_my_realloc(void *p, size_t size, char *file, int line)
{
	void   *ret;
	mmverbose_print(("+++ Reallocing at %s:%d\n", file, line));
	assert(_lookup_mem(p));
	_unregister_mem(p);
	ret = realloc(p, size);
	assert(ret);
	_register_mem(ret, size, file, line);
	return (ret);
}

void 
_my_free(void *p, char *file, int line)
{
	struct memories   *tmp;
	tmp = _lookup_mem(p);
	if (tmp == NULL) {
		fprintf(stderr, "Trying to free something that isn't mine:  "
			"%p (%s) (%s:%d)\n",
		    p, (char *) p,
		    file, line);
		_mdebug_dump();
		abort();
	} else {
		mmverbose_print(("--- Freeing memory from %s:%d\n",
			tmp->file, tmp->line));
		/* This helps us find bugs more easier on some systems. */
		memset(p, 0x00, tmp->size);
	}
	_unregister_mem(p);
	free(p);
}

void
_mymalloc_assert(void *p, char *file, int line)
{
	struct memories   *tmp;

	tmp = _lookup_mem(p);
	if(tmp==NULL) {
		fprintf(stderr, "allocation assertion failure:  %p (%s:%d)\n",
			p, file, line);
		_mdebug_dump();
		abort();
	}
}

#endif /* MYMALLOC */
