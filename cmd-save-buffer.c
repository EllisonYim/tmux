/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Saves a paste buffer to a file.
 */

enum cmd_retval	 cmd_save_buffer_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_save_buffer_entry = {
	"save-buffer", "saveb",
	"ab:", 1, 1,
	"[-a] " CMD_BUFFER_USAGE " path",
	0,
	NULL,
	NULL,
	cmd_save_buffer_exec
};

enum cmd_retval
cmd_save_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct client		*c = ctx->cmdclient;
	struct session          *s;
	struct paste_buffer	*pb;
	const char		*path, *newpath, *wd;
	char			*cause;
	int			 buffer;
	mode_t			 mask;
	FILE			*f;

	if (!args_has(args, 'b')) {
		if ((pb = paste_get_top(&global_buffers)) == NULL) {
			ctx->error(ctx, "no buffers");
			return (CMD_RETURN_ERROR);
		}
	} else {
		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "buffer %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		pb = paste_get_index(&global_buffers, buffer);
		if (pb == NULL) {
			ctx->error(ctx, "no buffer %d", buffer);
			return (CMD_RETURN_ERROR);
		}
	}

	path = args->argv[0];
	if (strcmp(path, "-") == 0) {
		if (c == NULL) {
			ctx->error(ctx, "%s: can't write to stdout", path);
			return (CMD_RETURN_ERROR);
		}
		evbuffer_add(c->stdout_data, pb->data, pb->size);
		server_push_stdout(c);
	} else {
		if (c != NULL)
			wd = c->cwd;
		else if ((s = cmd_current_session(ctx, 0)) != NULL) {
			wd = options_get_string(&s->options, "default-path");
			if (*wd == '\0')
				wd = s->cwd;
		} else
			wd = NULL;
		if (wd != NULL && *wd != '\0') {
			newpath = get_full_path(wd, path);
			if (newpath != NULL)
				path = newpath;
		}

		mask = umask(S_IRWXG | S_IRWXO);
		if (args_has(self->args, 'a'))
			f = fopen(path, "ab");
		else
			f = fopen(path, "wb");
		umask(mask);
		if (f == NULL) {
			ctx->error(ctx, "%s: %s", path, strerror(errno));
			return (CMD_RETURN_ERROR);
		}
		if (fwrite(pb->data, 1, pb->size, f) != pb->size) {
			ctx->error(ctx, "%s: fwrite error", path);
			fclose(f);
			return (CMD_RETURN_ERROR);
		}
		fclose(f);
	}

	return (CMD_RETURN_NORMAL);
}
