#ifndef CFD_SLICER_H
#define CFD_SLICER_H

#include "cfd_core.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#if defined(__has_include)
    #if __has_include(<immintrin.h>)
        #include <immintrin.h>
    #endif
#endif

#if defined(__AVX__) || (defined(_MSC_VER) && defined(__AVX__))
#else
#error "This file requires AVX-enabled compilation (for example /arch:AVX or equivalent), or a scalar fallback implementation."
#endif

#ifndef CFD_POINT_NOT_FOUND_VALUE
#define CFD_POINT_NOT_FOUND_VALUE 0.0f
#endif

typedef struct CFD_Point_Locate_Result {
    b32 found;
    u32 group_idx;
    u32 cell_idx;
    f32 w0, w1, w2, w3;
} CFD_Point_Locate_Result;

typedef struct MortonItem {
    u64 code;
    u32 cell_idx;
    u32 group_idx;
} MortonItem;

#define BVH_EMPTY_CHILD ((s64)INT64_MAX)
#define BVH_CHILDREN_COUNT 8
typedef struct BVH8Node {
    f32 min_x[BVH_CHILDREN_COUNT], max_x[BVH_CHILDREN_COUNT];
    f32 min_y[BVH_CHILDREN_COUNT], max_y[BVH_CHILDREN_COUNT];
    f32 min_z[BVH_CHILDREN_COUNT], max_z[BVH_CHILDREN_COUNT];

    s64 children[BVH_CHILDREN_COUNT]; // < 0: leaf range start encoded as ~start, == BVH_EMPTY_CHILD: empty slot, >= 0: internal child node index
    u32 leaf_counts[BVH_CHILDREN_COUNT]; // number of cells in the leaf (0 if the child is not a leaf)
} BVH8Node;

typedef struct {
    BVH8Node *nodes;
    MortonItem *items;
    u64 total_cells;
    u32 node_count;
    s64 root;
    u32 root_leaf_count;
} CFD_BVH8Tree;

typedef struct {
    BVH8Node *nodes;
    volatile u32 node_count;
    u32 max_nodes;
} BVH_Build_Context;

CFD_LIB CFD_BVH8Tree cfd_build_grid_bvh8(CFD_Arena *arena, CFD_Arena *scratch_arena, CFD_UnstructuredGrid *mesh);
CFD_LIB CFD_Point_Locate_Result cfd_bvh8_locate_point_tetra(const CFD_BVH8Tree *tree, const CFD_UnstructuredGrid *mesh, V3 p, f32 eps);
CFD_LIB b32 cfd_sample_point_tetra_node_data(
    const CFD_BVH8Tree *tree,
    const CFD_UnstructuredGrid *mesh,
    const f32 *node_data,
    u8 dim,
    V3 p,
    f32 eps,
    f32 *out_value
);
// the size of out_values must be: point_count * dim
CFD_LIB b32 cfd_sample_points_tetra_node_data_parallel(
    const CFD_BVH8Tree *tree,
    const CFD_UnstructuredGrid *mesh,
    const f32 *node_data,
    u8 dim,
    const V3 *points,
    u64 point_count,
    f32 eps,
    f32 *out_values
);
CFD_LIB BVH8Node *cfd_build_bvh8_parallel(CFD_Arena *restrict arena, MortonItem *restrict items, u64 n, u32 *restrict out_num_nodes, s64 *restrict out_root);
CFD_LIB void cfd_bvh8_refit_parallel(BVH_Build_Context *ctx, MortonItem *items, CFD_UnstructuredGrid *mesh);

CFD_LIB b32 cfd_generate_xy_plane_mesh(
    V3 aabb_min,
    V3 aabb_max,
    f32 z,
    u32 res_x, u32 res_y,
    V3 *out_vertices,
    u32 *out_conn
);

#ifdef CFD_LIB_IMPLEMENTATION

#if defined(_MSC_VER)
    #include <intrin.h>
    #pragma intrinsic(_BitScanReverse64)

    static force_inline int cfd_highest_set_bit_u64(u64 mask) {
        unsigned long where;
        _BitScanReverse64(&where, mask);
        return (int)where;
    }
