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
    Ensight_VariableType type;
    u32 ts, fs;
    b32 ts_set, fs_set;
    Str8 description;
    Str8 filename;
} Ensight_Variable;

typedef struct {
    Ensight_Variable *elems;
    u32 len;
} Ensight_VariableArray;

typedef struct {
    u32 time_set_number;
    Str8 time_set_description; // Empty string if not specified

    u32 number_of_steps;

    u32 filename_start_number;
    u32 filename_increment;

    float *time_values;
} Ensight_Time;

typedef struct {
    Ensight_Time *elems;
    u32 len;
} Ensight_TimeArray;

typedef struct {
    Ensight_Geometry *geometry;
    Ensight_VariableArray *variable;
    Ensight_TimeArray *times;
    char dirname[PATH_MAX];
} Ensight_Case;

CFD_LIB b32 ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, const char *case_filename, CFD_File *file);

CFD_INTERNAL Ensight_SectionType ensight_get_section_type(Str8 s);
CFD_INTERNAL Str8 cfd_file_ensight_readline(CFD_File *file);
CFD_INTERNAL void ensight_split_key_value(Str8 str, Str8 *key, Str8 *value);
CFD_INTERNAL Str8 ensight_consume_word(Str8 *src);

#ifdef CFD_ENSIGHT_GOLD_IMPLEMENTATION

CFD_LIB b32 ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, const char *case_filename, CFD_File *f) {
    cfd_info("case file size: %zu bytes", f->size);
    CFD_CHECK_NULL(arena);
    CFD_CHECK_NULL(encase);
    CFD_CHECK_NULL(case_filename);
    CFD_CHECK_NULL(f);

    encase->geometry = NULL;
    encase->variable = NULL;
    encase->times = NULL;

    cfd_dirname(case_filename, encase->dirname);
    cfd_info("Dirname: %s", encase->dirname);

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

        Str8 word;

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

                cfd_info("file='%.*s', ts=%d, fs=%d, coords_only=%d", str8_arg(filename), ts, fs, change_coords_only);

                word = ensight_consume_word(&key);

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

                    match->filename = str8_copy(arena, filename);
                }
                else if (str8_equals(word, str8_lit("boundary"))) {
                    if (likely(encase->geometry->boundary == NULL))
                        encase->geometry->boundary = cfd_arena_push_type(arena, Ensight_GeometryElem);
                    else
                        cfd_warn("duplicate 'boundary' keyword found in Ensight Gold file. Previous values will be overwritten.");

                    Ensight_GeometryElem *boundary;
                    boundary = encase->geometry->boundary;

                    boundary->filename = str8_copy(arena, filename);
                } else {
                    cfd_error("invalid key in GEOMETRY section '%.*s' instead of 'model' | 'measured' | 'match' | 'boundary'!", str8_arg(word));
                    return false;
                }

                break;
                }
            case ENSIGHT_VARIABLE:
                break;
            case ENSIGHT_TIME:
                break;
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

#endif /* CFD_ENSIGHT_GOLD_IMPLEMENTATION */
#endif /* CFD_ENSIGHT_GOLD_H */
