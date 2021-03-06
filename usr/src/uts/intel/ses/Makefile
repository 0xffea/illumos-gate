#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# uts/intel/ses/Makefile
#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
# Copyright (c) 2011 Bayard G. Bell. All rights reserved.
#
#	This makefile drives the production of the ses (SCSI enclosure
#	services) driver kernel module.
#
#	intel implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ses
OBJECTS		= $(SES_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SES_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/intel/io/scsi/targets
WARLOCK_OUT     = $(SES_OBJS:%.o=%.ll)
WARLOCK_OK      = $(MODULE).ok
WLCMD_DIR	= $(UTSBASE)/common/io/warlock

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)
CLEANFILES	+= $(WARLOCK_TARGETS)

#
# For now, disable these lint checks; maintainers should endeavor
# to investigate and remove these for maximum lint coverage.
# Please do not carry these forward to new Makefiles.
#
LINTTAGS	+= -erroff=E_ASSIGN_NARROW_CONV
LINTTAGS	+= -erroff=E_BAD_PTR_CAST_ALIGN

#
# Depends on scsi
#
LDFLAGS         += -dy -N misc/scsi

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)
		$(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

clobber:	$(CLOBBER_DEPS)
		$(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ


#
# Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch
TEST		= test

#
# Warlock targets
#
SCSI_FILES	= $(SCSI_OBJS:%.o=-l ../scsi/%.ll)

WARLOCK_TARGETS	= $(WARLOCK_OK)
$(CLOSED_BUILD)WARLOCK_TARGETS += warlock_with_glm

warlock: $(WARLOCK_TARGETS)

# XXX FIX ME: why only ses.ll?

$(WARLOCK_OK): $(WLCMD_DIR)/$(MODULE).wlcmd $(WARLOCK_OUT)
	@cd ../warlock; $(MAKE) warlock
	@cd ../scsi; $(MAKE) warlock
	$(WARLOCK) -c $(WLCMD_DIR)/$(MODULE).wlcmd ses.ll \
		../warlock/scsi.ll \
	        -l ../warlock/ddi_dki_impl.ll  \
	        $(SCSI_FILES)
	@ $(TOUCH) $@

%.ll: $(UTSBASE)/common/io/scsi/targets/%.c
	$(WLCC) $(CPPFLAGS) -o $@ $<

warlock_with_glm: $(WLCMD_DIR)/ses_with_glm.wlcmd $(WARLOCK_OUT) glm_files
	@cd ../warlock; $(MAKE) warlock
	@cd ../scsi; $(MAKE) warlock
	@cd $(CLOSED)/uts/intel/glm; $(MAKE) warlock;
	$(WARLOCK) -c $(WLCMD_DIR)/ses_with_glm.wlcmd ses.ll \
		$(CLOSED)/uts/intel/glm/*.ll \
	        $(SCSI_FILES) \
		../warlock/scsi.ll \
		-l ../warlock/ddi_dki_impl.ll 

glm_files:
	@cd $(CLOSED)/uts/intel/glm; pwd; $(MAKE) warlock

warlock_ddi.files:
	@cd ../warlock; pwd; $(MAKE) warlock