#elif defined(__GNUC__) || defined(__clang__)
    static force_inline int cfd_highest_set_bit_u64(u64 mask) {
        return 63 - __builtin_clzll(mask);
    }
#else
    #error "Unknown compiler!"
#endif


CFD_INTERNAL force_inline b32 cfd_point_in_aabb_eps(
    V3 p,
    f32 min_x, f32 min_y, f32 min_z,
    f32 max_x, f32 max_y, f32 max_z,
    f32 eps
) {
    return (p.x >= min_x - eps && p.x <= max_x + eps) &&
           (p.y >= min_y - eps && p.y <= max_y + eps) &&
           (p.z >= min_z - eps && p.z <= max_z + eps);
}

CFD_INTERNAL b32 cfd_point_in_tetra_barycentric(
    V3 p, V3 a, V3 b, V3 c, V3 d,
    f32 eps,
    f32 *out_w0, f32 *out_w1, f32 *out_w2, f32 *out_w3
) {
    V3 ab = v3_sub(b, a);
    V3 ac = v3_sub(c, a);
    V3 ad = v3_sub(d, a);
    V3 ap = v3_sub(p, a);

    f32 det = v3_dot(ab, v3_cross(ac, ad));
    if (det > -eps && det < eps) return false;

    f32 inv_det = 1.0f / det;

    f32 w1 = v3_dot(ap, v3_cross(ac, ad)) * inv_det;
    f32 w2 = v3_dot(ab, v3_cross(ap, ad)) * inv_det;
    f32 w3 = v3_dot(ab, v3_cross(ac, ap)) * inv_det;
    f32 w0 = 1.0f - w1 - w2 - w3;

    if (w0 >= -eps && w1 >= -eps && w2 >= -eps && w3 >= -eps) {
        if (out_w0) *out_w0 = w0;
        if (out_w1) *out_w1 = w1;
        if (out_w2) *out_w2 = w2;
        if (out_w3) *out_w3 = w3;
        return true;
    }

    return false;
}

#define SLICER_RADIX_BITS 11
#define SLICER_NUM_BUCKETS (1 << SLICER_RADIX_BITS)
#define SLICER_RADIX_MASK (SLICER_NUM_BUCKETS - 1)


#if defined(__BMI2__)
#define MORTON_USE_BMI2
#endif

CFD_INTERNAL force_inline u64 cfd_expand_bits(u32 v) {
#ifdef MORTON_USE_BMI2
    return _pdep_u64(v, 0x1249249249249249ULL);
#else
    u64 x = v & 0x1fffffULL;
    x = (x | x << 32) & 0x1f00000000ffffULL;
    x = (x | x << 16) & 0x1f0000ff0000ffULL;
    x = (x | x << 8)  & 0x100f00f00f00f00fULL;
    x = (x | x << 4)  & 0x10c30c30c30c30c3ULL;
    x = (x | x << 2)  & 0x1249249249249249ULL;
    return x;
#endif
}

