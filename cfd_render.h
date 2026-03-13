#ifndef CFD_RENDER_H
#define CFD_RENDER_H

#include "cfd_core.h"

typedef struct {
    u64 key[4]; // Sorted vertex indices (canonical form; key[3] = (u64)-1 for triangle)
    u64 v[4];   // Original vertex indices (winding order; v[3] = (u64)-1 for triangle)
} CFD_Face_Entry;

typedef struct {
    u32 group_idx;
    u64 cell_idx;
} CFD_Cell_Ref;

typedef struct {
    V3 *vertices;
    u64 *indices;
    u64 num_vertices;
    u64 num_triangles;

    // Remapping tables
    u64 *vertex_new_to_old;
    CFD_Cell_Ref *tria_to_cell;
} CFD_Surface_Mesh;

CFD_LIB b32 cfd_extract_surface(CFD_Arena *arena, CFD_Arena *scratch_arena, const CFD_UnstructuredGrid *mesh, CFD_Surface_Mesh *surface);
CFD_LIB b32 cfd_surface_mesh_to_obj_file(CFD_Surface_Mesh *surface, const char *filename);

CFD_INTERNAL u8 cfd_faces_per_cell_type[] = {
    /* [CFD_CELL_POINT]     =*/ 0,
    /* [CFD_CELL_BAR2]      =*/ 0,
    /* [CFD_CELL_BAR3]      =*/ 0,
    /* [CFD_CELL_TRIA3]     =*/ 0,
    /* [CFD_CELL_TRIA6]     =*/ 0,
    /* [CFD_CELL_QUAD4]     =*/ 0,
    /* [CFD_CELL_QUAD8]     =*/ 0,
    /* [CFD_CELL_TETRA4]    =*/ 4,
    /* [CFD_CELL_TETRA10]   =*/ 4,
    /* [CFD_CELL_PYRAMID5]  =*/ 5,
    /* [CFD_CELL_PYRAMID13] =*/ 5,
    /* [CFD_CELL_PENTA6]    =*/ 5,
    /* [CFD_CELL_PENTA15]   =*/ 5,
    /* [CFD_CELL_HEXA8]     =*/ 6,
    /* [CFD_CELL_HEXA20]    =*/ 6,
    /* [CFD_CELL_NSIDED]    =*/ 0,
    /* [CFD_CELL_NFACED]    =*/ 0,
};

/*
 * Face topology tables (VTK convention — outward normals, CCW winding from outside)
 * Higher-order elements (tetra10, hexa20, etc.) use the same corner indices.
 */

CFD_INTERNAL const u8 cfd_tetra_faces[][3] = {
    {0, 1, 3}, {1, 2, 3}, {2, 0, 3}, {0, 2, 1}
};

CFD_INTERNAL const u8 cfd_hexa_faces[][4] = {
    {0, 4, 7, 3}, {1, 2, 6, 5}, {0, 1, 5, 4},
    {3, 7, 6, 2}, {0, 3, 2, 1}, {4, 5, 6, 7}
};

CFD_INTERNAL const u8 cfd_penta_tri_faces[][3] = {
    {0, 1, 2}, {3, 5, 4}
};

CFD_INTERNAL const u8 cfd_penta_quad_faces[][4] = {
    {0, 3, 4, 1}, {1, 4, 5, 2}, {0, 2, 5, 3}
};

CFD_INTERNAL const u8 cfd_pyramid_quad_faces[][4] = {
    {0, 3, 2, 1}
};

CFD_INTERNAL const u8 cfd_pyramid_tri_faces[][3] = {
    {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}
};



CFD_INTERNAL force_inline void cfd_sort3_u64(u64 *a) {
    u64 t;
    if (a[0] > a[1]) { t = a[0]; a[0] = a[1]; a[1] = t; }
    if (a[1] > a[2]) { t = a[1]; a[1] = a[2]; a[2] = t; }
    if (a[0] > a[1]) { t = a[0]; a[0] = a[1]; a[1] = t; }
}

CFD_INTERNAL force_inline void cfd_sort4_u64(u64 *a) {
    u64 t;
    if (a[0] > a[1]) { t = a[0]; a[0] = a[1]; a[1] = t; }
    if (a[2] > a[3]) { t = a[2]; a[2] = a[3]; a[3] = t; }
    if (a[0] > a[2]) { t = a[0]; a[0] = a[2]; a[2] = t; }
    if (a[1] > a[3]) { t = a[1]; a[1] = a[3]; a[3] = t; }
    if (a[1] > a[2]) { t = a[1]; a[1] = a[2]; a[2] = t; }
}

