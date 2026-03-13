#ifndef CFD_ENSIGHT_GOLD_H
#define CFD_ENSIGHT_GOLD_H

#include "cfd_core.h"

typedef enum {
    ENSIGHT_NOSECTION, // Not a section name
    ENSIGHT_FORMAT,
    ENSIGHT_GEOMETRY,
    ENSIGHT_VARIABLE,
    ENSIGHT_TIME,
    ENSIGHT_FILE,
    ENSIGHT_MATERIAL,
} Ensight_SectionType;

typedef struct {
    s32 key; // -1 if empty
    s32 value;
} Ensight_Part_Slot;

typedef struct {
    Ensight_Part_Slot *slots;
    u32 cap;
} Ensight_Part_Map;

typedef struct {
    u64 num_cells_of_type[CFD_CELL_TYPE_COUNT];
    u8 cell_type_idx_map[CFD_CELL_TYPE_COUNT];
    s32 **num_elems_per_type;
    u64 *node_offsets;
    u64 *num_nodes_per_part;
    V3 min_aabb, max_aabb;
    Ensight_Part_Map part_map;
    u64 num_coordinates;
    u32 num_element_types;

    u32 num_parts;
    b32 has_node_ids;
    b32 has_element_ids;
    b32 has_aabb;
} Ensight_Model_Info;

typedef struct {
    Str8 filename;

    Ensight_Model_Info **model_info_array;
    u32 model_info_array_len;

    s32 ts, fs; // -1 if not given
    b32 change_coords_only;
} Ensight_GeometryElem;

typedef struct {
    Ensight_GeometryElem *model;
    Ensight_GeometryElem *measured;
    Ensight_GeometryElem *match;
    Ensight_GeometryElem *boundary;
} Ensight_Geometry;

typedef enum {
    ENSIGHT_VARIABLE_SCALAR_PER_NODE,
    ENSIGHT_VARIABLE_VECTOR_PER_NODE,
    ENSIGHT_VARIABLE_SCALAR_PER_ELEMENT,
    ENSIGHT_VARIABLE_VECTOR_PER_ELEMENT,
} Ensight_VariableType;

typedef struct {
    Str8 filename;
    Str8 description;
    Ensight_VariableType type;
    s32 ts, fs; // -1 if not given
} Ensight_Variable;

typedef struct {
    Ensight_Variable *elems;
    u32 len;
} Ensight_VariableArray;

typedef struct {
    Str8 description; // Empty string if not specified
    float *time_values;

    s32 ts;
    u32 number_of_steps;
    u32 filename_start_number;
    u32 filename_increment;
} Ensight_Time;

typedef struct {
    Ensight_Time *elems;
    u32 len;
} Ensight_TimeArray;

typedef struct {
    Ensight_Geometry *geometry;
    Ensight_VariableArray *variable;
    Ensight_TimeArray *time;
    char dirname[PATH_MAX];
    u32 dirname_len;
} Ensight_Case;

typedef struct {
    u32 var_count;
    u32 time_count;
} Ensight_Case_Sizes;

CFD_LIB b32  ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, const char *case_filename, CFD_File *file);
CFD_LIB u32  ensight_get_geometry_model_filename(const Ensight_Case * restrict encase, u32 time_idx, u8 * restrict filename_buffer, u32 filename_buffer_size);
CFD_LIB u32  ensight_get_variable_filename(const Ensight_Case *restrict encase, u32 variable_idx, u32 time_idx, u8 *restrict filename_buffer, u32 filename_buffer_size);
CFD_LIB b32  ensight_parse_geometry_model_info(CFD_Arena *arena, CFD_Arena *scratch_arena, Ensight_Case *encase, Ensight_Model_Info *model_info, CFD_File *restrict file);
CFD_LIB b32  ensight_parse_model_merge_parts(CFD_Arena *arena, const Ensight_Case *restrict encase, Ensight_Model_Info *model_info, CFD_File * restrict file, CFD_UnstructuredGrid *mesh);
CFD_LIB f32 *ensight_parse_variable_per_node(CFD_Arena *arena, const Ensight_Case *restrict encase, Ensight_Model_Info *model_info, CFD_File *restrict file, u8 dim);
CFD_LIB f32 *ensight_parse_variable_per_element(CFD_Arena *arena, const Ensight_Case *restrict encase, Ensight_Model_Info *model_info, CFD_File *restrict file, u8 dim);

CFD_INTERNAL Ensight_SectionType ensight_get_section_type(Str8 s);
CFD_INTERNAL Str8 cfd_file_ensight_readline(CFD_File *file);
CFD_INTERNAL void ensight_split_key_value(Str8 str, Str8 *key, Str8 *value);
CFD_INTERNAL Str8 ensight_consume_word(Str8 *src);
CFD_INTERNAL Ensight_Case_Sizes ensight_get_case_sizes(CFD_File file);
CFD_INTERNAL void ensight_resolve_filename_in_place(u8 *filename, u32 filename_len, u32 filename_num);
CFD_INTERNAL u8 *ensight_read_80_bytes(CFD_File *f);
CFD_INTERNAL CFD_Cell_Type ensight_read_element_type(u8 *line, b32 *is_ghost);

CFD_INTERNAL force_inline s32 ensight_get_time_set_index(const Ensight_Case * restrict encase, s32 ts) {
    for (s32 time_set_idx = 0; time_set_idx < (s32)encase->time->len; ++time_set_idx)
        if (encase->time->elems[time_set_idx].ts == ts)
            return time_set_idx;

    return -1;
}

CFD_INTERNAL force_inline u8 *ensight_read_n_bytes(CFD_File *restrict f, u64 n) {
    if (unlikely(n > f->size - f->cur)) return NULL;

    u8 *ret = f->buffer + f->cur;
    f->cur += n;

    return ret;
}

CFD_INTERNAL force_inline u8 *ensight_read_80_bytes(CFD_File *restrict f) {
    return ensight_read_n_bytes(f, 80);
}

CFD_INTERNAL force_inline b32 ensight_consume_i32(CFD_File *f, s32 *out_val) {
    u8 *ptr = ensight_read_n_bytes(f, sizeof(s32));
    if (unlikely(ptr == NULL)) return false;

    *out_val = *(s32 *)ptr;
    return true;
}

CFD_INTERNAL force_inline b32 ensight_part_map_create(CFD_Arena *arena, Ensight_Part_Map *map, u32 cap) {
    CFD_CHECK_NULL(arena, false);
    CFD_CHECK_NULL(map, false);

    map->cap = cap;
    map->slots = cfd_arena_push_array(arena, Ensight_Part_Slot, cap);
    if (unlikely(map->slots == NULL)) return false;

    memset(map->slots, -1, cap * sizeof(Ensight_Part_Slot));

    return true;
}

CFD_INTERNAL force_inline u32 ensight_part_number_hash(const Ensight_Part_Map *restrict map, s32 part_number) {
    return (u32)part_number % map->cap;
}

CFD_INTERNAL force_inline void ensight_part_map_insert(const Ensight_Part_Map *restrict map, s32 key, s32 value) {
    u32 idx = ensight_part_number_hash(map, key);

    while (map->slots[idx].key != -1)
        idx = (idx + 1) % map->cap;

    Ensight_Part_Slot *slot = &map->slots[idx];
    slot->key = key;
    slot->value = value;
}

