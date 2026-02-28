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
    Str8 filename;
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
} Ensight_Case;

typedef struct {
    u32 var_count;
    u32 time_count;
} Ensight_Case_Sizes;

CFD_LIB b32 ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, const char *case_filename, CFD_File *file);

CFD_INTERNAL Ensight_SectionType ensight_get_section_type(Str8 s);
CFD_INTERNAL Str8 cfd_file_ensight_readline(CFD_File *file);
CFD_INTERNAL void ensight_split_key_value(Str8 str, Str8 *key, Str8 *value);
CFD_INTERNAL Str8 ensight_consume_word(Str8 *src);
CFD_INTERNAL Ensight_Case_Sizes ensight_get_case_sizes(CFD_File file);

#ifdef CFD_LIB_IMPLEMENTATION

CFD_LIB b32 ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, const char *case_filename, CFD_File *f) {
    CFD_CHECK_NULL(arena);
    CFD_CHECK_NULL(encase);
    CFD_CHECK_NULL(case_filename);
    CFD_CHECK_NULL(f);
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


    return true;
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

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_ENSIGHT_GOLD_H */