CFD_INTERNAL force_inline void cfd_make_tri_face(CFD_Face_Entry *entry, u64 a, u64 b, u64 c) {
    entry->v[0] = a; entry->v[1] = b; entry->v[2] = c; entry->v[3] = (u64)-1;
    entry->key[0] = a; entry->key[1] = b; entry->key[2] = c; entry->key[3] = (u64)-1;
    cfd_sort3_u64(entry->key);
}

CFD_INTERNAL force_inline void cfd_make_quad_face(CFD_Face_Entry *entry, u64 a, u64 b, u64 c, u64 d) {
    entry->v[0] = a; entry->v[1] = b; entry->v[2] = c; entry->v[3] = d;
    entry->key[0] = a; entry->key[1] = b; entry->key[2] = c; entry->key[3] = d;
    cfd_sort4_u64(entry->key);
}

CFD_INTERNAL force_inline b32 cfd_face_keys_equal(const CFD_Face_Entry *a, const CFD_Face_Entry *b) {
    return a->key[0] == b->key[0] && a->key[1] == b->key[1] &&
           a->key[2] == b->key[2] && a->key[3] == b->key[3];
}

CFD_INTERNAL force_inline u32 cfd_generate_cell_faces(CFD_Cell_Type type, const u64 *cv, CFD_Face_Entry *out) {
    u32 n = 0;

    switch (type) {
        case CFD_CELL_TETRA4:
        case CFD_CELL_TETRA10:
            for (u32 f = 0; f < 4; ++f)
                cfd_make_tri_face(&out[n++], cv[cfd_tetra_faces[f][0]],
                                  cv[cfd_tetra_faces[f][1]], cv[cfd_tetra_faces[f][2]]);
            break;

        case CFD_CELL_HEXA8:
        case CFD_CELL_HEXA20:
            for (u32 f = 0; f < 6; ++f)
                cfd_make_quad_face(&out[n++], cv[cfd_hexa_faces[f][0]], cv[cfd_hexa_faces[f][1]],
                                   cv[cfd_hexa_faces[f][2]], cv[cfd_hexa_faces[f][3]]);
            break;

        case CFD_CELL_PENTA6:
        case CFD_CELL_PENTA15:
            for (u32 f = 0; f < 2; ++f)
                cfd_make_tri_face(&out[n++], cv[cfd_penta_tri_faces[f][0]],
                                  cv[cfd_penta_tri_faces[f][1]], cv[cfd_penta_tri_faces[f][2]]);
            for (u32 f = 0; f < 3; ++f)
                cfd_make_quad_face(&out[n++], cv[cfd_penta_quad_faces[f][0]], cv[cfd_penta_quad_faces[f][1]],
                                   cv[cfd_penta_quad_faces[f][2]], cv[cfd_penta_quad_faces[f][3]]);
            break;

        case CFD_CELL_PYRAMID5:
        case CFD_CELL_PYRAMID13:
            cfd_make_quad_face(&out[n++], cv[cfd_pyramid_quad_faces[0][0]], cv[cfd_pyramid_quad_faces[0][1]],
                               cv[cfd_pyramid_quad_faces[0][2]], cv[cfd_pyramid_quad_faces[0][3]]);
            for (u32 f = 0; f < 4; ++f)
                cfd_make_tri_face(&out[n++], cv[cfd_pyramid_tri_faces[f][0]],
                                  cv[cfd_pyramid_tri_faces[f][1]], cv[cfd_pyramid_tri_faces[f][2]]);
            break;

        default:
            break;
    }

    return n;
}


typedef struct {
    u64 hash;
    u64 face_idx;
} CFD_Face_Hash_Pair;

CFD_INTERNAL force_inline u64 cfd_hash_face_key(const u64 *key) {
    u64 h = key[0] * 0x9E3779B97F4A7C15ULL;
    h ^= key[1] * 0x517CC1B727220A95ULL;
    h ^= key[2] * 0x6C62272E07BB0142ULL;
    h ^= key[3] * 0x62B821756295C58DULL;
    h ^= h >> 32;
    return h;
}


#ifdef CFD_LIB_IMPLEMENTATION

#ifdef _OPENMP
#include <omp.h>
#endif