CFD_INTERNAL force_inline s32 ensight_part_map_get(const Ensight_Part_Map *restrict map, s32 key) {
    u32 idx = ensight_part_number_hash(map, key);

    u32 it = 0;

    while (map->slots[idx].key != key && map->slots[idx].key != -1) {
        idx = (idx + 1) % map->cap;

        if (unlikely(++it >= map->cap))
            return -1;
    }

    return map->slots[idx].value;
}

// WARNING: only use after ensight_read_80_bytes!
#define ensight_starts_with(line, s) cfd_mem_equals((line), "" s "", sizeof(s) - 1)

#ifdef CFD_LIB_IMPLEMENTATION

CFD_LIB b32 ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, const char *case_filename, CFD_File *f) {
    CFD_CHECK_NULL(arena, false);
    CFD_CHECK_NULL(encase, false);
    CFD_CHECK_NULL(case_filename, false);
    CFD_CHECK_NULL(f, false);
    cfd_info("case file size: %zu bytes", f->size);

    encase->geometry = NULL;
    encase->variable = NULL;
    encase->time = NULL;

    Ensight_Case_Sizes sizes = ensight_get_case_sizes(*f);
    if (sizes.var_count > 0) {
        encase->variable = cfd_arena_push_type_zero(arena, Ensight_VariableArray);
        encase->variable->elems = cfd_arena_push_array(arena, Ensight_Variable, sizes.var_count);
    }
    if (sizes.time_count > 0) {
        encase->time = cfd_arena_push_type_zero(arena, Ensight_TimeArray);
        encase->time->elems = cfd_arena_push_array(arena, Ensight_Time, sizes.time_count);
        encase->time->len = sizes.time_count;
    }


    cfd_dirname(case_filename, encase->dirname);
    encase->dirname_len = (u32)strlen(encase->dirname);
    cfd_info("Dirname: %s", encase->dirname);

    s32 time_set_idx = -1;

    Ensight_SectionType type = ENSIGHT_NOSECTION;

    while(!IS_CFD_FILE_EOF(f)) {
        Str8 line = cfd_file_ensight_readline(f);
        if (line.len == 0) continue;

        Ensight_SectionType new_type = ensight_get_section_type(line); 
        if (new_type != ENSIGHT_NOSECTION) {
            type = new_type;
            continue;
        }


        Str8 key, value;
        ensight_split_key_value(line, &key, &value);

        Str8 word, cursor_key;

        switch (type) {
            case ENSIGHT_FORMAT:
                word = ensight_consume_word(&key);
                if (unlikely(!str8_equals(word, str8_lit("type")))) {
                    cfd_error("expected 'type' keyword, got '%.*s'!", (int)word.len, word.buffer);
                    return false;
                }

                word = ensight_consume_word(&key);
                if (unlikely(word.len != 0)) {
                    cfd_error("only 'type' keyword is allowed as a key!");
                    return false;
                }

                word = ensight_consume_word(&value);
                if (unlikely(!str8_equals(word, str8_lit("ensight")))) return false;

                word = ensight_consume_word(&value);
                if (unlikely(!str8_equals(word, str8_lit("gold")))) return false;

                break;
            case ENSIGHT_GEOMETRY: {
                b32 change_coords_only = false;
                Str8 suffix = str8_lit("change_coords_only");

                if (str8_ends_with(value, suffix)) {
                    change_coords_only = true;
                    value.len -= suffix.len;
                    value = str8_rtrim(value);
                }

                s32 ts = -1, fs = -1;
                Str8 cursor = value;
                Str8 first = ensight_consume_word(&cursor);

                if (str8_is_all_digits(first) && cursor.len > 0) {
                    ts = str8_to_s32(first);
                    value = cursor;

                    Str8 second = ensight_consume_word(&cursor);
                    if (str8_is_all_digits(second) && cursor.len > 0) {
                        fs = str8_to_s32(second);
                        value = cursor;
                    }
                }

                Str8 filename = str8_ltrim(value);

                // cfd_info("file='%.*s', ts=%d, fs=%d, coords_only=%d", str8_arg(filename), ts, fs, change_coords_only);

                cursor_key = key;
                word = ensight_consume_word(&cursor_key);

                if (encase->geometry == NULL)
                    encase->geometry = cfd_arena_push_type_zero(arena, Ensight_Geometry);

                if (str8_equals(word, str8_lit("model"))) {
                    if (likely(encase->geometry->model == NULL))
                        encase->geometry->model = cfd_arena_push_type(arena, Ensight_GeometryElem);
                    else
                        cfd_warn("duplicate 'model' keyword found in Ensight Gold file. Previous values will be overwritten.");

                    Ensight_GeometryElem *model;
                    model = encase->geometry->model;

                    model->ts = ts;
                    model->fs = fs;
                    model->filename = str8_copy(arena, filename);
                    model->change_coords_only = change_coords_only;
                }
                else if (str8_equals(word, str8_lit("measured"))) {
                    if (likely(encase->geometry->measured == NULL))
                        encase->geometry->measured = cfd_arena_push_type(arena, Ensight_GeometryElem);
                    else
                        cfd_warn("duplicate 'measured' keyword found in Ensight Gold file. Previous values will be overwritten.");

                    Ensight_GeometryElem *measured;
                    measured = encase->geometry->measured;

                    measured->ts = ts;
                    measured->fs = fs;
                    measured->filename = str8_copy(arena, filename);
                    measured->change_coords_only = change_coords_only;
                }
                else if (str8_equals(word, str8_lit("match"))) {
                    if (likely(encase->geometry->match == NULL))
                        encase->geometry->match = cfd_arena_push_type(arena, Ensight_GeometryElem);
                    else
                        cfd_warn("duplicate 'match' keyword found in Ensight Gold file. Previous values will be overwritten.");

                    Ensight_GeometryElem *match;
                    match = encase->geometry->match;

                    match->ts = ts;
                    match->fs = fs;
                    match->filename = str8_copy(arena, filename);
                    match->change_coords_only = change_coords_only;
                }
                else if (str8_equals(word, str8_lit("boundary"))) {
                    if (likely(encase->geometry->boundary == NULL))
                        encase->geometry->boundary = cfd_arena_push_type(arena, Ensight_GeometryElem);
                    else
                        cfd_warn("duplicate 'boundary' keyword found in Ensight Gold file. Previous values will be overwritten.");

                    Ensight_GeometryElem *boundary;
                    boundary = encase->geometry->boundary;

                    boundary->ts = ts;
                    boundary->fs = fs;
                    boundary->filename = str8_copy(arena, filename);
                    boundary->change_coords_only = change_coords_only;
                } else {
                    cfd_error("invalid key in GEOMETRY section '%.*s' instead of 'model' | 'measured' | 'match' | 'boundary'!", str8_arg(key));
                    return false;
                }

                break;
                }
            case ENSIGHT_VARIABLE: {
                cursor_key = key;
                word = ensight_consume_word(&cursor_key);

                s32 ts = -1, fs = -1;
                Str8 value_cursor = value;
                Str8 first = ensight_consume_word(&value_cursor);
                if (str8_is_all_digits(first) && value_cursor.len > 0) {
                    ts = str8_to_s32(first);
                    value = value_cursor;

                    Str8 second = ensight_consume_word(&value_cursor);
                    if (str8_is_all_digits(second) && value_cursor.len > 0) {
                        fs = str8_to_s32(second);
                        value = value_cursor;
                    }
                }

                Str8 description = ensight_consume_word(&value);
                if (unlikely(description.len == 0)) {
                    cfd_error("no description found for variable in case file!");
                    return false;
                }

                Str8 filename = str8_ltrim(value);

                // cfd_info("Variable: ts=%d fs=%d description='%.*s' filename='%.*s'", ts, fs, str8_arg(description), str8_arg(filename));

                if (str8_equals(word, str8_lit("scalar"))) {
                    if (unlikely(encase->variable == NULL)) {
                        cfd_error("VARIABLE section error: no variable storage allocated!");
                        return false;
                    }

                    Ensight_Variable *variable = &encase->variable->elems[encase->variable->len++];
                    variable->ts = ts;
                    variable->fs = fs;
                    variable->description = str8_copy(arena, description);
                    variable->filename = str8_copy(arena, filename);

                    word = ensight_consume_word(&cursor_key);
                    if (unlikely(!str8_equals(word, str8_lit("per")))) {
                        cfd_error("expected 'per' keyword, got '%.*s' in case file!", str8_arg(word));
                        return false;
                    }

                    word = ensight_consume_word(&cursor_key);
                    if (str8_equals(word, str8_lit("node"))) {
                        variable->type = ENSIGHT_VARIABLE_SCALAR_PER_NODE;
                    } else if (str8_equals(word, str8_lit("element"))) {
                        variable->type = ENSIGHT_VARIABLE_SCALAR_PER_ELEMENT;
                    } else {
                        cfd_error("invalid key '%.*s' in case file!", str8_arg(key));
                        return false;
                    }
                }
                else if (str8_equals(word, str8_lit("vector"))) {
                    if (unlikely(encase->variable == NULL)) {
                        cfd_error("VARIABLE section error: no variable storage allocated!");
                        return false;
                    }

                    Ensight_Variable *variable = &encase->variable->elems[encase->variable->len++];
                    variable->ts = ts;
                    variable->fs = fs;
                    variable->description = str8_copy(arena, description);
                    variable->filename = str8_copy(arena, filename);

                    word = ensight_consume_word(&cursor_key);
                    if (unlikely(!str8_equals(word, str8_lit("per")))) {
                        cfd_error("expected 'per' keyword, got '%.*s' in case file!", str8_arg(word));
                        return false;
                    }

                    word = ensight_consume_word(&cursor_key);
                    if (str8_equals(word, str8_lit("node"))) {
                        variable->type = ENSIGHT_VARIABLE_VECTOR_PER_NODE;
                    } else if (str8_equals(word, str8_lit("element"))) {
                        variable->type = ENSIGHT_VARIABLE_VECTOR_PER_ELEMENT;
                    } else {
                        cfd_error("invalid key '%.*s' in case file!", str8_arg(key));
                        return false;
                    }
                }
                else if (str8_equals(word, str8_lit("constant"))) {
                    cfd_warn("'constant per case' and 'constant per case file' are not implemented yet!");
                }
                else if (str8_equals(word, str8_lit("tensor"))) {
                    cfd_error("'tensor symm/asymm per node/element' are not implemented yet!");
                    return false;
                }
                else if (str8_equals(word, str8_lit("complex"))) {
                    cfd_error("'complex scalar/vector per node/element' are not implemented yet!");
                    return false;
                } else {
                    cfd_error("invalid key in VARIABLE section '%.*s'!", str8_arg(key));
                }
                break;
                }
            case ENSIGHT_TIME: {
                if (unlikely(encase->time == NULL)) {
                    cfd_error("TIME section found but no 'time set' was counted during pre-scan!");
                    return false;
                }

                cursor_key = key;
                word = ensight_consume_word(&cursor_key);

                Ensight_Time *time = NULL;
                if (time_set_idx >= 0)
                    time = &encase->time->elems[time_set_idx];

                if (str8_equals(word, str8_lit("time"))) {
                    word = ensight_consume_word(&cursor_key);

                    if (str8_equals(word, str8_lit("set"))) {
                        Str8 value_cursor = value;
                        word = ensight_consume_word(&value_cursor);

                        if (unlikely(!str8_is_all_digits(word))) {
                            cfd_error("value of 'time set' must be numeric!");
                            return false;
                        }

                        time = &encase->time->elems[++time_set_idx];
                        time->ts = str8_to_s32(word);
                        time->description = str8_copy(arena, str8_ltrim(value_cursor));

                    } else if (str8_equals(word, str8_lit("values"))) {
                        if (unlikely(time_set_idx == -1)) {
                            cfd_error("TIME section protocol error: 'time set' must be defined before all other parameters (found '%.*s')!", str8_arg(key));
                            return false;
                        }

                        word = ensight_consume_word(&cursor_key);
                        if (str8_equals(word, str8_lit("file"))) {
                            cfd_error("'time values file' option in TIME section is not implemented yet!");
                            return false;
                        }

                        time->time_values = cfd_arena_push_array(arena, float, time->number_of_steps);
                        u32 time_idx = 0;

                        // TODO: check every value to see if it really is a float
                        while (time_idx < time->number_of_steps) {
                            word = ensight_consume_word(&value);
                            if (word.len == 0) break;
                            time->time_values[time_idx++] = str8_to_f32(word);
                        }

                        if (time->number_of_steps != time_idx) {
                            while (!IS_CFD_FILE_EOF(f) && time->number_of_steps != time_idx) {
                                line = cfd_file_ensight_readline(f);
                                if (line.len == 0) continue;

                                while (time_idx < time->number_of_steps) {
                                    word = ensight_consume_word(&line);
                                    if (word.len == 0) break;
                                    time->time_values[time_idx++] = str8_to_f32(word);
                                }

                            }
                        }

                    } else {
                        cfd_error("invalid key in TIME section '%.*s'!", str8_arg(key));
                        return false;
                    }
                } else if (str8_equals(word, str8_lit("number"))) {
                    if (unlikely(time_set_idx == -1)) {
                        cfd_error("TIME section protocol error: 'time set' must be defined before all other parameters (found '%.*s')!", str8_arg(key));
                        return false;
                    }

                    word = ensight_consume_word(&cursor_key);
                    Str8 steps_str = ensight_consume_word(&cursor_key);
                    if (unlikely(!str8_equals(word, str8_lit("of")) ||
                                  !str8_equals(steps_str, str8_lit("steps")))) {
                        cfd_error("invalid key in TIME section '%.*s' did you mean 'number of steps'?", str8_arg(key));
                        return false;
                    }

                    word = ensight_consume_word(&value);
                    if (unlikely(!str8_is_all_digits(word))) {
                        cfd_error("value of 'number of steps' must be numeric!");
                        return false;
                    }

                    time->number_of_steps = str8_to_u32(word);
                } else if (str8_equals(word, str8_lit("filename"))) {
                    if (unlikely(time_set_idx == -1)) {
                        cfd_error("TIME section protocol error: 'time set' must be defined before all other parameters (found '%.*s')!", str8_arg(key));
                        return false;
                    }

                    word = ensight_consume_word(&cursor_key);
                    if (str8_equals(word, str8_lit("start"))) {
                        word = ensight_consume_word(&cursor_key);
                        if (unlikely(!str8_equals(word, str8_lit("number")))) {
                            cfd_error("invalid key in TIME section '%.*s'!", str8_arg(key));
                            return false;
                        }

                        word = ensight_consume_word(&value);
                        if (unlikely(!str8_is_all_digits(word))) {
                            cfd_error("value of 'filename start number' must be numeric!");
                            return false;
                        }

                        time->filename_start_number = str8_to_u32(word);
                    } else if (str8_equals(word, str8_lit("increment"))) {
                        word = ensight_consume_word(&value);
                        if (unlikely(!str8_is_all_digits(word))) {
                            cfd_error("value of 'filename increment' must be numeric!");
                            return false;
                        }

                        time->filename_increment = str8_to_u32(word);
                    } else if (str8_equals(word, str8_lit("numbers"))) {
                        cfd_error("'filename numbers' and 'filename numbers file' options in TIME section are not implemented yet!");
                        return false;
                    } else {
                        cfd_error("invalid key in TIME section '%.*s'!", str8_arg(key));
                        return false;
                    }
                } else {
                    cfd_error("invalid key in TIME section '%.*s'!", str8_arg(key));
                    return false;
                }
                break;
                }
            case ENSIGHT_FILE:
                break;
            case ENSIGHT_MATERIAL:
                break;
            default:
                break;
        }


    }

    if (unlikely(encase->geometry == NULL)) {
        cfd_error("GEOMETRY section not found in case file!");
        return false;
    }

    if (unlikely(encase->geometry->model == NULL)) return false;

    s32 model_ts = encase->geometry->model->ts;
    u32 model_info_array_len;

    if (model_ts == -1) {
        model_info_array_len = 1;
    } else {
        if (unlikely(encase->time == NULL)) return false;

        time_set_idx = ensight_get_time_set_index(encase, model_ts);

        if (unlikely(time_set_idx == -1)) {
            cfd_error("no time set found with number: %d", model_ts);
            return false;
        }

        Ensight_Time *time = &encase->time->elems[time_set_idx];
        model_info_array_len = time->number_of_steps;
    }

    Ensight_GeometryElem *model = encase->geometry->model;
    model->model_info_array = (Ensight_Model_Info **)cfd_arena_alloc(arena, sizeof(Ensight_Model_Info *) * model_info_array_len);
    model->model_info_array_len = model_info_array_len;

    memset(model->model_info_array, 0, sizeof(Ensight_Model_Info *) * model_info_array_len);

    return true;
}