CFD_INTERNAL MortonItem *cfd_compute_morton_from_centroids(CFD_Arena *restrict arena, CFD_UnstructuredGrid *restrict mesh) {
    CFD_CHECK_NULL(arena, NULL);
    CFD_CHECK_NULL(mesh, NULL);

    const u32 num_cell_types = mesh->num_cell_types;
    V3 *restrict vertices = mesh->vertices;

    u64 total_cells = 0;
    for (u32 group_idx = 0; group_idx < num_cell_types; ++group_idx)
        total_cells += mesh->cell_groups[group_idx].num_cells;

    MortonItem *items = cfd_arena_push_array(arena, MortonItem, total_cells);
    if (unlikely(items == NULL)) return NULL;


    V3 aabb_min, aabb_max;
    if (!mesh->has_aabb) {
        cfd_calculate_aabb(mesh->vertices, mesh->num_vertices, &aabb_min, &aabb_max);
        mesh->aabb_min = aabb_min;
        mesh->aabb_max = aabb_max;
        mesh->has_aabb = true;
    } else {
        aabb_min = mesh->aabb_min;
        aabb_max = mesh->aabb_max;
    }

    V3 inv_range;
    inv_range.x = (aabb_max.x > aabb_min.x) ? 1.0f / (aabb_max.x - aabb_min.x) : 0.0f;
    inv_range.y = (aabb_max.y > aabb_min.y) ? 1.0f / (aabb_max.y - aabb_min.y) : 0.0f;
    inv_range.z = (aabb_max.z > aabb_min.z) ? 1.0f / (aabb_max.z - aabb_min.z) : 0.0f;

    u64 global_cell_offset = 0;


    for (u32 group_idx = 0; group_idx < num_cell_types; ++group_idx) {
        CFD_Cell_Group *restrict group = &mesh->cell_groups[group_idx];

        u64 num_cells = group->num_cells;
        u8 node_count = cfd_get_cell_node_count(group->type);
        if (unlikely(node_count == 0)) {
            cfd_error("Unsupported cell type in BVH build: group %u, type %u has variable/unknown node count.", group_idx, (u32)group->type);
            return NULL;
        }
        f32 node_count_inv = 1.0f / (f32)node_count;
        u64 *restrict connectivity = group->connectivity;

        u64 cell_idx;
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (cell_idx = 0; cell_idx < num_cells; ++cell_idx) {
            f32 cx = 0, cy = 0, cz = 0;
            u64 cell_base = cell_idx * (u64)node_count;

            for (u8 vert_idx = 0; vert_idx < node_count; ++vert_idx) {
                V3 v = vertices[connectivity[cell_base + (u64)vert_idx]];

                cx += v.x;
                cy += v.y;
                cz += v.z;
            }

            cx *= node_count_inv;
            cy *= node_count_inv;
            cz *= node_count_inv;

            f32 fx = (cx - aabb_min.x) * inv_range.x * 2097151.0f;
            f32 fy = (cy - aabb_min.y) * inv_range.y * 2097151.0f;
            f32 fz = (cz - aabb_min.z) * inv_range.z * 2097151.0f;

            if (fx < 0.0f) fx = 0.0f; else if (fx > 2097151.0f) fx = 2097151.0f;
            if (fy < 0.0f) fy = 0.0f; else if (fy > 2097151.0f) fy = 2097151.0f;
            if (fz < 0.0f) fz = 0.0f; else if (fz > 2097151.0f) fz = 2097151.0f;

            u32 ux = (u32)fx;
            u32 uy = (u32)fy;
            u32 uz = (u32)fz;

            u64 idx = global_cell_offset + cell_idx;
            items[idx].code = (cfd_expand_bits(ux) << 2) | (cfd_expand_bits(uy) << 1) | cfd_expand_bits(uz);
            items[idx].cell_idx = (u32)cell_idx;
            items[idx].group_idx = group_idx;
        }

        global_cell_offset += num_cells;
    }


    return items;
}

CFD_INTERNAL b32 cfd_radix_sort_morton_parallel(CFD_Arena *restrict scratch_arena, MortonItem *restrict items, u64 n) {
    MortonItem *current_in  = items;
    u64 scratch_save = scratch_arena->offset;
    MortonItem *current_out = cfd_arena_push_array(scratch_arena, MortonItem, n);
    if (unlikely(current_out == NULL)) {
        scratch_arena->offset = scratch_save;
        return false;
    }

    int max_threads = 1;
#ifdef _OPENMP
    max_threads = omp_get_max_threads();
#endif

    u64 *thread_histograms = cfd_arena_push_array(scratch_arena, u64, (u64)max_threads * SLICER_NUM_BUCKETS);
    if (unlikely(thread_histograms == NULL)) {
        scratch_arena->offset = scratch_save;
        return false;
    }

    // 64 bit / 11 bit = 6 pass (5.8)
    for (u32 pass = 0; pass < 6; ++pass) {
        u32 shift = pass * SLICER_RADIX_BITS;
        memset(thread_histograms, 0, (u64)max_threads * SLICER_NUM_BUCKETS * sizeof(u64));

#ifdef _OPENMP
#pragma omp parallel num_threads(max_threads)
#endif
        {
            int tid = 0;
            #ifdef _OPENMP
            tid = omp_get_thread_num();
            #endif

            u64 *local_hist = &thread_histograms[tid * SLICER_NUM_BUCKETS];

            u64 i;
#ifdef _OPENMP
#pragma omp for schedule(static)
#endif
            for (i = 0; i < n; ++i) {
                u32 bucket = (current_in[i].code >> shift) & SLICER_RADIX_MASK;
                ++local_hist[bucket];
            }

#ifdef _OPENMP
#pragma omp barrier
#endif

#ifdef _OPENMP
#pragma omp single
#endif
            {
                u64 total_offset = 0;
                for (u32 b = 0; b < SLICER_NUM_BUCKETS; ++b) {
                    for (int t = 0; t < max_threads; ++t) {
                        u32 idx = (u32)t * SLICER_NUM_BUCKETS + b;
                        u64 count = thread_histograms[idx];
                        thread_histograms[idx] = total_offset;
                        total_offset += count;
                    }
                }
            }

#ifdef _OPENMP
#pragma omp for schedule(static)
#endif
            for (i = 0; i < n; ++i) {
                u32 bucket = (current_in[i].code >> shift) & SLICER_RADIX_MASK;
                u64 dest_idx = thread_histograms[(u32)tid * SLICER_NUM_BUCKETS + bucket]++;
                current_out[dest_idx] = current_in[i];
            }
        }



        MortonItem *temp = current_in;
        current_in = current_out;
        current_out = temp;
    }

    if (current_in != items)
        memcpy(items, current_in, n * sizeof(MortonItem));

    scratch_arena->offset = scratch_save;
    return true;
}

