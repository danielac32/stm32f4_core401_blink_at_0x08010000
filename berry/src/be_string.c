#include "be_string.h"
#include "be_vm.h"
#include "be_mem.h"
#include "be_gc.h"
#include "be_constobj.h"
#include <string.h>

#define next(_s)    cast(void*, cast(bstring*, (_s)->next))
#define sstr(_s)    cast(char*, cast(bsstring*, _s) + 1)
#define lstr(_s)    cast(char*, cast(blstring*, _s) + 1)
#define cstr(_s)    (cast(bcstring*, s)->s)

#define be_define_const_str(_name, _s, _hash, _extra, _len, _next) \
const bcstring be_const_str_##_name = { \
    .next = (bgcobject *)_next, \
    .type = BE_STRING, \
    .marked = GC_CONST, \
    .extra = _extra, \
    .slen = _len, \
    .hash = _hash, \
    .s = _s \
}

struct bstringtable {
    bstring **table;
    int count; /* string count */
    int size;
};

/* const string table */
struct bconststrtab {
    const bstring* const *table;
    int count; /* string count */
    int size;
};

#if BE_USE_PRECOMPILED_OBJECT
#include "../generate/be_const_strtab_def.h"
#endif

int be_eqstr(bstring *s1, bstring *s2)
{
    int slen;
    if (s1 == s2) { /* short string or the same string */
        return 1;
    }
    slen = s1->slen;
    /* long string */
    if (slen == 255 && slen == s2->slen) {
        blstring *ls1 = cast(blstring*, s1);
        blstring *ls2 = cast(blstring*, s2);
        return ls1->llen == ls2->llen && !strcmp(lstr(ls1), lstr(ls2));
    }
    return 0;
}

static void resize(bvm *vm, int size)
{
    int i;
    bstringtable *tab = vm->strtab;
    if (size > tab->size) {
        tab->table = be_realloc(tab->table, size * sizeof(bstring*));
        for (i = tab->size; i < size; ++i) {
            tab->table[i] = NULL;
        }
    }
    for (i = 0; i < tab->size; ++i) { /* rehash */
        bstring *p = tab->table[i];
        tab->table[i] = NULL;
        while (p) { /* for each node in the list */
            bstring *hnext = next(p);
            uint32_t hash = be_strhash(p) & (size - 1);
            p->next = cast(void*, tab->table[hash]);
            tab->table[hash] = p;
            p = hnext;
        }
    }
    if (size < tab->size) {
        tab->table = be_realloc(tab->table, size * sizeof(bstring*));
    }
    tab->size = size;
}

/* FNV-1a Hash */
uint32_t str_hash(const char *str, size_t len)
{
    uint32_t hash = 2166136261u;
    while (len--) {
        hash = (hash ^ *str++) * 16777619u;
    }
    return hash;
}

void be_string_init(bvm *vm)
{
    vm->strtab = be_malloc(sizeof(bstringtable));
    vm->strtab->size = 0;
    vm->strtab->count = 0;
    vm->strtab->table = NULL;
    resize(vm, 8);
}

void be_string_deleteall(bvm *vm)
{
    int i;
    bstringtable *tab = vm->strtab;
    for (i = 0; i < tab->size; ++i) {
        bstring *node = tab->table[i];
        while (node) {
            bstring *next = next(node);
            be_free(node);
            node = next;
        }
    }
    be_free(tab->table);
    be_free(tab);
}

bstring* createstrobj(bvm *vm, size_t len, int islong)
{
    char *str;
    size_t size = (islong ? sizeof(blstring)
                : sizeof(bsstring)) + len + 1;
    bgcobject *gco = be_gc_newstr(vm, size, islong);
    bstring *s = cast_str(gco);
    if (s) {
        s->slen = islong ? 255 : (bbyte)len;
        if (islong) {
            str = cast(char*, lstr(s));
        } else {
            str = cast(char*, sstr(s));
        }
        str[len] = '\0';
    }
    return s;
}

#if BE_USE_PRECOMPILED_OBJECT
static bstring* find_conststr(const char *str, size_t len)
{
    const struct bconststrtab *tab = &m_const_string_table;
    uint32_t hash = str_hash(str, len);
    bcstring *s = (bcstring*)tab->table[hash % tab->size];
    for (; s != NULL; s = next(s)) {
        if (len == s->slen && !strncmp(str, s->s, len)) {
            return (bstring*)s;
        }
    }
    return NULL;
}
#endif

static bstring* newshortstr(bvm *vm, const char *str, size_t len)
{
    bstring *s;
    int size = vm->strtab->size;
    uint32_t hash = str_hash(str, len);
    bstring **list = vm->strtab->table + (hash & (size - 1));

    for (s = *list; s != NULL; s = next(s)) {
        if (len == s->slen && !strncmp(str, sstr(s), len)) {
            return s;
        }
    }
    s = createstrobj(vm, len, 0);
    strncpy(cast(char*, sstr(s)), str, len);
    s->extra = 0;
    s->next = cast(void*, *list);
#if BE_STR_HASH_CACHE
    cast(bsstring*, s)->hash = hash;
#endif
    *list = s;
    vm->strtab->count++;
    if (vm->strtab->count > size << 2) {
        resize(vm, size << 1);
    }
    return s;
}

static bstring* newlongstr(bvm *vm, const char *str, size_t len)
{
    bstring *s;
    blstring *ls;
    s = createstrobj(vm, len, 1);
    ls = cast(blstring*, s);
    s->extra = 0;
    ls->llen = cast_int(len);
    strncpy(cast(char*, lstr(s)), str, len);
    return s;
}

bstring* be_newstr(bvm *vm, const char *str)
{
    return be_newstrn(vm, str, strlen(str));
}

bstring* be_newstrn(bvm *vm, const char *str, size_t len)
{
    if (len <= SHORT_STR_MAX_LEN) {
#if BE_USE_PRECOMPILED_OBJECT
        bstring *s = find_conststr(str, len);
        return s ? s : newshortstr(vm, str, len);
#else
        return newshortstr(vm, str, len);
#endif
    }
    return newlongstr(vm, str, len); /* long string */
}

void be_gcstrtab(bvm *vm)
{
    bstringtable *strtab = vm->strtab;
    int size = strtab->size, i;
    for (i = 0; i < size; ++i) {
        bstring **list = strtab->table + i;
        bstring *prev = NULL, *node, *next;
        for (node = *list; node; node = next) {
            next = next(node);
            if (!gc_isfixed(node) && gc_iswhite(node)) {
                be_free(node);
                strtab->count--;
                if (prev) { /* link list */
                    prev->next = cast(void*, next);
                } else {
                    *list = next;
                }
            } else {
                prev = node;
                gc_setwhite(node);
            }
        }
    }
    if (strtab->count < size >> 2 && size > 8) {
        resize(vm, size >> 1);
    }
}

uint32_t be_strhash(bstring *s)
{
    if (gc_isconst(s)) {
        return cast(bcstring*, s)->hash;
    }
#if BE_STR_HASH_CACHE
    if (s->slen != 255) {
        return cast(bsstring*, s)->hash;
    }
#endif
    return str_hash(str(s), str_len(s));
}

const char* be_str2cstr(bstring *s)
{
    if (gc_isconst(s)) {
        return cstr(s);
    }
    if (s->slen == 255) {
        return lstr(s);
    }
    return sstr(s);
}

void be_str_setextra(bstring *s, int extra)
{
    if (!gc_isconst(s)) {
        s->extra = cast(bbyte, extra);
    }
}
