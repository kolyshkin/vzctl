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

#ifndef _LIST_H_
#define _LIST_H_


struct list_head {
	struct list_head *prev, *next;
};
typedef struct list_head list_head_t;
typedef struct list_head list_elem_t;

struct str_struct {
	list_elem_t list;
	char *val;
};

typedef struct str_struct str_param;

static inline void list_head_init(list_head_t *head)
{
	head->next = head;
	head->prev = head;
}

static inline void list_elem_init(list_elem_t *entry)
{
	entry->next = entry;
	entry->prev = entry;
}

static inline void list_add(list_elem_t *new, list_head_t *list)
{
	new->next = list->next;
	new->prev = list;
	list->next->prev = new;
	list->next = new;
}

static inline void list_add_tail(list_elem_t *new, list_head_t *list)
{
	new->next = list;
	new->prev = list->prev;
	list->prev->next = new;
	list->prev = new;
}

static inline void list_del(list_elem_t *el)
{
	el->prev->next = el->next;
	el->next->prev = el->prev;
	el->prev = (void *)0x5a5a5a5a;
	el->next = (void *)0xa5a5a5a5;
}

static inline void list_del_init(list_elem_t *el)
{
	el->prev->next = el->next;
	el->next->prev = el->prev;
	list_elem_init(el);
}

static inline int list_is_init(list_head_t *h)
{
	return h->next == NULL;
}

static inline int list_empty(list_head_t *h)
{
	if (list_is_init(h))
		return 1;
	return h->next == h;
}

static inline int list_elem_inserted(list_elem_t *el)
{
	return el->next != el;
}

static inline void list_moveall(list_head_t *src, list_head_t *dst)
{
	list_add(dst, src);
	list_del(src);
	list_head_init(src);
}

#define LIST_HEAD(name) \
	list_head_t name = { (void *) &name, (void *) &name }

#define list_entry(ptr, type, field)					\
	((type *)(void *)((char *)(ptr)-(unsigned long)(&((type *)0)->field)))

#define list_first_entry(head, type, field)				\
	list_entry((head)->next, type, field)

#define list_for_each(entry, head, field)				\
	for (entry = list_entry((head)->next, typeof(*entry), field);\
	     &entry->field != (head);					\
	     entry = list_entry(entry->field.next, typeof(*entry), field))

#define list_for_each_prev(entry, head, field)				\
	for (entry = list_entry((head)->prev, typeof(*entry), field);\
	     &entry->field != (head);					\
	     entry = list_entry(entry->field.prev, typeof(*entry), field))

#define list_for_each_safe(entry, tmp, head, field)			\
	for (entry = list_entry((head)->next, typeof(*entry), field),\
		tmp = list_entry(entry->field.next, typeof(*entry), field); \
	     &entry->field != (head);					\
	     entry = tmp,						\
	     tmp = list_entry(tmp->field.next, typeof(*tmp), field))


char *list2str_c(const char *name, char c, list_head_t *head);
char *list2str(const char *name, list_head_t *head);
int add_str_param(list_head_t *head, const char *str);
int add_str_param2(list_head_t *head, char *str);
int add_str2list(list_head_t *head, const char *val);
void free_str_param(list_head_t *head);
int copy_str_param(list_head_t *dst, list_head_t *src);
char *find_str(list_head_t *head, const char *val);
int __merge_str_list(int delall, list_head_t *old, list_head_t *add,
	list_head_t *del, list_head_t *merged,
	char* (*find_fn)(list_head_t*, const char*));
int merge_str_list(int delall, list_head_t *old, list_head_t *add,
	list_head_t *del, list_head_t *merged);
#endif /* _LIST_H_ */