CFD_LIB u32 ensight_get_geometry_model_filename(const Ensight_Case * restrict encase, u32 time_idx, u8 * restrict filename_buffer, u32 filename_buffer_size) {
    CFD_CHECK_NULL(encase, 0);
    CFD_CHECK_NULL(filename_buffer, 0);

    if (unlikely(encase->geometry == NULL || encase->geometry->model == NULL)) {
        cfd_error("ensight_get_geometry_model_filename(): geometry model is not defined in case file!");
        return 0;
    }

    const Ensight_GeometryElem *model = encase->geometry->model;

    if (unlikely(model->fs != -1)) {
        cfd_error("ensight_get_geometry_model_filename(): file sets are not supported yet!");
        return 0;
    }

    u32 dirname_len = encase->dirname_len;
    u32 model_filename_len = (u32)model->filename.len;
    u32 filename_len = dirname_len + 1 + model_filename_len + 1;
    if (unlikely(filename_len > filename_buffer_size)) {
        cfd_error("ensight_get_geometry_model_filename(): length of the filename is longer than the provided buffer size (%u > %u)", filename_len, filename_buffer_size);
        return 0;
    }

    u32 offset = 0;

    memcpy(filename_buffer, encase->dirname, dirname_len);
    offset += dirname_len;

    filename_buffer[offset++] = '/';

    memcpy(filename_buffer + offset, model->filename.buffer, model_filename_len);
    offset += model_filename_len;

    filename_buffer[offset] = '\0';

    s32 ts = model->ts;
    if (ts != -1) {
        if (unlikely(encase->time == NULL)) {
            cfd_error("there are no time sets in case file!");
            return 0;
        }

        s32 time_set_idx = ensight_get_time_set_index(encase, ts);
        if (unlikely(time_set_idx == -1)) {
            cfd_error("time set = %d not found in case file!", ts);
            return 0;
        }

        Ensight_Time *t = &encase->time->elems[time_set_idx];

        if (unlikely(time_idx >= t->number_of_steps)) {
            cfd_error("time_idx = %u is out of bounds (number_of_steps = %u)!", time_idx, t->number_of_steps);
            return 0;
        }

        u32 file_num = t->filename_start_number + time_idx * t->filename_increment;

        ensight_resolve_filename_in_place(filename_buffer + dirname_len + 1, model_filename_len, file_num);
    }

    return offset;
}

