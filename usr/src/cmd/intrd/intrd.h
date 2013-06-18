/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2013 David Hoeppner.  All rights reserved.
 */

#include <sys/list.h>
#include <sys/param.h>
#include <sys/pci_tools.h>
#include <sys/poll.h>
#include <sys/processor.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <kstat.h>
#include <libgen.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stropts.h>
#include <syslog.h>
#include <unistd.h>

#define	dprint(f, s)	(void) fprintf(stderr, f, s)

#define	INTRD_NORMAL_SLEEPTIME	10		/* time to sleep between samples */
#define	INTRD_IDLE_SLEEPTIME	45		/* time to sleep when idle */
#define	INTRD_ONECPU_SLEEPTIME	(60 * 15)	/* used if only 1 CPU on system */

/*
 * Single CPU.
 */
typedef struct intrd_cpu {
	list_node_t	ic_next;
	list_t		ic_ivec_list;	/* list of interupt vectors */
	uint_t		ic_number_ivecs;
	int		ic_instance;
	uint64_t	ic_tot;		/* total time idle + user + kernel */
	int64_t		ic_intrs;
	uint64_t	ic_big_intrs;
	hrtime_t	ic_crtime;
	float		ic_intr_load;
} intrd_cpu_t;

/*
 * Interrupt vector.
 */
typedef struct intrd_ivec {
	list_node_t	ii_next;
	uint64_t	ii_cookie;
	uint64_t	ii_ihs;
	uint64_t	ii_ino;
	uint64_t	ii_num_ino;
	uint64_t	ii_pil;
	uint64_t	ii_time;
	char		*ii_buspath;
	char		*ii_name;
	hrtime_t	ii_crtime;
} intrd_ivec_t;

/*
 * MSI device.
 */
typedef struct intrd_msi {
	list_node_t	im_next;
	const char	*im_name;
	list_t		im_ino_list;
} intrd_msi_t;

typedef struct intrd_msi_ino {
	list_node_t	imi_next;
	uint64_t	imi_ino;
	intrd_ivec_t	*imi_ivec;
} intrd_msi_ino_t;

/*
 * Root element of the statistics tree.
 */
typedef struct intrd_stat {
	list_t		is_cpu_list;
	uint_t		is_number_cpus;
	hrtime_t	is_snaptime;
} intrd_stat_t;

/*
 * Delta data structure.
 */
typedef struct intrd_delta {
	list_node_t	id_next;
	list_t		id_cpu_list;
	hrtime_t	id_snaptime_min;
	hrtime_t	id_snaptime_max;
	boolean_t	id_missing;
	float		id_avg_intr_load;
	uint64_t	id_avg_intr_nsec;
	float		id_goodness;
} intrd_delta_t;

/*
 * Function prototypes.
 */
void		intrd_delta_dump(intrd_delta_t *);
intrd_delta_t	*intrd_delta_generate(intrd_stat_t *, intrd_stat_t *);
intrd_delta_t	*intrd_delta_compress(list_t);

int		intrd_do_reconfig(void);
void		intrd_clear_deltas(void);

void		intrd_kstat_free(intrd_stat_t *);
void		intrd_kstat_dump(intrd_stat_t *);
intrd_stat_t	*intrd_kstat_get(kstat_ctl_t *, boolean_t);

intrd_cpu_t	*intrd_cpu_create(int);
intrd_cpu_t	*intrd_cpu_find(list_t, int);
intrd_ivec_t	*intrd_cpu_find_ivec(intrd_cpu_t *, char *, uint64_t);
intrd_ivec_t	*intrd_ivec_create(const char *, uint64_t);
intrd_ivec_t	*intrd_ivec_find_ino(list_t, uint64_t);
intrd_msi_t	*intrd_msi_create(const char *);
intrd_msi_t	*intrd_msi_find(list_t, const char *);
void		intrd_msi_set_ino(intrd_msi_t *, uint64_t, intrd_ivec_t *);
void		logperror(const char *, ...);
int		intrd_is_apic(const char *);
boolean_t	intrd_verify(boolean_t, const char *);

static float		intrd_goodness_cpu(intrd_cpu_t *, float);
