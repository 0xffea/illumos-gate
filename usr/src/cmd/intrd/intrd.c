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

/*
 * Interrupt Load Balancer.
 *
 *
 */

#include "intrd.h"

boolean_t debug = B_FALSE;
boolean_t using_scengen = B_FALSE;

int	sleeptime = INTRD_NORMAL_SLEEPTIME;	/* either normal_ or idle_ or onecpu_ */
float	timerange_toohi = 0.01f;

hrtime_t	statslen = 60;

/* Goodness */
float	goodness_unsafe_load = 0.9f;
float	goodness_min_delta = 0.1f;

static void
intrd_daemonize(void)
{
	if (daemon(0, 0) == -1) {
		logperror("fork");
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	intrd_stat_t	*stat;
	intrd_stat_t	*new_stat;
	kstat_named_t	*knp;
	kid_t		kid;
	kstat_ctl_t	*kc;
	kstat_t		*ksp;
	char		*cmdname;
	int		c;
	int		error;
	boolean_t	pcplusmp_sys = B_FALSE;

	hrtime_t	deltas_tottime = 0;
	list_t		delta_list;

	cmdname = basename(argv[0]);

	/*
	 * Parse arguments. intrd does not accept any public arguments; the
	 * two arguments below are meant for testing purposes.  -D generates
	 * a significant amount of syslog output.  -S <filename> loads the
	 * filename.  That file is expected to implement a kstat "simulator"
	 * which can be used to feed information to intrd and verify
	 * intrd's responses.
	 */
	while ((c = getopt(argc, argv, "DS:")) != EOF) {
		switch (c) {
			case 'S':
				using_scengen = B_TRUE;
				break;
			case 'D':
				debug = B_TRUE;
				break;
			default:
				break;
		}
	}

	if (!using_scengen) {
		/*
		 * We don't use the simulator.
		 */
		openlog(cmdname, LOG_PID, LOG_DAEMON);
		(void) setlogmask(LOG_UPTO(debug ? LOG_DEBUG : LOG_INFO));
	}

	if (debug)
		syslog(LOG_DEBUG, "intrd is starting (debug)");
	else
		syslog(LOG_DEBUG, "intrd is starting");

	/*
	 * Open the kernel statistics framework.
	 */
	while ((kc = kstat_open()) == NULL) {
		if (errno == EAGAIN) {
			(void) poll(NULL, 0, 200);
		} else {
			logperror("kstat_open");
			exit(1);	/* XXX restart */
		}
	}

	/*
	 * If no pci_intrs kstats were found, we need to exit, but we can't
	 * because SMF will restart us and/or report an error to the
	 * administrator.  But there's nothing an administrator can do.  So
	 * print out a message for SMF logs and silently pause forever.
	 */
	ksp = kstat_lookup(kc, "pci_intrs", -1, NULL);
	if (ksp == NULL) {
		logperror("%s : no interrupts were found; "
		    "your PCI bus may not yet be supported\n", cmdname);
		(void) pause();
		exit(0);
	}

	/*
	 * See if this is a system with a pcplusmp APIC.
	 * Such systems will get special handling.
	 * Assume that if one bus has a pcplusmp APIC that they all do.
	 *
	 * Use its buspath to query the system.  It is assumed that either all or none
	 * of the busses on a system are hosted by the pcplusmp APIC or APIX.
	 */
	kid = kstat_read(kc, ksp, NULL);
	knp = kstat_data_lookup(ksp, "buspath");
	if (intrd_is_apic(KSTAT_NAMED_STR_PTR(knp)) == 0)
		pcplusmp_sys = B_TRUE;

	list_create(&delta_list, sizeof (intrd_delta_t),
	    offsetof(intrd_delta_t, id_next));

	stat = intrd_kstat_get(kc, pcplusmp_sys);

	for (;;) {
		intrd_delta_t	*delta;
		intrd_delta_t	*delta_compressed;
		hrtime_t	delta_time;
		boolean_t	do_reconfig = B_FALSE;
		boolean_t	below_statslen;

		/*
		 * Sleep and update the kernel statistics.
		 */
		if (using_scengen) {
			/* XXX */
		} else {
			sleep(sleeptime);
			kstat_chain_update(kc);
		}

		new_stat = intrd_kstat_get(kc, pcplusmp_sys);

		if (debug) {
			intrd_kstat_dump(new_stat);
		}

		/*
		 * stat or new_stat could be zero if they're uninitialized, or if
		 * intrd_kstat_get() failed. If stat is zero, move new_stat to stat, sleep
		 * and try again. If new_stat is zero, then we also sleep and try
		 * again, hoping the problem will clear up.
		 */
		if (new_stat == NULL)
			continue;

		if (stat == NULL) {
			stat = new_stat;
			continue;
		}

		/*
		 * Compare new_stat with the prior set of values,
		 * result in delta.
		 */
		delta = intrd_delta_generate(stat, new_stat);

		if (debug) {
			/* Dump most recent stats to stdout */
			intrd_delta_dump(delta);
		}

		/* The new stats now become the old stats */
		intrd_kstat_free(stat);
		stat = new_stat;

		/*
		 * Check for reconfigurations and system overload.
		 */
		/* XXX data types? */
		delta_time = delta->id_snaptime_max - delta->id_snaptime_min;
		if (delta->id_missing || delta_time > statslen) {
			intrd_clear_deltas();
			syslog(LOG_DEBUG, "evaluating interrupt assignments");
			continue;
		}

		/*
		 * Decide if we must do a reconfiguration and
		 * insert delta into global list of deltas.
		 */
		below_statslen = (deltas_tottime < statslen);
		deltas_tottime += delta_time;
		do_reconfig = (below_statslen && deltas_tottime >= statslen);

		list_insert_tail(&delta_list, delta);

		delta_compressed = intrd_delta_compress(delta_list);
		if (delta_compressed == NULL) {
			intrd_clear_deltas();
			// XXX syslog
			continue;
		}

		if (debug)
			intrd_delta_dump(delta_compressed);

		if (do_reconfig) {
			error = intrd_do_reconfig();

			if (error != 0) {
				intrd_clear_deltas();
				if (error == -1) {
					syslog(LOG_DEBUG,
					    "do_reconfig FAILED!");
				}
			} else {
				syslog(LOG_DEBUG,
				    "setting new baseline of XXX");
			}
		}

		syslog(LOG_DEBUG, "---------------------------------------");
	}

	(void) kstat_close(kc);

	return (0);
}

/*
 * Create a new cpu structure.
 */
intrd_cpu_t *
intrd_cpu_create(int instance)
{
	intrd_cpu_t	*cpu;

	cpu = (intrd_cpu_t *)malloc(sizeof (intrd_cpu_t));
	if (cpu == NULL)
		return (cpu);

	list_link_init(&cpu->ic_next);

	cpu->ic_instance = instance;
	cpu->ic_intrs = 0;
	cpu->ic_big_intrs = 0;
	cpu->ic_number_ivecs = 0;

	list_create(&cpu->ic_ivec_list, sizeof (intrd_ivec_t),
	    offsetof(intrd_ivec_t, ii_next));

	return (cpu);
}

/*
 * Find a cpu by instance number.
 */
intrd_cpu_t *
intrd_cpu_find(list_t cpu_list, int instance)
{
	intrd_cpu_t	*cpu;

	for (cpu = list_head(&cpu_list); cpu != NULL;
	    cpu = list_next(&cpu_list, cpu)) {
		if (cpu->ic_instance == instance)
			return (cpu);
	}

	return (NULL);
}

intrd_ivec_t *
intrd_cpu_find_ivec(intrd_cpu_t *cpu, char *buspath, uint64_t ino)
{
	intrd_ivec_t	*ivec;

	for (ivec = list_head(&cpu->ic_ivec_list); ivec != NULL;
	    ivec = list_next(&cpu->ic_ivec_list, ivec)) {
		if (ivec->ii_ino == ino)
			return (ivec);
	}

	return (NULL);
}

intrd_ivec_t *
intrd_ivec_create(const char *buspath, uint64_t ino)
{
	intrd_ivec_t	*ivec;

	ivec = (intrd_ivec_t *)malloc(sizeof (intrd_ivec_t));
	if (ivec == NULL)
		return (ivec);

	list_link_init(&ivec->ii_next);

	ivec->ii_buspath = strdup(buspath);
	ivec->ii_ino = ino;
	ivec->ii_num_ino = 1;
	ivec->ii_ihs = 1;

	return (ivec);
}

intrd_ivec_t *
intrd_ivec_find_ino(list_t ivec_list, uint64_t ino)
{
	intrd_ivec_t	*ivec;

	for (ivec = list_head(&ivec_list); ivec != NULL;
	    ivec = list_next(&ivec_list, ivec)) {
		if (ivec->ii_num_ino == ino)
			return (ivec);
	}

	return (NULL);
}

intrd_msi_t *
intrd_msi_create(const char *name)
{
	intrd_msi_t	*msi;

	msi = (intrd_msi_t *)malloc(sizeof (intrd_msi_t));
	if (msi == NULL)
		return (msi);

	list_link_init(&msi->im_next);

	msi->im_name = name;

	list_create(&msi->im_ino_list, sizeof (intrd_msi_ino_t),
	    offsetof(intrd_msi_ino_t, imi_next));

	return (msi);
}

intrd_msi_t *
intrd_msi_find(list_t msi_list, const char *name)
{
	intrd_msi_t	*msi;

	for (msi = list_head(&msi_list); msi != NULL;
	    msi = list_next(&msi_list, msi)) {
		if (strcmp(msi->im_name, name) == 0)
			return (msi);
	}

	return (NULL);
}

void
intrd_msi_set_ino(intrd_msi_t *msi, uint64_t ino, intrd_ivec_t *ivec)
{
	intrd_msi_ino_t	*msi_ino;

	for (msi_ino = list_head(&msi->im_ino_list); msi_ino != NULL;
	    msi_ino = list_next(&msi->im_ino_list, msi_ino)) {
		if (msi_ino->imi_ino == ino) {
			msi_ino->imi_ivec = ivec;
			return;
		}
	}
}

void
logperror(const char *fmt, ...)
{
	if (debug) {
		syslog(LOG_DEBUG, "XXX"); /* XXX */
	} else {
		(void) fprintf(stderr, "XXX");
	}
}

/*
 * The following 3 functions are taken from the Intr perl module.
 */
static int
open_dev(const char *path)
{
	char	intrpath[MAXPATHLEN];

	(void) strcpy(intrpath, "/devices");
	(void) strcat(intrpath, path);
	(void) strcat(intrpath, ":intr");

	return (open(intrpath, O_RDWR));
}

int
intrd_intr_move(char *path, int old_cpu, int ino, int cpu_id, int num_ino)
{
	pcitool_intr_set_t iset;
	int	fd;
	int	error;

	fd = open_dev(path);
	if (fd == -1)
		return (fd);

	iset.old_cpu = old_cpu;
	iset.ino = ino;
	iset.cpu_id = cpu_id;
	iset.flags = (num_ino > 1) ? PCITOOL_INTR_FLAG_SET_GROUP : 0;
	iset.user_version = PCITOOL_VERSION;

	error = ioctl(fd, PCITOOL_DEVICE_SET_INTR, &iset);
	(void) close(fd);

	if (error == -1)
		return (error);

	return (0);
}

int
intrd_is_apic(const char *path)
{
	pcitool_intr_info_t iinfo;
	int	fd;
	int	error;

	fd = open_dev(path);
	if (fd == -1)
		return (fd);

	iinfo.user_version = PCITOOL_VERSION;

	error = ioctl(fd, PCITOOL_SYSTEM_INTR_INFO, &iinfo);
	(void) close (fd);

	if (error == -1)
		return (error);

	if (iinfo.ctlr_type == PCITOOL_CTLR_TYPE_PCPLUSMP ||
	    iinfo.ctlr_type == PCITOOL_CTLR_TYPE_APIX) {
		return (0);
	}

	return (1);
}

boolean_t
intrd_verify(boolean_t condition, const char *message)
{
	boolean_t result = condition == B_FALSE;

	if (result) {
		syslog(LOG_DEBUG, "VERIFY: %s", message);
	}

	return (result);
}

/*
 * Clears up and frees a single statistics probe.
 */
void
intrd_kstat_free(intrd_stat_t *stat)
{
	intrd_cpu_t	*cpu;
	intrd_ivec_t	*ivec;

	for (cpu = list_head(&stat->is_cpu_list); cpu != NULL;
	    cpu = list_next(&stat->is_cpu_list, cpu)) {
		for (ivec = list_head(&cpu->ic_ivec_list); ivec != NULL;
		    ivec = list_next(&cpu->ic_ivec_list, ivec)) {
			free(ivec);
		}
		free(cpu);
	}
}

/*
 * Print a kernel statistics snapshot.
 */
void
intrd_kstat_dump(intrd_stat_t *stat)
{
	intrd_cpu_t	*cpu;
	intrd_ivec_t	*ivec;

	syslog(LOG_DEBUG, "kstat_stat_t:");

	for (cpu = list_head(&stat->is_cpu_list); cpu != NULL;
	    cpu = list_next(&stat->is_cpu_list, cpu)) {

		syslog(LOG_DEBUG, "    CPU %d", cpu->ic_instance);
		syslog(LOG_DEBUG, " number ivec %d", cpu->ic_number_ivecs);

		for (ivec = list_head(&cpu->ic_ivec_list); ivec != NULL;
		    ivec = list_next(&cpu->ic_ivec_list, ivec)) {

			syslog(LOG_DEBUG, "	ivec %d:", ivec->ii_ino);
			syslog(LOG_DEBUG, "	    buspath: %s", ivec->ii_buspath);
		}
	}
}

/*
 * Read relevant kernel statistics.
 */
intrd_stat_t *
intrd_kstat_get(kstat_ctl_t *kc, boolean_t pcplusmp_sys)
{
	intrd_cpu_t	*cpu;
	intrd_ivec_t	*ivec;
	intrd_stat_t	*stat;
	intrd_msi_t	*msi;
	kstat_t		*ksp;
	kstat_named_t	*knp;
	kid_t		kid;
	list_t		msi_list;
	int64_t	snaptime = -1;	/* XXX */
	int64_t	snaptime_min = -1; /* snaptime is always a positive number */
	int64_t snaptime_max;
	float		timerange;

	stat = (intrd_stat_t *)malloc(sizeof (intrd_stat_t));
	if (stat == NULL)
		return (stat);

	stat->is_number_cpus = 0;

	list_create(&stat->is_cpu_list, sizeof (intrd_cpu_t),
	    offsetof(intrd_cpu_t, ic_next));

	list_create(&msi_list, sizeof (intrd_msi_t),
	    offsetof(intrd_msi_t, im_next));

	/*
	 * kstats are not generated atomically.  Each kstat hierarchy will
	 * have been generated within the kernel at a different time.  On a
	 * thrashing system, we may not run quickly enough in order to get
	 * coherent kstat timing information across all the kstats.  To
	 * determine if this is occurring, snaptime_min/max are used to
	 * find the breadth between the first and last snaptime of all the
	 * kstats we access. snaptime_max - snaptime_min roughly represents the
	 * total time taken up in intr_getstat().  If this time approaches the
	 * time between snapshots, our results may not be useful.
	 */
	snaptime_max = snaptime_min;

	/*
	 * Iterate over all cpus.  Check cpu state to make sure the
	 * processor is "on-line".  If not, it isn't accepting interrupts
	 * and doesn't concern us.
	 */
	for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
		if (ksp->ks_type != KSTAT_TYPE_NAMED)
			continue;

		if (strcmp(ksp->ks_module, "cpu_info") == 0) {

			kid = kstat_read(kc, ksp, NULL);
			knp = kstat_data_lookup(ksp, "state");
			if (knp == NULL || strcmp(knp->value.c, PS_ONLINE) != 0)
				continue;

			cpu = intrd_cpu_create(ksp->ks_instance);
			if (cpu == NULL)
				goto error;

			list_insert_tail(&stat->is_cpu_list, cpu);

			stat->is_number_cpus++;
		}
	}

	if (stat->is_number_cpus <= 1) {
		sleeptime = INTRD_ONECPU_SLEEPTIME;
		goto error;
	}

	for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
		if (ksp->ks_type != KSTAT_TYPE_NAMED)
			continue;

		if (strcmp(ksp->ks_module, "pci_intrs") == 0) {
			uint64_t	ino;
			uint64_t	time;
			char		*name;
			char		*type;
			char		*buspath;

			kstat_read(kc, ksp, NULL);

			/* Check if this CPU is online */
			knp = kstat_data_lookup(ksp, "cpu");
			cpu = intrd_cpu_find(stat->is_cpu_list, knp->value.ui64);
			if (cpu == NULL)
				continue;

			/* Check if this interrupt is not disabled */
			knp = kstat_data_lookup(ksp, "type");
			type = knp->value.c;
			if (strcmp(type, "disabled") == 0)
				continue;

			/* Update minimal and maximal snaptime */
			if (ksp->ks_snaptime < snaptime_min)
				snaptime_min = ksp->ks_snaptime;
			else if (ksp->ks_snaptime > snaptime_max)
				snaptime_max = ksp->ks_snaptime;

			/*
			 * Statistics common for new and existing ivecs.
			 */
			knp = kstat_data_lookup(ksp, "ino");
			ino = knp->value.ui64;

			knp = kstat_data_lookup(ksp, "time");
			time = knp->value.ui64;

			knp = kstat_data_lookup(ksp, "name");
			name = strdup(knp->value.c);

			knp = kstat_data_lookup(ksp, "buspath");
			buspath = KSTAT_NAMED_STR_PTR(knp);

			ivec = intrd_cpu_find_ivec(cpu, buspath, ino);
			if (ivec != NULL) {
				ivec->ii_time += time;
				ivec->ii_name = name;	/* XXX append name */

				/*
				 * If this new interrupt sharing ivec represents a
				 * change from an earlier getstat, make sure that
				 * generate_delta will see the change by setting
				 * crtime to the most recent crtime of its components.
				 */
				if (ksp->ks_crtime > ivec->ii_crtime)
					ivec->ii_crtime = ksp->ks_crtime;

				ivec->ii_ihs++;

				continue;
			}

			ivec = intrd_ivec_create(KSTAT_NAMED_STR_PTR(knp), ino);
			if (ivec == NULL)
				goto error;

			ivec->ii_time = time;
			ivec->ii_crtime = ksp->ks_crtime;
			ivec->ii_name = name;

			knp = kstat_data_lookup(ksp, "pil");
			ivec->ii_pil = knp->value.ui64;

			list_insert_tail(&cpu->ic_ivec_list, ivec);
			cpu->ic_number_ivecs++;

			if (pcplusmp_sys && strcmp(type, "msi")) {
				msi = intrd_msi_find(msi_list, name);
				if (msi == NULL) {
					msi = intrd_msi_create(name);
					if (msi == NULL)
						continue;
				}

				intrd_msi_set_ino(msi, ino, ivec);
			}

		} else if (strcmp(ksp->ks_module, "cpu") == 0 && strcmp(ksp->ks_name, "sys") == 0) {

			cpu = intrd_cpu_find(stat->is_cpu_list, ksp->ks_instance);
			if (cpu == NULL)
				continue;

			/* Total idle time */
			kid = kstat_read(kc, ksp, NULL);
			knp = kstat_data_lookup(ksp, "cpu_nsec_idle");
			cpu->ic_tot = knp->value.ui64;

			knp = kstat_data_lookup(ksp, "cpu_nsec_user");
			cpu->ic_tot += knp->value.ui64;

			knp = kstat_data_lookup(ksp, "cpu_nsec_kernel");
			cpu->ic_tot += knp->value.ui64;

			cpu->ic_crtime = ksp->ks_crtime;
			snaptime = ksp->ks_snaptime;
		}

		/* XXX */
		if (snaptime_min == -1 || snaptime < snaptime_min)
			snaptime_min = snaptime;
		else if (snaptime > snaptime_max)
			snaptime_max = snaptime;

	}

	/*
	 * All MSI interrupts of a device instance share a single MSI address.
	 * On X86 systems with an APIC, this MSI address is interpreted as CPU
	 * routing info by the APIC.  For this reason, on these platforms, all
	 * interrupts for MSI devices must be moved to the same CPU at the same
	 * time.
	 *
	 * Since all interrupts will be on the same CPU on these platforms, all
	 * interrupts can be consolidated into one ivec entry.  For such
	 * devices, num_ino will be > 1 to denote that a group move is needed.
	 *
	 * Loop thru all MSI devices on X86 pcplusmp systems. Nop on other
	 * systems.
	 */
	for (msi = list_head(&msi_list); msi != NULL;
	    msi = list_next(&msi_list, msi)) {
		intrd_msi_ino_t	*msi_ino;
		intrd_ivec_t	*first_ivec = NULL;
		intrd_ivec_t	*curr_ivec;
		int64_t	first_ino = -1;	/* XXX inode type */

		/*
		 * Loop thru inos of the device, sorted by lowest value first
		 * For each ivec found for a device, incr num_ino for the
		 * lowest ivec and remove other ivecs.
		 *
		 * Assumes PIL is the same for first and current ivec.
		 */

		for (msi_ino = list_head(&msi->im_ino_list); msi_ino != NULL;
		    msi_ino = list_next(&msi->im_ino_list, msi_ino)) {

			curr_ivec = msi_ino->imi_ivec;
			if (first_ino == -1) {
				first_ino = msi_ino->imi_ino;
				first_ivec = curr_ivec;
			} else {
				first_ivec->ii_num_ino++;
				first_ivec->ii_time += curr_ivec->ii_time;
				if (curr_ivec->ii_crtime >
				    first_ivec->ii_crtime) {
					first_ivec->ii_crtime =
					    curr_ivec->ii_crtime;
				}

				/* XXX (Valid?)
				 * Invalidate this cookie, less complicated and
				 * more efficient than deleting it.
				 */
				curr_ivec->ii_num_ino = 0;
			}

		}
	}

	/*
	 * We define the timerange as the amount of time spent gathering the
	 * various kstats, divided by our sleeptime. If we take a lot of time
	 * to access the kstats, and then we create a delta comparing these
	 * kstats with a prior set of kstats, that delta will cover
	 * substaintially different amount of time depending upon which
	 * interrupt or CPU is being examined.
	 *
	 * By checking the timerange here, we guarantee that any deltas
	 * created from these kstats will contain self-consistent data,
	 * in that all CPUs and interrupts cover a similar span of time.
	 *
	 * timerange_toohi is the upper bound. Any timerange above
	 * this is thrown out as garbage. If the stat is safely within this
	 * bound, we treat the stat as representing an instant in time, rather
	 * than the time range it actually spans. We arbitrarily choose minsnap
	 * as the snaptime of the stat.
	 */
	stat->is_snaptime = snaptime_min;
	timerange = (snaptime_max - snaptime_min) / sleeptime;	/* XXX div by zero */
	if (timerange > timerange_toohi) {
		/* Failure */
		syslog(LOG_DEBUG, "intrd_kstat_get: timerange %f fail", timerange);
		// XXX goto error;
	}

	return (stat);