CFD_LIB u32 ensight_get_variable_filename(const Ensight_Case *restrict encase, u32 variable_idx, u32 time_idx, u8 *restrict filename_buffer, u32 filename_buffer_size) {
    CFD_CHECK_NULL(encase, 0);
    CFD_CHECK_NULL(filename_buffer, 0);

    if (encase->variable == NULL) {
        cfd_error("ensight_get_variable_filename(): variable section is not defined in case file!");
        return 0;
    }

    const Ensight_VariableArray *var_array = encase->variable;
    if (unlikely(variable_idx >= var_array->len)) {
        cfd_error("ensight_get_variable_filename(): variable index is out of range!");
        return 0;
    }

    const Ensight_Variable *variable = &var_array->elems[variable_idx];

    if (unlikely(variable->fs != -1)) {
        cfd_error("ensight_get_variable_filename(): file sets are not supported yet!");
        return 0;
    }

    u32 dirname_len = encase->dirname_len;
    u32 variable_filename_len = (u32)variable->filename.len;
    u32 filename_len = dirname_len + 1 + variable_filename_len + 1;
    if (unlikely(filename_len > filename_buffer_size)) {
        cfd_error("ensight_get_variable_filename(): length of the filename is longer than the provided buffer size (%u > %u)", filename_len, filename_buffer_size);
        return 0;
    }

    u32 offset = 0;

    memcpy(filename_buffer, encase->dirname, dirname_len);
    offset += dirname_len;

    filename_buffer[offset++] = '/';

    memcpy(filename_buffer + offset, variable->filename.buffer, variable_filename_len);
    offset += variable_filename_len;

    filename_buffer[offset] = '\0';

    s32 ts = variable->ts;
    if (ts != -1) {
        if (unlikely(encase->time == NULL)) {
            cfd_error("there are no time sets in case file!");
            return 0;
        }

        s32 time_set_idx = ensight_get_time_set_index(encase, ts);
        if (unlikely(time_set_idx == -1)) {
            cfd_error("time set = %d not found in case file!", ts);
            return 0;
        }

        Ensight_Time *t = &encase->time->elems[time_set_idx];

        if (unlikely(time_idx >= t->number_of_steps)) {
            cfd_error("time_idx = %u is out of bounds (number_of_steps = %u)!", time_idx, t->number_of_steps);
            return 0;
        }

        u32 file_num = t->filename_start_number + time_idx * t->filename_increment;

        ensight_resolve_filename_in_place(filename_buffer + dirname_len + 1, variable_filename_len, file_num);
    }

    return offset;
}

