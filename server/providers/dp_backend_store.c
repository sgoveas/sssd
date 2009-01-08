/*
   SSSD

   Data Provider Backend Storage helper funcitons

   Copyright (C) Simo Sorce <ssorce@redhat.com>	2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <errno.h>
#include "ldb.h"
#include "ldb_errors.h"
#include "util/util.h"
#include "providers/dp_backend.h"
#include <time.h>

int dp_be_store_account_posix(struct be_ctx *ctx,
                              char *name, char *pwd,
                              uint64_t uid, uint64_t gid,
                              char *gecos, char *homedir, char *shell)
{
    TALLOC_CTX *tmp_ctx;
    const char *attrs[] = { "uid", NULL };
    struct ldb_dn *account_dn;
    struct ldb_message *msg;
    struct ldb_request *req;
	struct ldb_result *res;
    int lret, ret;
    int flags;

    tmp_ctx = talloc_new(ctx);
    if (!tmp_ctx) {
        return ENOMEM;
    }

    account_dn = ldb_dn_new_fmt(tmp_ctx, ctx->ldb,
                                "uid=%s,cn=users,cn=%s,cn=remote",
                                name, ctx->domain);
    if (!account_dn) {
        talloc_free(tmp_ctx);
        return ENOMEM;
    }

    lret = ldb_transaction_start(ctx->ldb);
    if (lret != LDB_SUCCESS) {
        DEBUG(1, ("Failed ldb transaction start !? (%d)\n", lret));
        ret = EIO;
        goto done;
    }

	res = talloc_zero(tmp_ctx, struct ldb_result);
	if (!res) {
        ret = ENOMEM;
        goto done;
	}

    lret = ldb_build_search_req(&req, ctx->ldb, tmp_ctx,
                                account_dn, LDB_SCOPE_BASE,
                                "(objectClass=User)", attrs, NULL,
                                res, ldb_search_default_callback, NULL);
    if (lret != LDB_SUCCESS) {
        DEBUG(1, ("Failed to build search request (%d) !?\n", lret));
        ret = EIO;
        goto done;
    }

    lret = ldb_request(ctx->ldb, req);
    if (lret == LDB_SUCCESS) {
        lret = ldb_wait(req->handle, LDB_WAIT_ALL);
    }
    if (lret != LDB_SUCCESS) {
        DEBUG(1, ("Failed to make search request: %s(%d)[%s]\n",
                  ldb_strerror(lret), lret, ldb_errstring(ctx->ldb)));
        ret = EIO;
        goto done;
    }

    talloc_free(req);
    req = NULL;

    msg = ldb_msg_new(tmp_ctx);
    if (!msg) {
        ret = ENOMEM;
        goto done;
    }
    msg->dn = account_dn;

    switch (res->count) {
    case 0:
        flags = LDB_FLAG_MOD_ADD;
        break;
    case 1:
        flags = LDB_FLAG_MOD_REPLACE;
        break;
    default:
        DEBUG(0, ("Cache DB corrupted, base search returned %d results\n",
                  res->count));
        ret = EIO;
        goto done;
    }

    talloc_free(res);
    res = NULL;

    if (flags == LDB_FLAG_MOD_ADD) {
        /* TODO: retrieve user objectclass list from configuration */
        lret = ldb_msg_add_empty(msg, "objectClass", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_string(msg, "objectClass", "user");
        }
        if (lret != LDB_SUCCESS) {
            ret = errno;
            goto done;
        }

        /* TODO: retrieve user name attribute from configuration */
        lret = ldb_msg_add_empty(msg, "uid", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_string(msg, "uid", name);
        }
        if (lret != LDB_SUCCESS) {
            ret = errno;
            goto done;
        }
    }

    /* TODO: retrieve attribute name mappings from configuration */

    /* pwd */
    if (pwd && *pwd) {
        lret = ldb_msg_add_empty(msg, "userPassword", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_string(msg, "userPassword", pwd);
        }
    } else {
        lret = ldb_msg_add_empty(msg, "userPassword",
                                 LDB_FLAG_MOD_DELETE, NULL);
    }
    if (lret != LDB_SUCCESS) {
        ret = errno;
        goto done;
    }

    /* uid */
    if (uid) {
        lret = ldb_msg_add_empty(msg, "uidNumber", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_fmt(msg, "uidNumber",
                                   "%lu", (long unsigned)uid);
        }
        if (lret != LDB_SUCCESS) {
            ret = errno;
            goto done;
        }
    } else {
        DEBUG(0, ("Cached users can't have UID == 0\n"));
        ret = EINVAL;
        goto done;
    }

    /* gid */
    if (gid) {
        lret = ldb_msg_add_empty(msg, "gidNumber", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_fmt(msg, "gidNumber",
                                   "%lu", (long unsigned)gid);
        }
        if (lret != LDB_SUCCESS) {
            ret = errno;
            goto done;
        }
    } else {
        DEBUG(0, ("Cached users can't have GID == 0\n"));
        ret = EINVAL;
        goto done;
    }

    /* gecos */
    if (gecos && *gecos) {
        lret = ldb_msg_add_empty(msg, "fullName", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_string(msg, "fullName", gecos);
        }
    } else {
        lret = ldb_msg_add_empty(msg, "fullName",
                                 LDB_FLAG_MOD_DELETE, NULL);
    }
    if (lret != LDB_SUCCESS) {
        ret = errno;
        goto done;
    }

    /* homedir */
    if (homedir && *homedir) {
        lret = ldb_msg_add_empty(msg, "homeDirectory", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_string(msg, "homeDirectory", homedir);
        }
    } else {
        lret = ldb_msg_add_empty(msg, "homeDirectory",
                                 LDB_FLAG_MOD_DELETE, NULL);
    }
    if (lret != LDB_SUCCESS) {
        ret = errno;
        goto done;
    }

    /* shell */
    if (shell && *shell) {
        lret = ldb_msg_add_empty(msg, "loginShell", flags, NULL);
        if (lret == LDB_SUCCESS) {
            lret = ldb_msg_add_string(msg, "loginShell", shell);
        }
    } else {
        lret = ldb_msg_add_empty(msg, "loginShell",
                                 LDB_FLAG_MOD_DELETE, NULL);
    }
    if (lret != LDB_SUCCESS) {
        ret = errno;
        goto done;
    }

    /* modification time */
    lret = ldb_msg_add_empty(msg, "lastUpdate", flags, NULL);
    if (lret == LDB_SUCCESS) {
        lret = ldb_msg_add_fmt(msg, "lastUpdate",
                               "%ld", (long int)time(NULL));
    }
    if (lret != LDB_SUCCESS) {
        ret = errno;
        goto done;
    }

    if (flags == LDB_FLAG_MOD_ADD) {
        lret = ldb_build_add_req(&req, ctx->ldb, tmp_ctx, msg, NULL,
                                 NULL, ldb_op_default_callback, NULL);
    } else {
        lret = ldb_build_mod_req(&req, ctx->ldb, tmp_ctx, msg, NULL,
                                 NULL, ldb_op_default_callback, NULL);
    }
    if (lret == LDB_SUCCESS) {
        lret = ldb_request(ctx->ldb, req);
        if (lret == LDB_SUCCESS) {
            lret = ldb_wait(req->handle, LDB_WAIT_ALL);
        }
    }
    if (lret != LDB_SUCCESS) {
        DEBUG(1, ("Failed to make modify request: %s(%d)[%s]\n",
                  ldb_strerror(lret), lret, ldb_errstring(ctx->ldb)));
        ret = EIO;
        goto done;
    }

    lret = ldb_transaction_commit(ctx->ldb);
    if (lret != LDB_SUCCESS) {
        DEBUG(1, ("Failed ldb transaction start !? (%d)\n", lret));
        ret = EIO;
        goto done;
    }

    ret = EOK;