error:
	syslog(LOG_DEBUG, "intrd_kstat_get: failed with error");

	intrd_kstat_free(stat);
	return (NULL);
}

/*
 * Print a snapshot delta.
 */
void
intrd_delta_dump(intrd_delta_t *delta)
{
	intrd_cpu_t	*cpu;
	intrd_ivec_t	*ivec;

	if (delta == NULL)
		return;

	syslog(LOG_DEBUG, "dumpdelta:");

	if (delta->id_missing)
		syslog(LOG_DEBUG, " RECONFIGURATION IN DELTA");

	syslog(LOG_DEBUG, " avgintrload: %5.2f%%  avgintrnsec: %d",
	    delta->id_avg_intr_load * 100, delta->id_avg_intr_nsec);

	if (delta->id_goodness != 0) {
		syslog(LOG_DEBUG, "    goodness: %5.2f%%",
		    delta->id_goodness * 100);
	}	

	for (cpu = list_head(&delta->id_cpu_list); cpu != NULL;
	    cpu = list_next(&delta->id_cpu_list, cpu)) {

		syslog(LOG_DEBUG, "    cpu %3d intr %7.3f%%  (bigintr %7.3f%%)",
		    cpu->ic_instance, cpu->ic_intr_load * 100,
		    cpu->ic_big_intrs * 100 / cpu->ic_tot); /* XXX big_intr vs. big_intrs */
		syslog(LOG_DEBUG, "        intrs %d, bigintr %d",
		    cpu->ic_intrs, cpu->ic_big_intrs);

		for (ivec = list_head(&cpu->ic_ivec_list); ivec != NULL;
		    ivec = list_next(&cpu->ic_ivec_list, ivec)) {

			syslog(LOG_DEBUG, "    %15s:\"%s\": %7.3f%%  %d",
			    ivec->ii_ihs > 1 ? "XXX" : ivec->ii_name, "XXX",
			    ivec->ii_time * 100 / cpu->ic_tot, ivec->ii_time);
		}
	}
}

