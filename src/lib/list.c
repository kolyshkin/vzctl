/*
 *  Copyright (C) 2000-2011, Parallels, Inc. All rights reserved.
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

#include "stdlib.h"
#include <stdio.h>
#include <string.h>

#include "list.h"

char *list2str_c(const char *name, char c, list_head_t *head)
{
	char *buf = NULL, *tmp;
	int buf_len, len, r;
	char *sp, *ep;
	const int delta = 256;
	struct str_struct *p;

	if (name != NULL)
		buf_len = strlen(name) + 3;
	else
		buf_len = delta;

	buf_len = buf_len < delta ? delta : buf_len + delta;
	buf = malloc(buf_len + 1);
	if (buf == NULL)
		return NULL;
	*buf = 0;
	ep = buf + buf_len;
	sp = buf;
	if (name != NULL) {
		r = sprintf(buf, "%s=", name);
		sp = buf + r;
	}
	if (c)
		sp += sprintf(sp, "%c", c);
	if (list_empty(head)) {
		if (c)
			sprintf(sp, "%c", c);
		return buf;
	}
	list_for_each(p, head, list) {
		if (p->val == NULL)
			continue;
		len = strlen(p->val);
		if (sp + len >= ep - 1) {
			int cur_len = sp - buf;

			buf_len += delta > len ? delta : len + 1;
			tmp = realloc(buf, buf_len);
			if (tmp == NULL) {
				free(buf);
				return NULL;
			}
			buf = tmp;
			ep = buf + buf_len;
			sp = buf + cur_len;
		}
		r = snprintf(sp, ep - sp + 1, "%s ", p->val);
		sp += r;
	}
	if (c)
		sp[-1] = c;
	else
		sp[-1] = 0;
	return buf;
}

char *list2str(const char *name, list_head_t *head)
{
	return list2str_c(name, 0, head);
}

void free_str_param(list_head_t *head)
{
	str_param *cur;

	if (list_empty(head))
		return;
	while(!list_empty(head)) {
		list_for_each (cur, head, list) {
			free(cur->val);
			list_del(&cur->list);
			free(cur);
			break;
		}
	}
	list_head_init(head);
}

int copy_str_param(list_head_t *dst, list_head_t *src)
{
	str_param *str;
	int ret = 0;

	if (list_empty(src))
		return 0;
	list_for_each(str, src, list) {
		if ((ret = add_str_param(dst, str->val))) {
			free_str_param(dst);
			break;
		}
	}
	return ret;
}

static inline struct str_struct *new_str_struct(const char *str)
{
	str_param *p;

	p = malloc(sizeof(*p));
	if (p == NULL)
		return NULL;
	p->val = strdup(str);
	if (p->val == NULL) {
		free(p);
		p = NULL;
	}
	return p;
}

int add_str_param(list_head_t *head, const char *str)
{
	str_param *str_p;

	if (str == NULL)
		return 0;
	if ((str_p = new_str_struct(str)) == NULL)
		return -1;
	if (list_is_init(head))
		list_head_init(head);
	list_add_tail(&str_p->list, head);

	return 0;

}

int add_str_param2(list_head_t *head, char *str)
{
	str_param *str_p;

	if (str == NULL)
		return 0;
	str_p = malloc(sizeof(*str_p));
	if (str_p == NULL)
		return -1;
	str_p->val = str;
	if (list_is_init(head))
		list_head_init(head);
	list_add_tail(&str_p->list, head);

	return 0;
}

char *find_str(list_head_t *head, const char *val)
{
	str_param *str;

	if (list_empty(head))
		return NULL;
	list_for_each(str, head, list) {
		if (!strcmp(str->val, val))
			return str->val;
	}
	return NULL;
}

int add_str2list(list_head_t *head, const char *val)
{
	char *token;
	int ret;
	char *tmp;

	ret = 0;
	if ((tmp = strdup(val)) == NULL)
		return -1;
	if ((token = strtok(tmp, "\t ")) == NULL) {
		free(tmp);
		return 0;
	}
	do {
		if (find_str(head, token))
			continue;
		if ((ret = add_str_param(head, token)))
			break;
	} while ((token = strtok(NULL, "\t ")));
	free(tmp);
	return ret;
}

int __merge_str_list(int delall, list_head_t *old, list_head_t *add,
	list_head_t *del, list_head_t *merged,
	char* (*find_fn)(list_head_t*, const char*))
{
	str_param *str;

	if (!delall && list_empty(add) && list_empty(del))
		return 0;
	if (!delall && !list_empty(old)) {
		/* add old values */
		list_for_each(str, old, list) {
			if (find_fn(add, str->val))
				continue;
			if (find_fn(del, str->val))
				continue;
			add_str_param(merged, str->val);
		}
	}
	if (!list_empty(add)) {
		list_for_each(str, add, list) {
			add_str_param(merged, str->val);
		}
	}
	return 0;
}

int merge_str_list(int delall, list_head_t *old, list_head_t *add,
		list_head_t *del, list_head_t *merged)
{
	return __merge_str_list(delall, old, add, del, merged, find_str);
}
