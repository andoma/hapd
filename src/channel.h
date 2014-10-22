#pragma once

cfg_t *channel_by_id(cfg_t *root, int id);

cfg_t *channel_by_name(cfg_t *root, const char *name);

void channel_set_binary(const char *chname, int on, const char *source);