/*
 * Generate the delta between to statistics snapshots.
 */
intrd_delta_t *
intrd_delta_generate(intrd_stat_t *stat, intrd_stat_t *new_stat)
{
	intrd_cpu_t	*cpu, *new_cpu;
	intrd_delta_t	*delta;
	float		intr_load = 0;
	uint64_t	intr_nsec = 0;
	uint_t		cpus = 0;

	delta = malloc(sizeof (intrd_delta_t));
	if (delta == NULL)
		/* XXX */
		return (delta);

	delta->id_snaptime_min = stat->is_snaptime;
	delta->id_snaptime_max = new_stat->is_snaptime;
	delta->id_missing = B_FALSE;

	list_create(&delta->id_cpu_list, sizeof (intrd_cpu_t),
	    offsetof(intrd_cpu_t, ic_next));

	if (intrd_verify(delta->id_snaptime_max > delta->id_snaptime_min,
	    "intrd_delta: stats aren't ascending")) {
		delta->id_missing = B_TRUE;
		return (NULL);
	}

	/* Number of CPUs must be the same */
	if (stat->is_number_cpus != new_stat->is_number_cpus) {
		syslog(LOG_DEBUG, "intrd_delta: number of CPUs changed");
		return (NULL);
	}

	for (new_cpu = list_head(&new_stat->is_cpu_list),
	    cpu = list_head(&stat->is_cpu_list); new_cpu != NULL && cpu != NULL;
	    new_cpu = list_next(&new_stat->is_cpu_list, new_cpu),
	    cpu = list_next(&stat->is_cpu_list, cpu)) {
		intrd_cpu_t	*delta_cpu;
		intrd_ivec_t	*new_ivec;

		/*
		 * Check for new onlined CPUs.
		 */
		if (intrd_verify(cpu->ic_crtime == new_cpu->ic_crtime,
		    "intrd_delta: cpu XXX changed")) {
			delta->id_missing = B_TRUE;
			// delete "delta cpu"
			return (delta);
		}

		delta_cpu = intrd_cpu_create(new_cpu->ic_instance);	/* XXX do we need instance at all? */
		if (delta_cpu == NULL)
			return (NULL);	/* XXX */

		list_insert_tail(&delta->id_cpu_list, delta_cpu);

		delta_cpu->ic_tot = new_cpu->ic_tot - cpu->ic_tot;
/* XXX must be unsigned
		if (intrd_verify(delta_cpu->ic_tot >= 0,
		     "generate_delta: deltas are not ascending?")) {
			delta->id_missing = B_TRUE;
			free(delta_cpu);
			return (delta);
		}
*/
		/* Avoid chance of division by zero */
		if (delta_cpu->ic_tot == 0)
			delta_cpu->ic_tot = 1;

		delta_cpu->ic_intrs = 0;
		delta_cpu->ic_big_intrs = 0;	/* XXX already set to zero */

/*
		if (intrd_verify(stat->is_number_ivecs == new_stat->is_number_ivecs,
		    "generate_delta: cpu XXX has more/less interrupts")) {
			delta->id_missing = 1;
			return (delta);
		}
*/

		for (new_ivec = list_head(&new_cpu->ic_ivec_list);
		    new_ivec != NULL; new_ivec =
		    list_next(&new_cpu->ic_ivec_list, new_ivec)) {

			intrd_ivec_t	*ivec;
			intrd_ivec_t	*delta_ivec;
			int64_t		time;	/* XXX type of time? */

			if (new_ivec->ii_num_ino == 0)
				continue;

			ivec = intrd_ivec_find_ino(cpu->ic_ivec_list,
			    new_ivec->ii_ino);
			if (ivec == NULL)
				delta->id_missing = 1;
				return (delta);

			delta_ivec = intrd_ivec_create(new_ivec->ii_buspath,
			    new_ivec->ii_ino);

			time = new_ivec->ii_time - ivec->ii_time;
			if (intrd_verify(time >= 0,
			    "XXX: ivec went backwards?")) {
				delta->id_missing = B_TRUE;
				free(delta_ivec);
				return (delta);
			}

			delta_cpu->ic_intrs += time;
			delta_ivec->ii_time = time;
			if (time > delta_cpu->ic_big_intrs) 	/* XXX intr/intrs */
				delta_cpu->ic_big_intrs = time;

			delta_ivec->ii_pil = new_ivec->ii_pil;
			delta_ivec->ii_name = new_ivec->ii_name;
			delta_ivec->ii_ihs = new_ivec->ii_ihs;
			delta_ivec->ii_num_ino = new_ivec->ii_num_ino;
		}

		if (delta_cpu->ic_tot < delta_cpu->ic_intrs) {
			/* Maybe just a rounding error */
			delta_cpu->ic_tot = delta_cpu->ic_intrs;
		}

		delta_cpu->ic_intr_load = delta_cpu->ic_intrs /
		    delta_cpu->ic_tot;
		intr_load += delta_cpu->ic_intr_load;
		intr_nsec += delta_cpu->ic_intrs;
		cpus++;
	}

	if (cpus > 0) {
		delta->id_avg_intr_load = intr_load / cpus;
		delta->id_avg_intr_nsec = intr_nsec / cpus;
	} else {
		delta->id_avg_intr_load = 0;
		delta->id_avg_intr_nsec = 0;
	}

	return (delta);
}

