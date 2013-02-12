#ifdef HAVE_PLOOP
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

struct vzctl_snapshot_param;

#define SNAPSHOT_XML	"Snapshots.xml"
#define GET_SNAPSHOT_XML(buf, ve_private) \
	snprintf(buf, sizeof(buf), "%s/" SNAPSHOT_XML, ve_private);

#define GET_SNAPSHOT_XML_TMP(buf, ve_private) \
	snprintf(buf, sizeof(buf), "%s/" SNAPSHOT_XML ".tmp", ve_private);

/* src/lib/snapshot.c */
void vzctl_free_snapshot_tree(struct vzctl_snapshot_tree *tree);
struct vzctl_snapshot_tree *vzctl_alloc_snapshot_tree(void);
int vzctl_add_snapshot_tree_entry(struct vzctl_snapshot_tree *tree, int current,
		const char *guid, const char *parent_guid, const char *name,
		const char *date, const char *desc);
void vzctl_del_snapshot_tree_entry(struct vzctl_snapshot_tree *tree, const char *guid);
int vzctl_add_snapshot(struct vzctl_snapshot_tree *tree, const char *guid,
		const struct vzctl_snapshot_param *param);
void vzctl_snapshot_tree_set_current(struct vzctl_snapshot_tree *tree, const char *guid);
int vzctl_find_snapshot_by_guid(struct vzctl_snapshot_tree *tree, const char *guid);

/* src/lib/xml.c */
int vzctl_read_snapshot_tree(const char *fname, struct vzctl_snapshot_tree *tree);
int vzctl_store_snapshot_tree(const char *fname, struct vzctl_snapshot_tree *tree);

/* src/snapshot.c */
int is_snapshot_supported(const char *ve_private);
int vzctl_env_create_snapshot(vps_handler *h, envid_t veid, const fs_param *fs,
		const struct vzctl_snapshot_param *param);

int vzctl_env_switch_snapshot(vps_handler *h, envid_t veid, vps_param *g_p,
		const fs_param *fs, const char *guid);

int vzctl_env_delete_snapshot(vps_handler *h, envid_t veid, const fs_param *fs,
		const char *guid);

/* src/snapshot-list.c */
int vzctl_env_snapshot_list(int argc, char **argv, int envid,
		const char *ve_private);
#endif /* __SNAPSHOT_H__ */
#endif /* HAVE_PLOOP */
