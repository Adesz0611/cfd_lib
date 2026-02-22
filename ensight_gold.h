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

#define ENSIGHT_MUTABLE_STRING_SIZE 80
typedef struct {
    u8 buffer[ENSIGHT_MUTABLE_STRING_SIZE];
    u32 len;
} Ensight_MutStr;

typedef struct {
    Ensight_MutStr filename;
    s32 ts, fs;
    b32 ts_set, fs_set;
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
    s32 ts, fs;
    b32 ts_set, fs_set;
    Ensight_MutStr description;
    Ensight_MutStr filename;
} Ensight_Variable;

typedef struct {
    Ensight_Variable *elems;
    u32 len;
} Ensight_VariableArray;

typedef struct {
    s32 time_set_number;
    Ensight_MutStr time_set_description; // Empty string if not specified

    s32 number_of_steps;

    s32 filename_start_number;
    s32 filename_increment;

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

CFD_LIB b32 ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, char *filename, CFD_File *file);
CFD_LIB Ensight_SectionType ensight_get_section_type(Str8 s);

#ifdef CFD_ENSIGHT_GOLD_IMPLEMENTATION

CFD_LIB b32 ensight_parse_case(CFD_Arena *arena, Ensight_Case *encase, char *filename, CFD_File *f) {
    encase->geometry = NULL;
    encase->variable = NULL;
    encase->times = NULL;

    (void)arena;

    cfd_dirname(filename, encase->dirname);
    cfd_info("Dirname: %s", encase->dirname);

    Ensight_SectionType type = ENSIGHT_NOSECTION;

    while(!IS_CFD_FILE_EOF(f)) {
        Str8 line = cfd_file_readline(f);
        if (line.buffer == NULL) {
            cfd_error("error while reading a line!");
            return false;
        }

        type = ensight_get_section_type(line);
        if (type == ENSIGHT_NOSECTION || line.len == 0)
            continue;





    }


    return true;
}

CFD_LIB Ensight_SectionType ensight_get_section_type(Str8 s) {
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

#endif /* CFD_ENSIGHT_GOLD_IMPLEMENTATION */
#endif /* CFD_ENSIGHT_GOLD_H */