intrd_delta_t *
intrd_delta_compress(list_t delta_list)
{
	intrd_cpu_t	*cpu, *new_cpu;
	intrd_ivec_t	*ivec, *new_ivec;
	intrd_delta_t	*delta;
	intrd_delta_t	*new_delta;

	hrtime_t	tot = 0;
	uint64_t	intrs = 0;
	processorid_t	cpus = 0;
	float		high_intrload = 0;

	if (intrd_verify(list_is_empty(&delta_list) != 0,
	    "list of deltas is empty?"))
		return (NULL);

	new_delta = malloc(sizeof (intrd_delta_t));

	delta = list_head(&delta_list);
	new_delta->id_snaptime_min = delta->id_snaptime_min;
	delta = list_tail(&delta_list);
	new_delta->id_snaptime_max = delta->id_snaptime_max;
	new_delta->id_missing = B_FALSE;

	for (delta = list_head(&delta_list); delta != NULL;
	    delta = (delta_list, delta)) {

		if (intrd_verify(delta->id_missing == B_FALSE,
		    "contains bad delta?")) {
			return (NULL);
		}

		for (cpu = list_head(&delta->id_cpu_list); cpu != NULL;
		    cpu = list_next(&delta->id_cpu_list, cpu)) {

			intrs += cpu->ic_intrs;
			tot += cpu->ic_tot;

			new_cpu = intrd_cpu_create(cpu->ic_instance);
			new_cpu->ic_intrs += cpu->ic_intrs;
			new_cpu->ic_tot += cpu->ic_tot;

			list_insert_tail(&new_delta->id_cpu_list, new_cpu);

			/* XXX */

				for (ivec = list_head(&cpu->ic_ivec_list);
				    ivec != NULL; ivec = list_next(
				    &cpu->ic_ivec_list, ivec)) {

					new_ivec = intrd_ivec_create(
					    ivec->ii_buspath, ivec->ii_ino);

					new_ivec->ii_time += ivec->ii_time;
					new_ivec->ii_pil = ivec->ii_pil;
					new_ivec->ii_name = ivec->ii_name;
					new_ivec->ii_ihs = ivec->ii_ihs;
					new_ivec->ii_num_ino =
					    ivec->ii_num_ino;
				}
		}
	}

	for (cpu = list_head(&new_delta->id_cpu_list); cpu != NULL;
	    cpu = list_next(&new_delta->id_cpu_list, cpu)) {
		uint64_t big_intr = 0;

		cpus++;

		/* Find maximum ivec time */
		for (ivec = list_head(&cpu->ic_ivec_list); ivec != NULL;
		    ivec = list_next(&cpu->ic_ivec_list, ivec)) {
			if (ivec->ii_time > big_intr)
				big_intr = ivec->ii_time;
		}

		cpu->ic_big_intrs = big_intr;	/* XXX !!! big_intr !!! */
		cpu->ic_intr_load = cpu->ic_intrs / cpu->ic_tot;

		if (high_intrload < cpu->ic_intr_load)
			high_intrload = cpu->ic_intr_load;

		if (cpu->ic_tot <= 0)
			cpu->ic_tot = 1;
	}

	if (cpus == 0) {
		new_delta->id_avg_intr_nsec = 0;
		new_delta->id_avg_intr_load = 0;
	} else {
		new_delta->id_avg_intr_nsec = intrs / cpus;
		new_delta->id_avg_intr_load = intrs / tot;
	}

	return (new_delta);
}

