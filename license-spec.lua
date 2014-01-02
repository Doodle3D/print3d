local M = {
	BASE_PATH = 'src',
	EXCLUDE_FILES = { 'src/Timer%..*' },
	PROCESS_FILES = {
		['src/[^/]*%.[ch]p?p?'] = 'cstyle',
		['src/drivers/[^/]*%.[ch]p?p?'] = 'cstyle',
		['src/frontends/[^/]*%.[ch]p?p?'] = 'cstyle',
		['src/frontends/cmdline/[^/]*%.[ch]p?p?'] = 'cstyle',
		['src/frontends/lua/[^/]*%.[ch]p?p?'] = 'cstyle',
		['src/server/[^/]*%.[ch]p?p?'] = 'cstyle',
		['src/script/[^/]*%.sh'] = 'sh',
		['src/script/print3d_init'] = 'sh'
	},
	IGNORE_GIT_CHANGED = false
}
return M
