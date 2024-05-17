#ifndef BE_MAP_H
#define BE_MAP_H

#include "be_object.h"

typedef struct bmapkey {
    union bvaldata v;
    uint32_t type:8;
    uint32_t next:24;
} bmapkey;

typedef struct bmapnode {
    bmapkey key;
    bvalue value;
} bmapnode;

struct bmap {
    bcommon_header;
    bmapnode *slots;
    bmapnode *lastfree;
    int size;
    int count;
};

typedef bmapnode *bmapiter;

#define be_map_iter()       NULL
#define be_map_count(map)   ((map)->count)
#define be_map_size(map)    (map->size)

bmap* be_map_new(bvm *vm);
void be_map_delete(bmap *map);
bvalue* be_map_find(bmap *map, bvalue *key);
bvalue* be_map_insert(bmap *map, bvalue *key, bvalue *value);
int be_map_remove(bmap *map, bvalue *key);
bvalue* be_map_findstr(bmap *map, bstring *key);
bvalue* be_map_insertstr(bmap *map, bstring *key, bvalue *value);
void be_map_removestr(bmap *map, bstring *key);
bmapnode* be_map_next(bmap *map, bmapiter *iter);
bvalue be_map_key2value(bmapnode *node);
void be_map_release(bvm *vm, bmap *map);

#endif