CFD_INTERNAL u64 cfd_find_morton_threshold(MortonItem *items, u64 start, u64 end, u64 threshold) {
    u64 low  = start;
    u64 high = end;

    while (low < high) {
        u64 mid = low + (high - low) / 2;
        if (items[mid].code < threshold) low = mid + 1;
        else                             high = mid;
    }

    return low;
}

CFD_INTERNAL void cfd_find_splits_8way(MortonItem *items, u64 start, u64 end, u64 *out_splits) {
    out_splits[0] = start;
    out_splits[8] = end;

    u64 first_code = items[start].code;
    u64 last_code  = items[end-1].code;
    u64 diff = first_code ^ last_code;

    if (diff == 0) {
        for (u64 i = 0; i < 8; ++i)
            out_splits[i] = start + (i * (end - start) / 8);

        return;
    }

    int highest_bit = cfd_highest_set_bit_u64(diff);
    int shift = (highest_bit / 3) * 3;

    u64 common_prefix = first_code >> (shift + 3);
    for (u64 i = 1; i < 8; ++i) {
        u64 threshold = ((common_prefix << 3) + i) << shift;
        out_splits[i] = cfd_find_morton_threshold(items, out_splits[i-1], end, threshold);
    }
}

CFD_INTERNAL void cfd_build_bvh8_recursive(BVH_Build_Context *ctx, MortonItem *items, u64 start, u64 end, u32 max_leaf_size, s64 *output_ptr) {
    u64 count = end - start;

    if (count <= max_leaf_size) {
        *output_ptr = ~(s64)start;
        return;
    }

    u64 splits[9];
    cfd_find_splits_8way(items, start, end, splits);

    u32 non_empty = 0;
    for (int i = 0; i < 8; ++i)
        if (splits[i + 1] > splits[i])
            ++non_empty;

    if (non_empty <= 1) {
        *output_ptr = ~(s64)start;
        return;
    }

    u32 node_idx_u32 = cfd_atomic_add_u32(&ctx->node_count, 1);
    if (unlikely(node_idx_u32 >= ctx->max_nodes)) {
        cfd_error("BVH node allocation overflow: node_idx=%u, max_nodes=%u", node_idx_u32, ctx->max_nodes);
        *output_ptr = BVH_EMPTY_CHILD;
        return;
    }

    s32 node_idx = (s32)node_idx_u32;
    *output_ptr = (s64)node_idx;

    BVH8Node *node = &ctx->nodes[node_idx];
    for (int i = 0; i < 8; ++i) {
        node->children[i]    = BVH_EMPTY_CHILD;
        node->leaf_counts[i] = 0;
        node->min_x[i] = node->min_y[i] = node->min_z[i] =  FLT_MAX;
        node->max_x[i] = node->max_y[i] = node->max_z[i] = -FLT_MAX;
    }

    for (int i = 0; i < 8; ++i) {
        u64 c_start = splits[i];
        u64 c_end   = splits[i + 1];

        if (c_end > c_start) {
            #ifdef _OPENMP
            #pragma omp task shared(ctx, items) if (c_end - c_start > 4096)
            #endif
            {
                cfd_build_bvh8_recursive(ctx, items, c_start, c_end, max_leaf_size, &node->children[i]);

                if (node->children[i] < 0)
                    node->leaf_counts[i] = (u32)(c_end - c_start);
            }
        }
    }

    #ifdef _OPENMP
    #pragma omp taskwait
    #endif
}