static float
intrd_goodness(intrd_delta_t *delta)
{
	intrd_cpu_t	*cpu;
	float		high_goodness = 0.0f;
	float		goodness;

	if (delta->id_missing != B_FALSE)
		return (1);

	for (cpu = list_head(&delta->id_cpu_list); cpu != NULL;
	    cpu = list_next(&delta->id_cpu_list, cpu)) {

		goodness = intrd_goodness_cpu(cpu, delta->id_avg_intr_load);
		if (intrd_verify(goodness >= 0 && goodness <= 1,
		   "goodness is out of range")) {
			if (debug)
				intrd_delta_dump(delta);
			return (1);
		}

		if (goodness == 1)
			return (1);

		if (goodness > high_goodness)
			high_goodness = goodness;
	}

	return (high_goodness);
}

static float
intrd_goodness_cpu(intrd_cpu_t *cpu, float avg_intr_load)
{
	float	load_no_bigintr;
	float	goodness;
	float	load;

	load = cpu->ic_intrs / cpu->ic_tot;

	if (load < avg_intr_load)
		return (0);

	/* XXX !!! big_intr !!! */
	load_no_bigintr = (cpu->ic_intrs - cpu->ic_big_intrs) / cpu->ic_tot;

	if (load > goodness_unsafe_load && cpu->ic_number_ivecs > 1)
		return (1);

	goodness = load - avg_intr_load;
	if (goodness > load_no_bigintr)
		goodness = load_no_bigintr;

	return (goodness);
}