typedef struct {
    s32 part_num;
    u64 node_offset;
    u64 num_nodes;
    u32 num_types;
    // invisible cell type array
} PartInfo_Internal;

CFD_LIB b32 ensight_parse_geometry_model_info(CFD_Arena *arena, CFD_Arena *scratch_arena, Ensight_Case *encase, Ensight_Model_Info *model_info, CFD_File *restrict file) {
    CFD_CHECK_NULL(arena, false);
    CFD_CHECK_NULL(scratch_arena, false);
    CFD_CHECK_NULL(encase, false);
    CFD_CHECK_NULL(model_info, false);
    CFD_CHECK_NULL(file, false);

    u8 *line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL || !ensight_starts_with(line, "C Binary"))) {
        cfd_error("not a valid binary ensight gold geometry file!");
        return false;
    }

    // Skip the description lines
    if (unlikely(ensight_read_n_bytes(file, 2 * 80) == NULL)) return false;

    // Node id
    line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL || !ensight_starts_with(line, "node id "))) {
        cfd_error("'node id' label not found in geometry file!");
        return false;
    }

    line += 8;
    b32 has_node_ids = ensight_starts_with(line, "given") | ensight_starts_with(line, "ignore");

    // Element id
    line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL || !ensight_starts_with(line, "element id "))) {
        cfd_error("'element id' label not found in geometry file!");
        return false;
    }

    line += 11;
    b32 has_element_ids = ensight_starts_with(line, "given") | ensight_starts_with(line, "ignore");


    model_info->has_aabb = false;

    line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL)) return false;

    if (ensight_starts_with(line, "extents")) {
        model_info->has_aabb = true;
        V3 *min = &model_info->min_aabb;
        V3 *max = &model_info->max_aabb;

        f32 *aabb_ptr = (f32 *)ensight_read_n_bytes(file, 6 * sizeof(f32));
        if (aabb_ptr == NULL) {
            cfd_info("no extents values found in geometry file");
            return false;
        }

        min->x = *(aabb_ptr++);
        max->x = *(aabb_ptr++);

        min->y = *(aabb_ptr++);
        max->y = *(aabb_ptr++);

        min->z = *(aabb_ptr++);
        max->z = *(aabb_ptr);

        line = ensight_read_80_bytes(file);
        if (unlikely(line == NULL)) return false;
    }

    model_info->has_node_ids    = has_node_ids;
    model_info->has_element_ids = has_element_ids;

    u32 num_parts = 0;
    u64 total_num_nodes = 0;

    CFD_Cell_Type cell_type;
    b32 is_ghost;

    u64 *num_cells_of_type = model_info->num_cells_of_type;
    memset(num_cells_of_type, 0, CFD_CELL_TYPE_COUNT * sizeof(u64));

    if (unlikely(!cfd_arena_align(scratch_arena, align_of(PartInfo_Internal)))) return false;
    PartInfo_Internal *partinfos = (PartInfo_Internal *)(scratch_arena->buffer + scratch_arena->offset);

    u32 num_types_per_part;
    while (line != NULL && ensight_starts_with(line, "part")) {
        ++num_parts;

        s32 *part_ptr = (s32 *)ensight_read_n_bytes(file, 4);
        if (unlikely(part_ptr == NULL)) {
            cfd_error("part number not found in geometry file");
            return false;
        }

        s32 part_num = *part_ptr;
        PartInfo_Internal *p_info = cfd_arena_push_type(scratch_arena, PartInfo_Internal);
        if (p_info == NULL) return false;

        p_info->part_num = part_num;
        p_info->node_offset = total_num_nodes;
        p_info->num_nodes = 0;
        num_types_per_part = 0;

        //cfd_info("%u. part: part number = %d", num_parts, part_num);
        ensight_read_80_bytes(file);

        while (likely(!IS_CFD_FILE_EOF(file))) {
            line = ensight_read_80_bytes(file);
            if (unlikely(line == NULL)) break;

            if (ensight_starts_with(line, "part")) break;

            if (ensight_starts_with(line, "coordinates")) {
                s32 num_of_nodes;
                if (unlikely(!ensight_consume_i32(file, &num_of_nodes))) return false;

                if (has_node_ids)
                    ensight_read_n_bytes(file, (u64)num_of_nodes * sizeof(s32));

                ensight_read_n_bytes(file, 3 * (u64)num_of_nodes * sizeof(f32));

                p_info->node_offset = total_num_nodes;
                p_info->num_nodes = (u64)num_of_nodes;
                total_num_nodes += (u64)num_of_nodes;
            } else if (ensight_starts_with(line, "block")) {
                cfd_error("structured data is not implemented yet!");
                return false;
            } else if ((cell_type = ensight_read_element_type(line, &is_ghost)) != CFD_CELL_UNKNOWN) {
                s32 num_of_elements;
                if (unlikely(!ensight_consume_i32(file, &num_of_elements))) return false;

                if (has_element_ids)
                    ensight_read_n_bytes(file, (u64)num_of_elements * sizeof(s32));

                if (!is_ghost)
                    num_cells_of_type[cell_type] += (u64)num_of_elements;

                u8 node_count = cfd_get_cell_node_count(cell_type);
                if (node_count == 0) {
                    cfd_error("nsided/nfaced elements are not supported yet!");
                    return false;
                }

                ensight_read_n_bytes(file, (u64)node_count * (u64)num_of_elements * sizeof(s32));

                s32 *ne = cfd_arena_push_type(scratch_arena, s32);
                if (unlikely(ne == NULL)) return false;

                *ne = num_of_elements;
                ++num_types_per_part;
            } else {
                cfd_error("unknown block or corrupted file: %.80s", (const char*)line);
                return false;
            }
        }

        p_info->num_types = num_types_per_part;
    }

    if (unlikely(!ensight_part_map_create(arena, &model_info->part_map, num_parts * 2))) return false;

    model_info->num_elems_per_type = (s32 **)cfd_arena_alloc_aligned(arena, num_parts * sizeof(s32 *), align_of(s32 *));
    if (model_info->num_elems_per_type == NULL) return false;

    model_info->node_offsets = cfd_arena_push_array(arena, u64, num_parts);
    if (unlikely(model_info->node_offsets == NULL)) return false;

    model_info->num_nodes_per_part = cfd_arena_push_array(arena, u64, num_parts);
    if (unlikely(model_info->num_nodes_per_part == NULL)) return false;

    for (s32 part_idx = 0; part_idx < (s32)num_parts; ++part_idx) {
        //(partinfos + align_of(PartInfo_Internal) - 1) & ~(align_of(PartInfo_Internal) - 1)
        s32 *num_elems = model_info->num_elems_per_type[part_idx];
        ensight_part_map_insert(&model_info->part_map, partinfos->part_num, part_idx);

        u32 num_types = partinfos->num_types;
        num_elems = cfd_arena_push_array(arena, s32, num_types);
        s32 *a = (s32 *)((u8 *)partinfos + sizeof(PartInfo_Internal));
        for (u32 i = 0; i < num_types; ++i) {
            num_elems[i] = a[i];
        }

        model_info->num_elems_per_type[part_idx] = num_elems;
        model_info->node_offsets[part_idx] = partinfos->node_offset;
        model_info->num_nodes_per_part[part_idx] = partinfos->num_nodes;

        partinfos = (PartInfo_Internal *)((u64)partinfos + sizeof(PartInfo_Internal) + partinfos->num_types * sizeof(s32));
        partinfos = (PartInfo_Internal *)(((u64)partinfos + align_of(PartInfo_Internal) - 1) & ~(align_of(PartInfo_Internal) - 1));
    }

    u32 element_type_idx = 0;
    u32 num_element_types = 0;
    for (u32 i = 0; i < CFD_CELL_TYPE_COUNT; ++i) {
        if (num_cells_of_type[i] != 0) {
            ++num_element_types;
            model_info->cell_type_idx_map[i] = (u8)(element_type_idx++);
        }
    }

    model_info->num_parts = num_parts;
    model_info->num_coordinates = total_num_nodes;
    model_info->num_element_types = num_element_types;

    cfd_info("number of element types: %u", num_element_types);
    cfd_info("number of parts: %u", num_parts);
    cfd_info("total number of nodes: %zu", total_num_nodes);

    return true;
}