CFD_LIB BVH8Node *cfd_build_bvh8_parallel(CFD_Arena *restrict arena, MortonItem *restrict items, u64 n, u32 *restrict out_num_nodes, s64 *restrict out_root) {
    u32 max_nodes = (u32)n;

    BVH8Node *node_array = (BVH8Node *)cfd_arena_alloc_aligned(arena, max_nodes * sizeof(BVH8Node), 64);
    if (unlikely(node_array == NULL)) return NULL;

    BVH_Build_Context ctx;
    ctx.nodes = node_array;
    ctx.node_count = 0;
    ctx.max_nodes = max_nodes;

    s64 root_idx = BVH_EMPTY_CHILD;

    #ifdef _OPENMP
    #pragma omp parallel
    #endif
    {
        #ifdef _OPENMP
        #pragma omp single
        #endif
        {
            cfd_build_bvh8_recursive(&ctx, items, 0, n, 4, &root_idx);
        }
    }

    *out_num_nodes = ctx.node_count;
    if (out_root) *out_root = root_idx;
    return node_array;
}

CFD_INTERNAL force_inline float cfd_hmin_avx(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 m1 = _mm_min_ps(lo, hi);

    __m128 m2 = _mm_min_ps(m1, _mm_movehl_ps(m1, m1));
    __m128 m3 = _mm_min_ps(m2, _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(1, 1, 1, 1)));

    return _mm_cvtss_f32(m3);
}

CFD_INTERNAL force_inline float cfd_hmax_avx(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_max_ps(lo, hi);
    lo = _mm_max_ps(lo, _mm_shuffle_ps(lo, lo, _MM_SHUFFLE(1, 0, 3, 2)));
    lo = _mm_max_ps(lo, _mm_shuffle_ps(lo, lo, _MM_SHUFFLE(2, 3, 0, 1)));
    return _mm_cvtss_f32(lo);
}

CFD_INTERNAL void cfd_get_node_combined_aabb_simd(BVH8Node *node, V3 *out_min, V3 *out_max) {
    __m256 mx = _mm256_load_ps(node->min_x);
    __m256 xx = _mm256_load_ps(node->max_x);
    __m256 my = _mm256_load_ps(node->min_y);
    __m256 xy = _mm256_load_ps(node->max_y);
    __m256 mz = _mm256_load_ps(node->min_z);
    __m256 xz = _mm256_load_ps(node->max_z);

    out_min->x = cfd_hmin_avx(mx);
    out_max->x = cfd_hmax_avx(xx);
    out_min->y = cfd_hmin_avx(my);
    out_max->y = cfd_hmax_avx(xy);
    out_min->z = cfd_hmin_avx(mz);
    out_max->z = cfd_hmax_avx(xz);
}

