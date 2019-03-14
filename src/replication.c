#include <stddef.h>

#include <libco.h>
#include <sqlite3.h>

#include "./lib/assert.h"
#include "./lib/logger.h"

#include "command.h"
#include "leader.h"
#include "queue.h"
#include "replication.h"

/* Set to 1 to enable tracing. */
#if 1
#define tracef(MSG, ...) debugf(r->logger, MSG, ##__VA_ARGS__)
#else
#define tracef(MSG, ...)
#endif

static void open_apply_cb(struct raft_apply *req, int status)
{
	struct leader *leader;
	leader = req->data;
	raft_free(req);
	if (status != 0) {
		assert(0); /* TODO */
	}
	co_switch(leader->loop);
}

/* Implementation of the sqlite3_wal_replication interface */
struct replication
{
	struct dqlite_logger *logger;
	struct raft *raft;
	queue apply_reqs;
};

int replication__begin(sqlite3_wal_replication *replication, void *arg)
{
	struct replication *r = replication->pAppData;
	struct leader *leader = arg;
	int rc;

	if (raft_state(r->raft) != RAFT_LEADER) {
		return SQLITE_IOERR_NOT_LEADER;
	}

	if (leader->db->follower == NULL) {
		struct command_open c;
		c.filename = leader->db->filename;
		rc = command__apply(r->raft, COMMAND_OPEN, &c, leader,
				    open_apply_cb);
		if (rc != 0) {
			return rc;
		}
		co_switch(leader->main);
	}

	return SQLITE_OK;
}

int replication__abort(sqlite3_wal_replication *r, void *arg)
{
	(void)r;
	(void)arg;

	return SQLITE_OK;
}

int replication__frames(sqlite3_wal_replication *r,
			void *arg,
			int page_size,
			int n,
			sqlite3_wal_replication_frame *frames,
			unsigned truncate,
			int commit)
{
	return SQLITE_OK;
}

int replication__undo(sqlite3_wal_replication *r, void *arg)
{
	(void)r;
	(void)arg;

	return SQLITE_OK;
}

int replication__end(sqlite3_wal_replication *r, void *arg)
{
	(void)r;
	(void)arg;

	return SQLITE_OK;
}

int replication__init(struct sqlite3_wal_replication *replication,
		      struct dqlite_logger *logger,
		      struct raft *raft)
{
	struct replication *r = sqlite3_malloc(sizeof *r);

	if (r == NULL) {
		return DQLITE_NOMEM;
	}

	r->logger = logger;
	r->raft = raft;
	QUEUE__INIT(&r->apply_reqs);

	replication->iVersion = 1;
	replication->pAppData = r;
	replication->xBegin = replication__begin;
	replication->xAbort = replication__abort;
	replication->xFrames = replication__frames;
	replication->xUndo = replication__undo;
	replication->xEnd = replication__end;

	return 0;
}

void replication__close(struct sqlite3_wal_replication *replication)
{
	struct replication *r = replication->pAppData;
	sqlite3_free(r);
}