done:
    if (ret != EOK) {
        lret = ldb_transaction_cancel(ctx->ldb);
        if (lret != LDB_SUCCESS) {
            DEBUG(1, ("Failed to cancel ldb transaction (%d)\n", lret));
        }
    }

    talloc_free(tmp_ctx);
    return ret;
}

int dp_be_remove_account_posix(struct be_ctx *ctx, char *name)
{
    TALLOC_CTX *tmp_ctx;
    struct ldb_dn *account_dn;
    int ret;

    tmp_ctx = talloc_new(ctx);
    if (!tmp_ctx) {
        return ENOMEM;
    }

    account_dn = ldb_dn_new_fmt(tmp_ctx, ctx->ldb,
                                "uid=%s,cn=users,cn=%s,cn=remote",
                                name, ctx->domain);
    if (!account_dn) {
        talloc_free(tmp_ctx);
        return ENOMEM;
    }

    ret = ldb_delete(ctx->ldb, account_dn);

    if (ret != LDB_SUCCESS) {
        DEBUG(2, ("LDB Error: %s(%d)\nError Message: [%s]\n",
              ldb_strerror(ret), ret, ldb_errstring(ctx->ldb)));
        ret = EIO;
    }

    talloc_free(tmp_ctx);
    return ret;
}

/* TODO: Unify with nss_ldb and provide a single cachedb interface */

int dp_be_cachedb_init(struct be_ctx *ctx)
{
    TALLOC_CTX *tmp_ctx;
    char *ldb_file;
    char *default_db_file;
    int ret;

    tmp_ctx = talloc_new(ctx);
    if (!tmp_ctx) {
        return ENOMEM;
    }

    default_db_file = talloc_asprintf(tmp_ctx, "%s/%s", DB_PATH, DATA_PROVIDER_DB_FILE);
    if (!default_db_file) {
        talloc_free(tmp_ctx);
        return ENOMEM;
    }

    ret = confdb_get_string(ctx->cdb, tmp_ctx,
                            DATA_PROVIDER_DB_CONF_SEC, "ldbFile",
                            default_db_file, &ldb_file);
    if (ret != EOK) {
        talloc_free(tmp_ctx);
        return ret;
    }

    ctx->ldb = ldb_init(tmp_ctx, ctx->ev);
    if (!ctx->ldb) {
        talloc_free(tmp_ctx);
        return EIO;
    }

    ret = ldb_connect(ctx->ldb, ldb_file, 0, NULL);
    if (ret != LDB_SUCCESS) {
        talloc_free(tmp_ctx);
        return EIO;
    }

    talloc_steal(ctx, ctx->ldb);

    talloc_free(tmp_ctx);
    return EOK;
}