CFD_LIB void cfd_bvh8_refit_parallel(BVH_Build_Context *ctx, MortonItem *items, CFD_UnstructuredGrid *mesh) {
    s32 num_nodes = (s32)ctx->node_count;
    if (num_nodes == 0) return;

    s32 i;
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 64)
#endif
    for (i = 0; i < num_nodes; ++i) {
        BVH8Node *node = &ctx->nodes[i];

        for (int j = 0; j < 8; ++j) {
            if (node->children[j] < 0) {
                u64 leaf_start = (u64)(~node->children[j]);
                u32 leaf_count = node->leaf_counts[j];

                V3 bmin = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
                V3 bmax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

                for (u32 k = 0; k < leaf_count; ++k) {
                    u32 cell_idx = items[leaf_start + k].cell_idx;
                    u32 group_idx = items[leaf_start + k].group_idx;

                    V3 cmin, cmax;
                    cfd_get_cell_aabb(mesh, group_idx, cell_idx, &cmin, &cmax);

                    if (cmin.x < bmin.x) bmin.x = cmin.x;
                    if (cmin.y < bmin.y) bmin.y = cmin.y;
                    if (cmin.z < bmin.z) bmin.z = cmin.z;
                    if (cmax.x > bmax.x) bmax.x = cmax.x;
                    if (cmax.y > bmax.y) bmax.y = cmax.y;
                    if (cmax.z > bmax.z) bmax.z = cmax.z;
                }

                node->min_x[j] = bmin.x; node->max_x[j] = bmax.x;
                node->min_y[j] = bmin.y; node->max_y[j] = bmax.y;
                node->min_z[j] = bmin.z; node->max_z[j] = bmax.z;

            }
        }
    }

    for (i = num_nodes - 1; i >= 0; --i) {
        BVH8Node *node = &ctx->nodes[i];

        for (int j = 0; j < 8; ++j) {
            s64 child_idx = node->children[j];

            if (child_idx == BVH_EMPTY_CHILD) {
                continue;
            } else if (child_idx < 0) {
                continue;
            } else if (child_idx < num_nodes) {
                V3 cmin, cmax;

                cfd_get_node_combined_aabb_simd(&ctx->nodes[child_idx], &cmin, &cmax);

                node->min_x[j] = cmin.x; node->max_x[j] = cmax.x;
                node->min_y[j] = cmin.y; node->max_y[j] = cmax.y;
                node->min_z[j] = cmin.z; node->max_z[j] = cmax.z;
            }

        }
    }
}

CFD_LIB CFD_BVH8Tree cfd_build_grid_bvh8(CFD_Arena *arena, CFD_Arena *scratch_arena, CFD_UnstructuredGrid *mesh) {
    CFD_BVH8Tree tree;
    memset(&tree, 0, sizeof(tree));
    tree.root = BVH_EMPTY_CHILD;

    CFD_CHECK_NULL(arena, tree);
    CFD_CHECK_NULL(scratch_arena, tree);
    CFD_CHECK_NULL(mesh, tree);

    u64 total_cells = 0;
    for (u32 group_idx = 0; group_idx < mesh->num_cell_types; ++group_idx) {
        total_cells += mesh->cell_groups[group_idx].num_cells;
    }

    if (unlikely(total_cells == 0)) return tree;

    MortonItem *items = cfd_compute_morton_from_centroids(arena, mesh);
    if (unlikely(items == NULL)) return tree;

    if (unlikely(!cfd_radix_sort_morton_parallel(scratch_arena, items, total_cells))) {
        return tree;
    }

    u32 num_nodes = 0;
    s64 root = BVH_EMPTY_CHILD;
    BVH8Node *nodes = cfd_build_bvh8_parallel(arena, items, total_cells, &num_nodes, &root);
    if (unlikely(nodes == NULL)) return tree;

    BVH_Build_Context ctx;
    ctx.nodes = nodes;
    ctx.node_count = num_nodes;
    ctx.max_nodes = (u32)total_cells;

    cfd_bvh8_refit_parallel(&ctx, items, mesh);

    tree.nodes = nodes;
    tree.node_count = num_nodes;
    tree.items = items;
    tree.total_cells = total_cells;
    tree.root = root;
    tree.root_leaf_count = (root < 0) ? (u32)total_cells : 0;

    return tree;
}