// LSB radix sort on CFD_Face_Hash_Pair (8-bit radix, 8 passes)
CFD_INTERNAL void cfd_radix_sort_hash_pairs(CFD_Face_Hash_Pair *restrict src, CFD_Face_Hash_Pair *restrict dst, u64 n) {
    for (int pass = 0; pass < 8; ++pass) {
        int shift = pass * 8;

        u64 count[256];
        memset(count, 0, sizeof(count));

        for (u64 i = 0; i < n; ++i)
            count[(src[i].hash >> shift) & 0xFF]++;

        u64 sum = 0;
        for (int b = 0; b < 256; ++b) {
            u64 c = count[b];
            count[b] = sum;
            sum += c;
        }

        for (u64 i = 0; i < n; ++i) {
            u32 b = (src[i].hash >> shift) & 0xFF;
            dst[count[b]++] = src[i];
        }

        CFD_Face_Hash_Pair *t = src; src = dst; dst = t;
    }
}

#ifdef _OPENMP
// Parallel radix sort: per-thread histograms + global prefix sum + parallel scatter
CFD_INTERNAL void cfd_radix_sort_hash_pairs_parallel(
    CFD_Face_Hash_Pair *restrict src,
    CFD_Face_Hash_Pair *restrict dst,
    u64 n,
    u64 *restrict thread_hist, /* num_threads * 256 u64s */
    int num_threads)
{
    for (int pass = 0; pass < 8; ++pass) {
        int shift = pass * 8;

        #pragma omp parallel num_threads(num_threads)
        {
            int tid = omp_get_thread_num();
            u64 *my_hist = thread_hist + (u64)tid * 256;
            memset(my_hist, 0, 256 * sizeof(u64));

            u64 lo = (n * (u64)tid) / (u64)num_threads;
            u64 hi = (n * (u64)(tid + 1)) / (u64)num_threads;

            for (u64 i = lo; i < hi; ++i)
                my_hist[(src[i].hash >> shift) & 0xFF]++;
        }

        {
            u64 sum = 0;
            for (u64 b = 0; b < 256; ++b) {
                for (int t = 0; t < num_threads; ++t) {
                    u64 c = thread_hist[(u64)t * 256 + b];
                    thread_hist[(u64)t * 256 + b] = sum;
                    sum += c;
                }
            }
        }

        #pragma omp parallel num_threads(num_threads)
        {
            int tid = omp_get_thread_num();
            u64 *my_off = thread_hist + (u64)tid * 256;
            u64 lo = (n * (u64)tid) / (u64)num_threads;
            u64 hi = (n * (u64)(tid + 1)) / (u64)num_threads;

            for (u64 i = lo; i < hi; ++i) {
                u32 b = (src[i].hash >> shift) & 0xFF;
                dst[my_off[b]++] = src[i];
            }
        }

        CFD_Face_Hash_Pair *t = src; src = dst; dst = t;
    }
}
#endif


