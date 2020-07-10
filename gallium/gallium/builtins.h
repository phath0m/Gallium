#ifndef _GALLIUM_BUILTINS_H
#define _GALLIUM_BUILTINS_H

#include <stdint.h>
#include <gallium/list.h>
#include <gallium/object.h>

#define GA_BUILTIN_TYPE_DECL(var_name,type_name,ctr) struct ga_obj var_name = { \
    .ref_count = 1, \
    .type      = &ga_type_type_inst, \
    .un.statep = type_name, \
    .obj_ops = &(struct ga_obj_ops) { .invoke = ctr } \
};

struct stackframe;
struct stringbuf;
struct ga_code;

struct ga_dict_kvp {
    struct ga_obj   *   key;
    struct ga_obj   *   val;
};

typedef struct ga_obj * (*ga_builtin_t) (struct ga_obj *, struct vm *, int, struct ga_obj **);

extern struct ga_obj    ga_obj_type_inst;
extern struct ga_obj    ga_bool_true_inst;
extern struct ga_obj    ga_bool_false_inst;
extern struct ga_obj    ga_null_inst;

extern struct ga_obj    ga_class_type_inst;
extern struct ga_obj    ga_dict_type_inst;
extern struct ga_obj    ga_func_type_inst;
extern struct ga_obj    ga_int_type_inst;
extern struct ga_obj    ga_list_type_inst;
extern struct ga_obj    ga_range_type_inst;
extern struct ga_obj    ga_str_type_inst;
extern struct ga_obj    ga_weakref_type_inst;

struct ga_obj	*		ga_builtin_mod();

struct ga_obj   *       ga_argument_error_new(const char *);
struct ga_obj   *       ga_attribute_error_new(const char *);
struct ga_obj   *       ga_io_error_new(const char *);
struct ga_obj   *       ga_index_error_new(const char *);
struct ga_obj   *       ga_internal_error_new(const char*);
struct ga_obj   *       ga_key_error_new();
struct ga_obj   *       ga_name_error_new(const char *);
struct ga_obj   *       ga_type_error_new(const char *);
struct ga_obj   *       ga_value_error_new(const char *);

struct ga_obj   *       ga_bool_from_bool(bool);
bool                    ga_bool_to_bool(struct ga_obj *);

struct ga_obj   *       ga_builtin_new(ga_builtin_t, struct ga_obj *);

struct ga_obj   *       ga_class_base(struct ga_obj *);
struct ga_obj   *       ga_class_new(const char *, struct ga_obj *, struct ga_obj *);

struct ga_obj   *       ga_closure_new(struct stackframe *, struct ga_code *);

struct ga_obj   *       ga_file_new(int, mode_t);

struct ga_obj   *       ga_func_new(struct ga_code *);
void                    ga_func_add_param(struct ga_obj *, const char *, int);

struct ga_obj   *       ga_list_new();
void                    ga_list_append(struct ga_obj *, struct ga_obj *);
void                    ga_list_remove(struct ga_obj *, struct vm *, struct ga_obj *);
int                     ga_list_size(struct ga_obj *);

struct ga_obj   *       ga_method_new(struct ga_obj *, struct ga_obj *);

struct ga_obj   *       ga_int_from_i64(int64_t);
int64_t                 ga_int_to_i64(struct ga_obj *);

struct ga_obj   *       ga_range_new(int64_t, int64_t, int64_t);

struct ga_obj   *       ga_str_from_cstring(const char *);
struct ga_obj   *       ga_str_from_stringbuf(struct stringbuf *);
size_t                  ga_str_len(struct ga_obj *);
const char      *       ga_str_to_cstring(struct ga_obj *);


struct ga_obj   *       ga_dict_new();
void                    ga_dict_get_iter(struct ga_obj *, list_iter_t *);

struct ga_obj   *       ga_tuple_new(int nelems);
struct ga_obj   *       ga_tuple_get_elem(struct ga_obj *self, int elem);
int                     ga_tuple_get_size(struct ga_obj *self);
void                    ga_tuple_init_elem(struct ga_obj *self, int elem, struct ga_obj *obj);

struct ga_obj   *       ga_mod_new(const char *, struct ga_code *);
void                    ga_mod_import(struct ga_obj *, struct vm *, struct ga_obj *);

struct ga_obj   *       ga_weakref_new(struct ga_obj *);
struct ga_obj   *       ga_weakref_val(struct ga_obj *);

#endif
