/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _VZFEATURES_H_
#define _VZFEATURES_H_

struct feature_s {
	char *name;
	int on;
	unsigned long long mask;
};

struct feature_s *find_feature(const char *name);

void features_mask2str(unsigned long long mask, unsigned long long known,
		const char *delim, char *buf, int len);
void print_json_features(unsigned long long mask, unsigned long long known);

#endif //_VZFEATURES_H_
