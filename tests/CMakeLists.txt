# Copyright (C) 2017-2022 Greenbone Networks GmbH
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

# test-hosts executable

include_directories (${GLIB_INCLUDE_DIRS})

if (BUILD_SHARED)
  add_executable (test-hosts test-hosts.c)
  set_target_properties (test-hosts PROPERTIES LINKER_LANGUAGE C)
  target_link_libraries (test-hosts ${LIBGVM_BASE_NAME} -lm ${GLIB_LDFLAGS})
endif (BUILD_SHARED)

## End
