/* Copyright (C) 2022 Norihiro Kamae <norihiro@nagater.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/detail/file_parser_error.hpp>
#include <string>
#include <cstdio>
#include "vmxprop.h"

using namespace boost::property_tree;

bool load_ptree(vmx_prop_t &pt, const char *fname)
{
	try {
		read_json(fname, pt);
		return true;
	} catch (json_parser_error &e) {
		fprintf(stderr, "Error: %s\n", e.what());
		return false;
	}
}