CFD_LIB b32 ensight_parse_model_merge_parts(CFD_Arena *arena, const Ensight_Case *restrict encase, Ensight_Model_Info *model_info, CFD_File * restrict file, CFD_UnstructuredGrid *mesh) {
    CFD_CHECK_NULL(arena, false);
    CFD_CHECK_NULL(encase, false);
    CFD_CHECK_NULL(model_info, false);
    CFD_CHECK_NULL(file, false);
    CFD_CHECK_NULL(mesh, false);

    mesh->vertices = cfd_arena_push_array(arena, V3, model_info->num_coordinates);
    mesh->num_vertices = model_info->num_coordinates;
    mesh->num_cell_types = model_info->num_element_types;
    mesh->cell_groups = cfd_arena_push_array(arena, CFD_Cell_Group, model_info->num_element_types);

    CFD_Cell_Group *cell_groups = mesh->cell_groups;
    u64 *num_cells_of_type = model_info->num_cells_of_type;
    u32 num_cell_types = 0;
    for (u32 i = 0; i < CFD_CELL_TYPE_COUNT; ++i) {
        if (num_cells_of_type[i] != 0) {
            u64 num_cells = num_cells_of_type[i];
            u64 node_count = (u64)cfd_get_cell_node_count(i) * num_cells;

            CFD_Cell_Group *cell_group = &cell_groups[num_cell_types++];
            cell_group->type = (CFD_Cell_Type)i;
            cell_group->num_cells = num_cells;
            cell_group->connectivity = cfd_arena_push_array(arena, u64, node_count);
        }
    }


    u64 cell_type_offsets[CFD_CELL_TYPE_COUNT];
    memset(cell_type_offsets, 0, CFD_CELL_TYPE_COUNT * sizeof(u64));

    u64 vertices_offset = 0;

    CFD_Cell_Type cell_type;
    b32 is_ghost;

    file->cur = 0;

    u64 skip_bytes = 5 * 80;
    skip_bytes += (u64)model_info->has_aabb * (80 + 6 * sizeof(f32));
    ensight_read_n_bytes(file, skip_bytes);

    const u32 num_parts = model_info->num_parts;
    const b32 has_node_ids = model_info->has_node_ids;
    const b32 has_element_ids = model_info->has_element_ids;
    const u8 *cell_type_idx_map = model_info->cell_type_idx_map;
    const u64 *node_offsets = model_info->node_offsets;

    u8 *line;

    for (u32 part_idx = 0; part_idx < num_parts; ++part_idx) {
        ensight_read_n_bytes(file, 80 + 1 * sizeof(s32) + 80);

        u64 current_part_vertex_offset = node_offsets[part_idx];

        while (!IS_CFD_FILE_EOF(file)) {
            line = ensight_read_80_bytes(file);

            if (ensight_starts_with(line, "part")) {
                file->cur -= 80;
                break;
            }
            else if (ensight_starts_with(line, "coordinates")) {
                s32 num_of_nodes;
                if (unlikely(!ensight_consume_i32(file, &num_of_nodes))) return false;

                if (has_node_ids)
                    ensight_read_n_bytes(file, (u64)num_of_nodes * sizeof(s32));

                f32 *base_ptr = (f32 *)(file->buffer + file->cur);

                const f32 *restrict raw_x = base_ptr;
                const f32 *restrict raw_y = base_ptr + num_of_nodes;
                const f32 *restrict raw_z = base_ptr + 2 * num_of_nodes;
                V3  *restrict vertices = mesh->vertices + vertices_offset;

                for (s32 i = 0; i < num_of_nodes; ++i) {
                    vertices[i].x = raw_x[i];
                    vertices[i].y = raw_y[i];
                    vertices[i].z = raw_z[i];
                }


                vertices_offset += (u64)num_of_nodes;

                u64 coords_byte_size = (u64)num_of_nodes * 3 * sizeof(f32);
                file->cur += coords_byte_size;
            } else if ((cell_type = ensight_read_element_type(line, &is_ghost)) != CFD_CELL_UNKNOWN) {
                s32 num_of_elements;
                if (unlikely(!ensight_consume_i32(file, &num_of_elements))) return false;

                if (has_element_ids)
                    file->cur += (u64)num_of_elements * sizeof(s32);

                u8 node_count = cfd_get_cell_node_count(cell_type);
                if (unlikely(node_count == 0)) {
                    cfd_error("nsided/nfaced elements are not supported yet!");
                    return false;
                }

                u64 elements_size = (u64)node_count * (u64)num_of_elements;

                if (likely(!is_ghost)) {
                    u64 *target = cell_groups[cell_type_idx_map[cell_type]].connectivity + cell_type_offsets[cell_type];
                    const s32 *restrict raw_indices = (const s32 *)(file->buffer + file->cur);

                    for (u64 i = 0; i < elements_size; ++i) {
                        target[i] = (u64)raw_indices[i] - 1 + current_part_vertex_offset;
                    }

                    cell_type_offsets[cell_type] += elements_size;
                }


                file->cur += elements_size * sizeof(s32);
            } else {
                break;
            }
        }
    }

    return true;
}

