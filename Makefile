#
#  Copyright (C) 2011 Andreas Öman
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


WITH_CURL        := yes
WITH_HTTP_SERVER := yes
WITH_CTRLSOCK    := yes

BUILDDIR = ${CURDIR}/build

PROG=${BUILDDIR}/hapd


SRCS =  src/main.c \
	src/hap_input.c \
	src/hap_output.c \
	src/timestore.c \
	src/influxdb.c \
	src/zway.c \
	src/cron.c \
	src/channel.c \
	src/sun.c \

LDFLAGS += -lm


install: ${PROG}
	install -D ${PROG} "${prefix}/bin/hapd"
uninstall:
	rm -f "${prefix}/bin/hapd"

include libsvc/libsvc.mk
-include config.local
-include $(DEPS)
