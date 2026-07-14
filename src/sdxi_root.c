// SPDX-License-Identifier: BSD-3-Clause
#include "sdxi_internal.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int sdxi_parse_dev_name(const char *name, int *pf_out, int *vf_out)
{
	char *end;
	long pf;
	long vf;

	if (!name || strncmp(name, "sdxi", 4) != 0)
		return -EINVAL;

	errno = 0;
	pf = strtol(name + 4, &end, 10);
	if (errno || end == name + 4 || pf < 0 || pf > INT_MAX)
		return -EINVAL;

	if (*end == '\0') {
		*pf_out = (int)pf;
		*vf_out = -1;
		return 0;
	}

	if (*end != 'v')
		return -EINVAL;

	errno = 0;
	vf = strtol(end + 1, &end, 10);
	if (errno || end == NULL || *end != '\0' || vf < 0 || vf > INT_MAX)
		return -EINVAL;

	*pf_out = (int)pf;
	*vf_out = (int)vf;
	return 0;
}

static int sdxi_root_dev_ent_cmp(const void *a, const void *b)
{
	const struct sdxi_root_dev_ent *aa = a;
	const struct sdxi_root_dev_ent *bb = b;

	if (aa->pf != bb->pf)
		return (aa->pf > bb->pf) - (aa->pf < bb->pf);
	return (aa->vf > bb->vf) - (aa->vf < bb->vf);
}

static void sdxi_root_free_inventory(struct sdxi_root *root)
{
	size_t i;

	if (!root || !root->devs)
		return;

	for (i = 0; i < root->ndevs; i++)
		free(root->devs[i].path);

	free(root->devs);
	root->devs = NULL;
	root->ndevs = 0;
}

static int sdxi_root_validate_opts(const struct sdxi_root_opts *opts)
{
	size_t i;

	if (!opts)
		return 0;
	if (opts->size < sizeof(*opts))
		return -EINVAL;
	if (opts->flags != 0)
		return -EINVAL;

	for (i = 0; i < sizeof(opts->rsvd) / sizeof(opts->rsvd[0]); i++) {
		if (opts->rsvd[i] != 0)
			return -EINVAL;
	}

	if (!opts->uri || opts->uri[0] == '\0')
		return 0;
	if (strcmp(opts->uri, SDXI_ROOT_URI_LOCAL) == 0)
		return 0;

	return -ENOTSUP;
}

int sdxi_root_create(const struct sdxi_root_opts *opts,
		     sdxi_root_t **root_out)
{
	sdxi_root_t *root;
	int ret;

	if (!root_out)
		return -EINVAL;

	*root_out = NULL;

	ret = sdxi_root_validate_opts(opts);
	if (ret)
		return ret;

	root = calloc(1, sizeof(*root));
	if (!root)
		return -ENOMEM;

	root->backend = SDXI_BACKEND_LOCAL;
	*root_out = root;
	return 0;
}

void sdxi_root_destroy(sdxi_root_t *root)
{
	if (!root)
		return;

	sdxi_root_free_inventory(root);
	free(root);
}

static int sdxi_root_scan_local(struct sdxi_root_dev_ent **devs_out,
				size_t *ndevs_out)
{
	struct dirent *de;
	struct sdxi_root_dev_ent *devs = NULL;
	size_t ndevs = 0;
	size_t cap = 0;
	DIR *dir;
	int ret = 0;

	if (!devs_out || !ndevs_out)
		return -EINVAL;

	*devs_out = NULL;
	*ndevs_out = 0;

	dir = opendir("/dev");
	if (!dir)
		return -errno;

	while ((de = readdir(dir)) != NULL) {
		size_t name_len;
		int pf;
		int vf;

		if (sdxi_parse_dev_name(de->d_name, &pf, &vf) != 0)
			continue;

		if (ndevs == cap) {
			size_t new_cap = cap ? cap * 2 : 16;
			struct sdxi_root_dev_ent *new_devs;

			new_devs = realloc(devs, new_cap * sizeof(*new_devs));
			if (!new_devs) {
				ret = -ENOMEM;
				goto out;
			}

			devs = new_devs;
			cap = new_cap;
		}

		name_len = strlen(de->d_name);
		devs[ndevs].path = malloc(name_len + sizeof("/dev/"));
		if (!devs[ndevs].path) {
			ret = -ENOMEM;
			goto out;
		}

		memcpy(devs[ndevs].path, "/dev/", sizeof("/dev/") - 1);
		memcpy(devs[ndevs].path + (sizeof("/dev/") - 1), de->d_name,
		       name_len + 1);

		devs[ndevs].pf = pf;
		devs[ndevs].vf = vf;
		ndevs++;
	}

	if (ndevs > 1)
		qsort(devs, ndevs, sizeof(*devs), sdxi_root_dev_ent_cmp);

	*devs_out = devs;
	*ndevs_out = ndevs;
	devs = NULL;
	ret = 0;

out:
	if (dir)
		closedir(dir);
	if (devs) {
		size_t i;

		for (i = 0; i < ndevs; i++)
			free(devs[i].path);
		free(devs);
	}

	return ret;
}

int sdxi_root_scan(sdxi_root_t *root)
{
	struct sdxi_root_dev_ent *new_devs = NULL;
	size_t new_ndevs = 0;
	int ret;

	if (!root)
		return -EINVAL;
	if (root->backend != SDXI_BACKEND_LOCAL)
		return -ENOTSUP;

	ret = sdxi_root_scan_local(&new_devs, &new_ndevs);
	if (ret)
		return ret;

	sdxi_root_free_inventory(root);
	root->devs = new_devs;
	root->ndevs = new_ndevs;
	return 0;
}

size_t sdxi_root_dev_count(const sdxi_root_t *root)
{
	if (!root)
		return 0;
	return root->ndevs;
}

const char *sdxi_root_dev_path(const sdxi_root_t *root, size_t index)
{
	if (!root || index >= root->ndevs)
		return NULL;

	return root->devs[index].path;
}

int sdxi_root_dev_open(const sdxi_root_t *root, size_t index,
		       sdxi_dev_t **dev_out)
{
	const char *path;

	if (!root || !dev_out)
		return -EINVAL;

	path = sdxi_root_dev_path(root, index);
	if (!path)
		return -ENOENT;

	return sdxi_dev_open(path, dev_out);
}

int sdxi_list_devs(char ***paths_out, size_t *count_out)
{
	sdxi_root_t *root = NULL;
	char **paths = NULL;
	size_t count;
	size_t i;
	int ret;

	if (!paths_out || !count_out)
		return -EINVAL;

	*paths_out = NULL;
	*count_out = 0;

	ret = sdxi_root_create(NULL, &root);
	if (ret)
		return ret;

	ret = sdxi_root_scan(root);
	if (ret)
		goto out;

	count = sdxi_root_dev_count(root);
	if (count == 0) {
		ret = 0;
		goto out;
	}

	paths = calloc(count + 1, sizeof(*paths));
	if (!paths) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < count; i++) {
		const char *path = sdxi_root_dev_path(root, i);

		paths[i] = strdup(path);
		if (!paths[i]) {
			ret = -ENOMEM;
			goto out;
		}
	}

	*paths_out = paths;
	*count_out = count;
	paths = NULL;
	ret = 0;

out:
	if (paths) {
		for (i = 0; paths[i] != NULL; i++)
			free(paths[i]);
		free(paths);
	}
	sdxi_root_destroy(root);
	return ret;
}

void sdxi_free_dev_list(char **paths, size_t count)
{
	size_t i;

	if (!paths)
		return;

	for (i = 0; i < count; i++)
		free(paths[i]);
	free(paths);
}
