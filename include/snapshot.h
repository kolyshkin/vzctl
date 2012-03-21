#ifndef __SNAPSHOT_H__
#define __SNAPSHOT_H__

#include "types.h"
#include "res.h"

struct vzctl_snapshot_data {
	int current;
	char *guid;
	char *parent_guid;
	char *name;
	char *date;
	char *desc;
};

struct vzctl_snapshot_tree {
	struct vzctl_snapshot_data **snapshots;
	int nsnapshots;
};

/* src/lib/xml.c */
int vzctl_read_snapshot_tree(const char *fname, struct vzctl_snapshot_tree *tree);
int vzctl_store_snapshot_tree(const char *fname, struct vzctl_snapshot_tree *tree);

#endif /* __SNAPSHOT_H__ */