CFD_LIB b32 cfd_extract_surface(CFD_Arena *arena, CFD_Arena *scratch_arena,
                                 const CFD_UnstructuredGrid *mesh,
                                 CFD_Surface_Mesh *surface) {
    CFD_CHECK_NULL(arena, false);
    CFD_CHECK_NULL(scratch_arena, false);
    CFD_CHECK_NULL(mesh, false);
    CFD_CHECK_NULL(surface, false);

    // 1. Count total face entries
    u64 total_faces = 0;
    for (u32 g = 0; g < mesh->num_cell_types; ++g) {
        const CFD_Cell_Group *group = &mesh->cell_groups[g];
        total_faces += group->num_cells * (u64)cfd_faces_per_cell_type[group->type];
    }

    if (total_faces == 0) {
        memset(surface, 0, sizeof(*surface));
        cfd_warn("cfd_extract_surface(): no 3D volume cells found");
        return true;
    }

    cfd_info("total face entries: %zu (%.2f MB scratch)",
             total_faces, (double)(total_faces * sizeof(CFD_Face_Entry)) / (double)MB(1));

    CFD_Face_Entry *faces = cfd_arena_push_array(scratch_arena, CFD_Face_Entry, total_faces);
    if (unlikely(faces == NULL)) {
        cfd_error("scratch arena out of memory for face entries");
        return false;
    }

    CFD_Cell_Ref *face_to_cell = cfd_arena_push_array(scratch_arena, CFD_Cell_Ref, total_faces);
    if (unlikely(face_to_cell == NULL)) {
        cfd_error("scratch arena out of memory for face-to-cell map");
        return false;
    }

    // 2. Generate face entries + cell tracking (OpenMP parallel)
    u64 face_offset = 0;
    for (u32 group_idx = 0; group_idx < mesh->num_cell_types; ++group_idx) {
        const CFD_Cell_Group *group = &mesh->cell_groups[group_idx];
        u32 fpc = cfd_faces_per_cell_type[group->type];
        if (fpc == 0) continue;

        u64 node_count = (u64)cfd_get_cell_node_count(group->type);
        const u64 *conn = group->connectivity;
        u64 num_cells = group->num_cells;
        CFD_Cell_Type type = group->type;

        u64 cell_idx;
#ifdef _OPENMP
        #pragma omp parallel for schedule(static)
#endif
        for (cell_idx = 0; cell_idx < num_cells; ++cell_idx) {
            u64 fo = face_offset + cell_idx * (u64)fpc;
            cfd_generate_cell_faces(type, conn + cell_idx * node_count, faces + fo);
            for (u32 face_idx = 0; face_idx < fpc; ++face_idx) {
                face_to_cell[fo + face_idx].group_idx = group_idx;
                face_to_cell[fo + face_idx].cell_idx  = cell_idx;
            }
        }

        face_offset += num_cells * (u64)fpc;
    }

    // 3. Radix sort for boundary face detection

    // 3a. Compute hash pairs (parallel)
    CFD_Face_Hash_Pair *pairs = cfd_arena_push_array(scratch_arena, CFD_Face_Hash_Pair, total_faces);
    if (unlikely(pairs == NULL)) {
        cfd_error("scratch arena out of memory for hash pairs");
        return false;
    }

    CFD_Face_Hash_Pair *pairs_tmp = cfd_arena_push_array(scratch_arena, CFD_Face_Hash_Pair, total_faces);
    if (unlikely(pairs_tmp == NULL)) {
        cfd_error("scratch arena out of memory for hash pairs sort buffer");
        return false;
    }

    u64 i;
#ifdef _OPENMP
    #pragma omp parallel for schedule(static)
#endif
    for (i = 0; i < total_faces; ++i) {
        pairs[i].hash = cfd_hash_face_key(faces[i].key);
        pairs[i].face_idx = i;
    }

    // 3b. Radix sort by hash (LSB, 8-bit radix, 8 passes)
#ifdef _OPENMP
    int num_threads;
    #pragma omp parallel
    {
        #pragma omp single
        num_threads = omp_get_num_threads();
    }

    u64 *thread_hist = cfd_arena_push_array(scratch_arena, u64, (u64)num_threads * 256);
    if (unlikely(thread_hist == NULL)) {
        cfd_error("scratch arena out of memory for radix sort histograms");
        return false;
    }

    cfd_radix_sort_hash_pairs_parallel(pairs, pairs_tmp, total_faces, thread_hist, num_threads);
    (void)cfd_radix_sort_hash_pairs;
#else
    cfd_radix_sort_hash_pairs(pairs, pairs_tmp, total_faces);
#endif

    // 3c. Linear scan sorted pairs — mark boundary faces.
    u8 *is_boundary = cfd_arena_push_array_zero(scratch_arena, u8, total_faces);
    if (unlikely(is_boundary == NULL)) {
        cfd_error("scratch arena out of memory for boundary flags");
        return false;
    }

    {
        i = 0;
        while (i < total_faces) {
            u64 j = i + 1;
            while (j < total_faces && pairs[j].hash == pairs[i].hash)
                ++j;

            u64 glen = j - i;

            if (glen == 1) {
                // Singleton — definitely boundary
                is_boundary[i] = 1;
            } else if (glen == 2 &&
                       cfd_face_keys_equal(&faces[pairs[i].face_idx],
                                           &faces[pairs[i + 1].face_idx])) {
                // Common case: matched pair — internal, skip both
            } else {
                // Rare: hash collision or >2 cells sharing a face
                for (u64 a = i; a < j; ++a) {
                    b32 has_match = false;
                    for (u64 b = i; b < j; ++b) {
                        if (a != b && cfd_face_keys_equal(&faces[pairs[a].face_idx],
                                                          &faces[pairs[b].face_idx])) {
                            has_match = true;
                            break;
                        }
                    }
                    if (!has_match)
                        is_boundary[a] = 1;
                }
            }

            i = j;
        }
    }

    // 4. Count boundary triangles
    u64 num_boundary_tris = 0;
    for (i = 0; i < total_faces; ++i) {
        if (is_boundary[i]) {
            const CFD_Face_Entry *face = &faces[pairs[i].face_idx];
            num_boundary_tris += (face->v[3] != (u64)-1) ? 2 : 1;
        }
    }

    if (num_boundary_tris == 0) {
        memset(surface, 0, sizeof(*surface));
        cfd_warn("cfd_extract_surface(): no boundary faces found (fully enclosed mesh?)");
        return true;
    }

    cfd_info("boundary triangles: %zu", num_boundary_tris);

    surface->num_triangles = num_boundary_tris;
    surface->indices = cfd_arena_push_array(arena, u64, num_boundary_tris * 3);
    if (unlikely(surface->indices == NULL)) {
        cfd_error("failed to allocate surface index buffer");
        return false;
    }

    surface->tria_to_cell = cfd_arena_push_array(arena, CFD_Cell_Ref, num_boundary_tris);
    if (unlikely(surface->tria_to_cell == NULL)) {
        cfd_error("failed to allocate tri-to-cell map");
        return false;
    }

    // 5. Extract boundary triangles (old vertex indices)
    {
        u64 *restrict out = surface->indices;
        CFD_Cell_Ref *restrict tcell = surface->tria_to_cell;
        u64 tri = 0;

        for (i = 0; i < total_faces; ++i) {
            if (!is_boundary[i]) continue;

            u64 fi = pairs[i].face_idx;
            const CFD_Face_Entry *face = &faces[fi];
            CFD_Cell_Ref cell = face_to_cell[fi];

            out[tri * 3 + 0] = face->v[0];
            out[tri * 3 + 1] = face->v[1];
            out[tri * 3 + 2] = face->v[2];
            tcell[tri] = cell;
            ++tri;

            // Quad face: split 0-1-2-3 → tri(0,1,2) + tri(0,2,3)
            if (face->v[3] != (u64)-1) {
                out[tri * 3 + 0] = face->v[0];
                out[tri * 3 + 1] = face->v[2];
                out[tri * 3 + 2] = face->v[3];
                tcell[tri] = cell;
                ++tri;
            }
        }
    }

    // 6. Compact vertices and remap indices

    cfd_arena_reset(scratch_arena);
    u64 *vtx_old_to_new = cfd_arena_push_array(scratch_arena, u64, mesh->num_vertices);
    if (unlikely(vtx_old_to_new == NULL)) {
        cfd_error("scratch arena out of memory for vertex remap");
        return false;
    }
    memset(vtx_old_to_new, 0xFF, mesh->num_vertices * sizeof(u64));

    u64 num_verts = 0;
    for (u64 t = 0; t < num_boundary_tris * 3; ++t) {
        u64 old_v = surface->indices[t];
        if (vtx_old_to_new[old_v] == (u64)-1)
            vtx_old_to_new[old_v] = num_verts++;
    }

    surface->num_vertices = num_verts;
    surface->vertex_new_to_old = cfd_arena_push_array(arena, u64, num_verts);
    surface->vertices = cfd_arena_push_array(arena, V3, num_verts);
    if (unlikely(surface->vertex_new_to_old == NULL || surface->vertices == NULL)) {
        cfd_error("failed to allocate compacted vertex data");
        return false;
    }

    for (u64 old_v = 0; old_v < mesh->num_vertices; ++old_v) {
        u64 new_v = vtx_old_to_new[old_v];
        if (new_v != (u64)-1) {
            surface->vertex_new_to_old[new_v] = old_v;
            surface->vertices[new_v] = mesh->vertices[old_v];
        }
    }

    for (u64 t = 0; t < num_boundary_tris * 3; ++t)
        surface->indices[t] = vtx_old_to_new[surface->indices[t]];

    cfd_info("surface: %zu vertices, %zu triangles", num_verts, num_boundary_tris);

    return true;
}

CFD_LIB b32 cfd_surface_mesh_to_obj_file(CFD_Surface_Mesh *surface, const char *export_filename) {
    FILE *f = fopen(export_filename, "w");
    if (f == NULL) {
#ifdef _MSC_VER
        char error_str[256];
        strerror_s(error_str, 256, errno);
        cfd_error("couldn't open '%s' file for writing: %s", export_filename, error_str);
#else
        cfd_error("couldn't open '%s' file for writing: %s", export_filename, strerror(errno));
#endif
        return false;
    }

    const u64 num_vertices = surface->num_vertices;
    const u64 num_triangles = surface->num_triangles;
    u64 *indices = surface->indices;

    for (u64 vertex_idx = 0; vertex_idx < num_vertices; ++vertex_idx) {
        V3 *v = &surface->vertices[vertex_idx];
        fprintf(f, "v %f %f %f\n", (double)v->x, (double)v->y, (double)v->z);
    }

    for (u64 tria_idx = 0; tria_idx < num_triangles; ++tria_idx) {
        u64 *p = &indices[tria_idx * 3];
        fprintf(f, "f %zu %zu %zu\n", p[0] + 1, p[1] + 1, p[2] + 1);
    }


    fclose(f);
    return true;
}

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_RENDER_H */