static float
intrd_imbalanced(float goodness, float baseline)
{
	if (goodness > 0.5f)
		return (1);

	if (abs(goodness - baseline) > goodness_min_delta)
		return (1);

	return (0);
}

static void
intr_move_intr(intrd_delta_t *delta, uint64_t inum, processorid_t old_cpuid,
    processorid_t new_cpuid)
{
	intrd_cpu_t	*old_cpu;
	intrd_ivec_t	*ivec;

	old_cpu = intrd_cpu_find(delta->id_cpu_list, old_cpuid);

	ivec = intrd_ivec_find_ino(old_cpu->ic_ivec_list, inum);
	old_cpu->ic_intrs -= ivec->ii_time;
	old_cpu->ic_intr_load = old_cpu->ic_intrs / old_cpu->ic_tot;

	/* XXX delete old_cpu->ic_ivec_list[inum] */

	intrd_verify(old_cpu->ic_intrs >= 0, "interrupt time > total time?");
	/* XXX !!! ic_big_intr !!! */
	intrd_verify(ivec->ii_time <= old_cpu->ic_big_intrs,
	    "interrupt time > big intr time?");
	
	/* XXX !!! ic_big_intr !!! */
	if (ivec->ii_time >= old_cpu->ic_big_intrs) {
		int64_t	bigtime = 0;
		intrd_ivec_t	*new_ivec;

		for (new_ivec = list_head(&old_cpu->ic_ivec_list);
		    new_ivec != NULL; list_next(&old_cpu->ic_ivec_list,
		    new_ivec)) {
			if (new_ivec->ii_time > bigtime)
				bigtime = ivec->ii_time;
		}

		/* XXX !!! ic_big_intr !!! */
		olf_cpu->ic_big_intrs = bigtime;
	}
}

int
intrd_do_reconfig(void)
{
	return (0);
}

void
intrd_clear_deltas(void)
{
}