CFD_LIB CFD_Point_Locate_Result cfd_bvh8_locate_point_tetra(const CFD_BVH8Tree *tree, const CFD_UnstructuredGrid *mesh, V3 p, f32 eps) {
    CFD_Point_Locate_Result result;
    memset(&result, 0, sizeof(result));
    result.group_idx = 0xFFFFFFFFu;
    result.cell_idx  = 0xFFFFFFFFu;

    CFD_CHECK_NULL(tree, result);
    CFD_CHECK_NULL(mesh, result);

    if (tree->root == BVH_EMPTY_CHILD || tree->total_cells == 0)
        return result;

    s64 stack[128];
    s32 stack_count = 0;

    stack[stack_count++] = tree->root;

    while (stack_count > 0) {
        s64 node_ref = stack[--stack_count];

        if (node_ref < 0) {
            if (unlikely(tree->root_leaf_count == 0)) {
                cfd_error("Invalid BVH root leaf state: root is leaf but root_leaf_count is 0.");
                return result;
            }

            u64 leaf_start = (u64)(~node_ref);
            u64 leaf_end = leaf_start + tree->root_leaf_count;

            for (u64 item_idx = leaf_start; item_idx < leaf_end; ++item_idx) {
                const MortonItem *item = &tree->items[item_idx];
                const CFD_Cell_Group *group = &mesh->cell_groups[item->group_idx];

                if (group->type != CFD_CELL_TETRA4) continue;

                const u64 *conn = &group->connectivity[(u64)item->cell_idx * 4ULL];

                V3 a = mesh->vertices[conn[0]];
                V3 b = mesh->vertices[conn[1]];
                V3 c = mesh->vertices[conn[2]];
                V3 d = mesh->vertices[conn[3]];

                f32 w0, w1, w2, w3;
                if (cfd_point_in_tetra_barycentric(p, a, b, c, d, eps, &w0, &w1, &w2, &w3)) {
                    result.found = true;
                    result.group_idx = item->group_idx;
                    result.cell_idx = item->cell_idx;
                    result.w0 = w0;
                    result.w1 = w1;
                    result.w2 = w2;
                    result.w3 = w3;
                    return result;
                }
            }

            continue;
        }

        if ((u32)node_ref >= tree->node_count) continue;

        const BVH8Node *node = &tree->nodes[node_ref];

        for (int child_i = 0; child_i < 8; ++child_i) {
            s64 child = node->children[child_i];

            if (child == BVH_EMPTY_CHILD) continue;

            if (!cfd_point_in_aabb_eps(
                    p,
                    node->min_x[child_i], node->min_y[child_i], node->min_z[child_i],
                    node->max_x[child_i], node->max_y[child_i], node->max_z[child_i],
                    eps
                )) {

                continue;
            }

            if (child < 0) {
                u64 leaf_start = (u64)(~child);
                u64 leaf_count = node->leaf_counts[child_i];

                for (u32 k = 0; k < leaf_count; ++k) {
                    const MortonItem *item = &tree->items[leaf_start + k];
                    const CFD_Cell_Group *group = &mesh->cell_groups[item->group_idx];

                    if (group->type != CFD_CELL_TETRA4) continue;

                    const u64 *conn = &group->connectivity[(u64)item->cell_idx * 4ULL];

                    V3 a = mesh->vertices[conn[0]];
                    V3 b = mesh->vertices[conn[1]];
                    V3 c = mesh->vertices[conn[2]];
                    V3 d = mesh->vertices[conn[3]];

                    f32 w0, w1, w2, w3;
                    if (cfd_point_in_tetra_barycentric(p, a, b, c, d, eps, &w0, &w1, &w2, &w3)) {
                        result.found = true;
                        result.group_idx = item->group_idx;
                        result.cell_idx = item->cell_idx;
                        result.w0 = w0;
                        result.w1 = w1;
                        result.w2 = w2;
                        result.w3 = w3;
                        return result;
                    }
                }

            } else {
                if (stack_count < (s32)(sizeof(stack) / sizeof(stack[0]))) {
                    stack[stack_count++] = child;
                } else {
                    cfd_error("BVH traversal stack overflow in cfd_bvh8_locate_point_tetra().");
                    return result;
                }
            }
        }
    }

    return result;
}