CFD_LIB f32 *ensight_parse_variable_per_node(CFD_Arena *arena, const Ensight_Case *restrict encase, Ensight_Model_Info *model_info, CFD_File *restrict file, u8 dim) {
    CFD_CHECK_NULL(arena, NULL);
    CFD_CHECK_NULL(encase, NULL);
    CFD_CHECK_NULL(model_info, NULL);
    CFD_CHECK_NULL(file, NULL);

    f32 *result = cfd_arena_push_array(arena, f32, model_info->num_coordinates * (u64)dim);
    if (unlikely(result == NULL)) return NULL;

    u8 *line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL)) {
        cfd_error("description line not found in per node variable file!");
        return NULL;
    }

    line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL)) return NULL;

    const u64 *node_offsets = model_info->node_offsets;
    const u64 *num_nodes_per_part = model_info->num_nodes_per_part;

    while (ensight_starts_with(line, "part")) {
        s32 part_num;
        if (unlikely(!ensight_consume_i32(file, &part_num))) return NULL;

        s32 part_idx = ensight_part_map_get(&model_info->part_map, part_num);
        if (unlikely(part_idx == -1)) {
            cfd_error("ensight_parse_variable_per_node(): part number(%d) not found", part_num);
            return NULL;
        }

        u64 part_start = node_offsets[part_idx];
        u64 part_num_nodes = num_nodes_per_part[part_idx];

        while (likely(!IS_CFD_FILE_EOF(file))) {
            line = ensight_read_80_bytes(file);
            if (unlikely(line == NULL)) break;

            if (ensight_starts_with(line, "part")) break;

            if (ensight_starts_with(line, "coordinates")) {
                u64 num_values = part_num_nodes * (u64)dim;
                f32 *src = (f32 *)ensight_read_n_bytes(file, num_values * sizeof(f32));
                if (unlikely(src == NULL)) return NULL;

                f32 *dst = result + part_start * (u64)dim;
                if (dim == 1) {
                    memcpy(dst, src, part_num_nodes * sizeof(f32));
                } else {
                    // EnSight Gold stores vectors in SoA: [x0..xN, y0..yN, z0..zN]
                    // Convert to AoS: [x0,y0,z0, x1,y1,z1, ...]
                    for (u64 n = 0; n < part_num_nodes; ++n) {
                        for (u8 d = 0; d < dim; ++d) {
                            dst[n * (u64)dim + d] = src[(u64)d * part_num_nodes + n];
                        }
                    }
                }
            } else if (ensight_starts_with(line, "block")) {
                cfd_error("structured data is not implemented yet!");
                return NULL;
            } else {
                cfd_error("unknown block or corrupted file: %.80s", (const char*)line);
                return NULL;
            }
        }
    }

    return result;
}

CFD_LIB f32 *ensight_parse_variable_per_element(CFD_Arena *arena, const Ensight_Case *restrict encase, Ensight_Model_Info *model_info, CFD_File *restrict file, u8 dim) {
    CFD_CHECK_NULL(arena, NULL);
    CFD_CHECK_NULL(encase, NULL);
    CFD_CHECK_NULL(model_info, NULL);
    CFD_CHECK_NULL(file, NULL);

    u64 total_cells = 0;
    for (u32 i = 0; i < CFD_CELL_TYPE_COUNT; ++i)
        total_cells += model_info->num_cells_of_type[i];

    if (unlikely(total_cells == 0)) {
        cfd_error("ensight_parse_variable_per_element(): no cells found in model info!");
        return NULL;
    }

    f32 *result = cfd_arena_push_array(arena, f32, total_cells * dim);
    if (unlikely(result == NULL)) return NULL;

    u64 cell_type_base[CFD_CELL_TYPE_COUNT];
    u64 cell_type_offsets[CFD_CELL_TYPE_COUNT];
    memset(cell_type_offsets, 0, sizeof(cell_type_offsets));

    u64 offset = 0;
    for (u32 i = 0; i < CFD_CELL_TYPE_COUNT; ++i) {
        cell_type_base[i] = offset;
        offset += model_info->num_cells_of_type[i];
    }

    u8 *line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL)) {
        cfd_error("description line not found in per element variable file!");
        return NULL;
    }

    line = ensight_read_80_bytes(file);
    if (unlikely(line == NULL)) return NULL;

    CFD_Cell_Type cell_type;
    b32 is_ghost;

    while (ensight_starts_with(line, "part")) {
        s32 part_num;
        if (unlikely(!ensight_consume_i32(file, &part_num))) return NULL;

        s32 part_idx = ensight_part_map_get(&model_info->part_map, part_num);
        if (unlikely(part_idx == -1)) {
            cfd_error("ensight_parse_variable_per_element(): part number(%d) not found", part_num);
            return NULL;
        }

        u32 elem_idx = 0;

        while (likely(!IS_CFD_FILE_EOF(file))) {
            line = ensight_read_80_bytes(file);
            if (unlikely(line == NULL)) break;

            if (ensight_starts_with(line, "part")) break;

            if (ensight_starts_with(line, "block")) {
                cfd_error("structured data is not implemented yet!");
                return NULL;
            } else if ((cell_type = ensight_read_element_type(line, &is_ghost)) != CFD_CELL_UNKNOWN) {
                s32 num_of_elements = model_info->num_elems_per_type[part_idx][elem_idx++];
                u64 num_values = (u64)num_of_elements * (u64)dim;

                if (!is_ghost) {
                    f32 *src = (f32 *)ensight_read_n_bytes(file, num_values * sizeof(f32));
                    if (unlikely(src == NULL)) return NULL;

                    u64 dest_base = (cell_type_base[cell_type] + cell_type_offsets[cell_type]) * (u64)dim;
                    f32 *dst = result + dest_base;

                    if (dim == 1) {
                        memcpy(dst, src, (u64)num_of_elements * sizeof(f32));
                    } else {
                        // EnSight Gold stores vectors in SoA: [x0..xN, y0..yN, z0..zN]
                        // Convert to AoS: [x0,y0,z0, x1,y1,z1, ...]
                        for (s32 e = 0; e < num_of_elements; ++e) {
                            for (u8 d = 0; d < dim; ++d) {
                                dst[e * dim + d] = src[d * num_of_elements + e];
                            }
                        }
                    }

                    cell_type_offsets[cell_type] += (u64)num_of_elements;
                } else {
                    ensight_read_n_bytes(file, num_values * sizeof(f32));
                }
            } else {
                cfd_error("unknown block or corrupted file: %.80s", (const char*)line);
                return NULL;
            }
        }
    }

    return result;
}

CFD_INTERNAL Ensight_SectionType ensight_get_section_type(Str8 s) {
    if (s.len == 0) return ENSIGHT_NOSECTION;

    switch(s.buffer[0]) {
        case 'F':
            if (s.len >= 6 && memcmp(s.buffer + 1, "ORMAT", 5) == 0)
                return ENSIGHT_FORMAT;
            else if (s.len >= 4 && memcmp(s.buffer + 1, "ILE", 3) == 0)
                return ENSIGHT_FILE;
            break;
        case 'G':
            if (s.len >= 8 && memcmp(s.buffer + 1, "EOMETRY", 7) == 0)
                return ENSIGHT_GEOMETRY;
            break;
        case 'V':
            if (s.len >= 8 && memcmp(s.buffer + 1, "ARIABLE", 7) == 0)
                return ENSIGHT_VARIABLE;
            break;
        case 'T':
            if (s.len >= 4 && memcmp(s.buffer + 1, "IME", 3) == 0)
                return ENSIGHT_TIME;
            break;
        case 'M':
            if (s.len >= 8 && memcmp(s.buffer + 1, "ATERIAL", 7) == 0)
                return ENSIGHT_MATERIAL;
            break;
        default:
            break;
    }

    return ENSIGHT_NOSECTION;
}

