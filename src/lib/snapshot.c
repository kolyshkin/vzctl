#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "snapshot.h"
#include "vzerror.h"
#include "logger.h"

static void remove_data_from_array(void **array, int nelem, int id)
{
	int i;

	for (i = id; i < nelem - 1; i++)
		array[i] = array[i + 1];
}

static void free_snapshot_data(struct vzctl_snapshot_data *data)
{
	free(data->guid);
	free(data->parent_guid);
	free(data->name);
	free(data->date);
	free(data->desc);
	free(data);
}

void vzctl_free_snapshot_tree(struct vzctl_snapshot_tree *tree)
{
	int i;

	for (i = 0; i < tree->nsnapshots; i++)
		free_snapshot_data(tree->snapshots[i]);
	free(tree->snapshots);
	free(tree);
}

struct vzctl_snapshot_tree *vzctl_alloc_snapshot_tree(void)
{
	return calloc(1, sizeof(struct vzctl_snapshot_tree));
}

int vzctl_find_snapshot_by_guid(struct vzctl_snapshot_tree *tree, const char *guid)
{
	int i;

	for (i = 0; i < tree->nsnapshots; i++)
		if (strcmp(tree->snapshots[i]->guid, guid) == 0)
			return i;
	return -1;
}

static int find_snapshot_current(struct vzctl_snapshot_tree *tree)
{
	int i;

	for (i = 0; i < tree->nsnapshots; i++)
		if (tree->snapshots[i]->current)
			return i;
	return -1;
}

void vzctl_snapshot_tree_set_current(struct vzctl_snapshot_tree *tree, const char *guid)
{
	int i;

	for (i = 0; i < tree->nsnapshots; i++) {
		tree->snapshots[i]->current = 0;
		if (strcmp(tree->snapshots[i]->guid, guid) == 0)
			tree->snapshots[i]->current = 1;
	}
}

void vzctl_del_snapshot_tree_entry(struct vzctl_snapshot_tree *tree, const char *guid)
{
	int id, i;
	struct vzctl_snapshot_data *snap;

	id = vzctl_find_snapshot_by_guid(tree, guid);
	if (id == -1)
		return;
	snap = tree->snapshots[id];

	for (i = 0; i < tree->nsnapshots; i++) {
		if (snap->current) {
			// set new current
			if (strcmp(tree->snapshots[i]->guid, snap->parent_guid) == 0) {
				tree->snapshots[i]->current = 1;
				break;
			}
		} else {
			// update parent
			if (strcmp(tree->snapshots[i]->parent_guid, guid) == 0)
				strcpy(tree->snapshots[i]->parent_guid, snap->parent_guid);
		}
	}

	free_snapshot_data(snap);
	remove_data_from_array((void**)tree->snapshots, tree->nsnapshots, id);
	tree->nsnapshots--;
}

int vzctl_add_snapshot_tree_entry(struct vzctl_snapshot_tree *tree, int current, const char *guid,
		const char *parent_guid, const char *name, const char *date,
		const char *desc)
{
	struct vzctl_snapshot_data **tmp;
	struct vzctl_snapshot_data *data;

	if (vzctl_find_snapshot_by_guid(tree, guid) != -1)
		return vzctl_err(VZ_INVALID_PARAMETER_VALUE, 0,
				"Invalid guid %s specified: already exists", guid);

	data = calloc(1, sizeof(struct vzctl_snapshot_data));
	if (data == NULL)
		return vzctl_err(VZ_RESOURCE_ERROR, ENOMEM, "calloc failed");

	tmp = realloc(tree->snapshots, sizeof(struct vzctl_snapshot_data *) * (tree->nsnapshots+1));
	if (tmp == NULL) {
		free(data);
		return vzctl_err(VZ_RESOURCE_ERROR, ENOMEM, "realloc failed");
	}
	tree->snapshots = tmp;
	data->guid = strdup(guid);
	data->parent_guid = strdup(parent_guid);
	data->name = strdup(name ? name : "");
	data->date = strdup(date ? date : "");
	data->desc = strdup(desc ? desc : "");

	if (data->guid == NULL || data->parent_guid == NULL) {
		free_snapshot_data(data);
		return vzctl_err(VZ_RESOURCE_ERROR, ENOMEM, "strdup failed");
	}
	if (current) {
		int i;
		// set new current
		for (i = 0; i < tree->nsnapshots; i++)
			tree->snapshots[i]->current = 0;
		data->current = 1;
	}

	tree->snapshots[tree->nsnapshots] = data;
	tree->nsnapshots++;

	return 0;
}

static const char *get_date(char *buf, int len)
{
	struct tm *p_tm_time;
	time_t ptime;

	ptime = time(NULL);
	p_tm_time = localtime(&ptime);
	strftime(buf, len, "%F %T", p_tm_time);

	return buf;
}

int vzctl_add_snapshot(struct vzctl_snapshot_tree *tree, const char *guid,
		const struct vzctl_snapshot_param *param)
{
	int i;
	char *parent_guid = "";
	char buf[64];

	i = find_snapshot_current(tree);
	if (i != -1)
		parent_guid = tree->snapshots[i]->guid;

	return vzctl_add_snapshot_tree_entry(tree, 1, guid, parent_guid,
			param->name, get_date(buf, sizeof(buf)), param->desc);
}