CFD_LIB b32 cfd_sample_point_tetra_node_data(
    const CFD_BVH8Tree *tree,
    const CFD_UnstructuredGrid *mesh,
    const f32 *node_data,
    u8 dim,
    V3 p,
    f32 eps,
    f32 *out_value
) {
    CFD_CHECK_NULL(tree, false);
    CFD_CHECK_NULL(mesh, false);
    CFD_CHECK_NULL(node_data, false);
    CFD_CHECK_NULL(out_value, false);

    if (unlikely(dim == 0)) {
        cfd_error("cfd_sample_point_tetra_node_data(): dim must be > 0.");
        return false;
    }

    CFD_Point_Locate_Result hit = cfd_bvh8_locate_point_tetra(tree, mesh, p, eps);

    if (!hit.found) {
        for (u8 d = 0; d < dim; ++d)
            out_value[d] = CFD_POINT_NOT_FOUND_VALUE;
        return false;
    }

    const CFD_Cell_Group *group = &mesh->cell_groups[hit.group_idx];
    if (group->type != CFD_CELL_TETRA4) {
        for (u8 d = 0; d < dim; ++d)
            out_value[d] = CFD_POINT_NOT_FOUND_VALUE;
        return false;
    }

    const u64 *conn = &group->connectivity[(u64)hit.cell_idx * 4ULL];

    u64 i0 = conn[0];
    u64 i1 = conn[1];
    u64 i2 = conn[2];
    u64 i3 = conn[3];

    for (u8 d = 0; d < dim; ++d) {
        out_value[d] =
            hit.w0 * node_data[i0 * dim + d] +
            hit.w1 * node_data[i1 * dim + d] +
            hit.w2 * node_data[i2 * dim + d] +
            hit.w3 * node_data[i3 * dim + d];
    }

    return true;
}

CFD_LIB b32 cfd_sample_points_tetra_node_data_parallel(
    const CFD_BVH8Tree *tree,
    const CFD_UnstructuredGrid *mesh,
    const f32 *node_data,
    u8 dim,
    const V3 *points,
    u64 point_count,
    f32 eps,
    f32 *out_values
) {
    CFD_CHECK_NULL(tree, false);
    CFD_CHECK_NULL(mesh, false);
    CFD_CHECK_NULL(node_data, false);
    CFD_CHECK_NULL(points, false);
    CFD_CHECK_NULL(out_values, false);

    if (unlikely(dim == 0)) {
        cfd_error("cfd_sample_points_tetra_node_data_parallel(): dim must be > 0.");
        return false;
    }

    u64 i;
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 64)
#endif
    for (i = 0; i < point_count; ++i) {
        f32 *dst = out_values + i * dim;
        cfd_sample_point_tetra_node_data(tree, mesh, node_data, dim, points[i], eps, dst);
    }

    return true;
}

CFD_LIB b32 cfd_generate_xy_plane_mesh(
    V3 aabb_min,
    V3 aabb_max,
    f32 z,
    u32 res_x, u32 res_y,
    V3 *out_vertices,
    u32 *out_conn
) {
    CFD_CHECK_NULL(out_vertices, false);
    CFD_CHECK_NULL(out_conn, false);

    if (unlikely(res_x < 2 || res_y < 2)) {
        cfd_error("cfd_generate_xy_plane_mesh(): res_x and res_y must be >= 2.");
        return false;
    }

    f32 min_x = aabb_min.x;
    f32 min_y = aabb_min.y;
    f32 max_x = aabb_max.x;
    f32 max_y = aabb_max.y;

    f32 dx = (max_x - min_x) / (f32)(res_x - 1);
    f32 dy = (max_y - min_y) / (f32)(res_y - 1);

    // Vertex generation
    for (u32 y = 0; y < res_y; ++y) {
        f32 py = min_y + (f32)y * dy;

        for (u32 x = 0; x < res_x; ++x) {
            f32 px = min_x + (f32)x * dx;
            u64 v_idx = (u64)y * (u64)res_x + (u64)x;

            out_vertices[v_idx].x = px;
            out_vertices[v_idx].y = py;
            out_vertices[v_idx].z = z;
        }
    }

    // Triangle connectivity
    u64 conn_idx = 0;
    for (u32 y = 0; y < res_y - 1; ++y) {
        for (u32 x = 0; x < res_x - 1; ++x) {
            u32 v00 = y * res_x + x;
            u32 v10 = y * res_x + (x + 1);
            u32 v01 = (y + 1) * res_x + x;
            u32 v11 = (y + 1) * res_x + (x + 1);

            // Triangle 1
            out_conn[conn_idx++] = v00;
            out_conn[conn_idx++] = v10;
            out_conn[conn_idx++] = v11;

            // Triangle 2
            out_conn[conn_idx++] = v00;
            out_conn[conn_idx++] = v11;
            out_conn[conn_idx++] = v01;
        }
    }

    return true;
}

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_SLICER_H */