CFD_INTERNAL Str8 cfd_file_ensight_readline(CFD_File *file) {
    if (file->cur >= file->size) {
        Str8 result = { NULL, 0 };
        return result;
    }

    // Skips leading whitespace characters
    while (file->cur < file->size && (file->buffer[file->cur] == ' ' || file->buffer[file->cur] == '\t'))
        ++file->cur;

    Str8 line;
    line.buffer = file->buffer + file->cur;
    line.len = 0;

    b32 found_comment = false;

    while (file->cur < file->size) {
        u8 ch = file->buffer[file->cur];

        if (ch == '\r') {
            if (file->cur + 1 < file->size && file->buffer[file->cur + 1] == '\n')
                file->cur += 2;
            else
                file->cur++;

            break;
        }

        else if (ch == '\n') {
            file->cur++;
            break;
        }

        else if (ch == '#') found_comment = true;
        else if (!found_comment) ++line.len;

        file->cur++;
    }

    // Removes trailing whitespace characters
    while (line.len > 0 && (line.buffer[line.len - 1] == ' ' || line.buffer[line.len - 1] == '\t'))
        --line.len;

    return line;
}

CFD_INTERNAL void ensight_split_key_value(Str8 str, Str8 *key, Str8 *value) {
    key->buffer   = NULL;
    key->len      = 0;
    value->buffer = NULL;
    value->len    = 0;

    if (str.len == 0) return;

    void *colon_ptr = memchr(str.buffer, ':', str.len);
    if (!colon_ptr) {
        cfd_error("':' expected but not found in '%.*s'", (int)str.len, str.buffer);
        return;
    }

    u64 colon_idx = (u64)((u8 *)colon_ptr - str.buffer);

    u64 key_len = colon_idx;
    while (key_len > 0 && (str.buffer[key_len - 1] == ' ' || str.buffer[key_len - 1] == '\t'))
        --key_len;

    key->buffer = str.buffer;
    key->len = key_len;

    u64 val_start = colon_idx + 1;
    while (val_start < str.len && (str.buffer[val_start] == ' ' || str.buffer[val_start] == '\t'))
        ++val_start;

    if (val_start < str.len) {
        value->buffer = str.buffer + val_start;
        value->len    = str.len - val_start;
    }
}

CFD_INTERNAL Str8 ensight_consume_word(Str8 *src) {
    Str8 token = { NULL, 0 };
    if (src->len == 0) return token;

    u64 start = 0;
    while (start < src->len && (src->buffer[start] == ' ' || src->buffer[start] == '\t'))
        ++start;

    src->buffer += start;
    src->len -= start;

    token.buffer = src->buffer;

    u64 end = 0;
    while (end < src->len && !(src->buffer[end] == ' ' || src->buffer[end] == '\t'))
        ++end;

    token.len = end;

    src->buffer += end;
    src->len -= end;

    return token;
}

CFD_INTERNAL Ensight_Case_Sizes ensight_get_case_sizes(CFD_File file) {
    CFD_File *f = &file;

    Ensight_Case_Sizes sizes = { 0, 0 };

    while(!IS_CFD_FILE_EOF(f)) {
        Str8 line = cfd_file_ensight_readline(f);
        if (line.len == 0) continue;

        Str8 word = ensight_consume_word(&line);
        if (str8_equals(word, str8_lit("scalar"))
            || str8_equals(word, str8_lit("vector"))
          //|| str8_equals(word, str8_lit("constant"))
          //|| str8_equals(word, str8_lit("tensor"))
          //|| str8_equals(word, str8_lit("complex"))
            ) {
            ++sizes.var_count;
        } else if (str8_equals(word, str8_lit("time"))) {
            Str8 key, value;
            ensight_split_key_value(line, &key, &value);

            word = ensight_consume_word(&key);
            if (str8_equals(word, str8_lit("set")))
                ++sizes.time_count;
        }
    }

    cfd_info("var_count = %d", sizes.var_count);
    cfd_info("time_count = %d", sizes.time_count);
    return sizes;
}

CFD_INTERNAL void ensight_resolve_filename_in_place(u8 *filename, u32 filename_len, u32 filename_num) {
    s32 end_star = -1;
    for (s32 i = (s32)filename_len - 1; i >= 0; --i) {
        if (filename[i] == '*') {
            end_star = i;
            break;
        }
    }

    if (unlikely(end_star == -1)) return;

    s32 start_star = end_star;
    while (start_star > 0 && filename[start_star - 1] == '*')
        --start_star;

    for (s32 i = end_star; i >= start_star; --i) {
        filename[i] = (u8)((filename_num % 10) + '0');
        filename_num /= 10;
    }
}

CFD_INTERNAL CFD_Cell_Type ensight_read_element_type(u8 *line, b32 *is_ghost) {
    *is_ghost = false;

    if (line[0] == 'g' && line[1] == '_') {
        line += 2;
        *is_ghost = true;
    }

    switch (line[0]) {
        case 'b':
            if (line[1] == 'a' && line[2] == 'r') {
                if (line[3] == '2') return CFD_CELL_BAR2;
                if (line[3] == '3') return CFD_CELL_BAR3;
            }
            break;
        case 'h':
            if (line[1] == 'e' && line[2] == 'x' && line[3] == 'a') {
                if (line[4] == '8')                   return CFD_CELL_HEXA8;
                if (line[4] == '2' && line[5] == '0') return CFD_CELL_HEXA20;
            }
            break;
        case 'n':
            ++line;
            if (ensight_starts_with(line, "faced")) return CFD_CELL_NFACED;
            if (ensight_starts_with(line, "sided")) return CFD_CELL_NSIDED;
            break;
        case 'p':
            ++line;
            if (ensight_starts_with(line, "enta")) {
                if (line[4] == '6')                   return CFD_CELL_PENTA6;
                if (line[4] == '1' && line[5] == '5') return CFD_CELL_PENTA15;
            }
            if (ensight_starts_with(line, "oint")) {
                return CFD_CELL_POINT;
            }
            if (ensight_starts_with(line, "yramid")) {
                if (line[6] == '5')                   return CFD_CELL_PYRAMID5;
                if (line[6] == '1' && line[7] == '3') return CFD_CELL_PYRAMID13;
            }
            break;
        case 'q':
            if (line[1] == 'u' && line[2] == 'a' && line[3] == 'd') {
                if (line[4] == '4') return CFD_CELL_QUAD4;
                if (line[4] == '8') return CFD_CELL_QUAD8;
            }
            break;
        case 't':
            ++line;
            if (ensight_starts_with(line, "etra")) {
                if (line[4] == '4')                   return CFD_CELL_TETRA4;
                if (line[4] == '1' && line[5] == '0') return CFD_CELL_TETRA10;
            }
            if (ensight_starts_with(line, "ria")) {
                if (line[3] == '3') return CFD_CELL_TRIA3;
                if (line[3] == '6') return CFD_CELL_TRIA6;
            }
            break;
        default:
            return CFD_CELL_UNKNOWN;
    }

    return CFD_CELL_UNKNOWN;
}

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_ENSIGHT_GOLD_H */
