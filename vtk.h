#ifndef CFD_VTK_H
#define CFD_VTK_H

#include "cfd_core.h"

typedef enum {
    VTK_UNKNOWN = 0,
    VTK_VERTEX,
    VTK_POLY_VERTEX,
    VTK_LINE,
    VTK_POLY_LINE,
    VTK_TRIANGLE,
    VTK_TRIANGLE_STRIP,
    VTK_POLYGON,
    VTK_PIXEL,
    VTK_QUAD,
    VTK_TETRA,
    VTK_VOXEL,
    VTK_HEXAHEDRON,
    VTK_WEDGE,
    VTK_PYRAMID,
    VTK_PENTAGONAL_PRISM,
    VTK_HEXAGONAL_PRISM,
    VTK_CELL_TYPE_COUNT,
    // ...
} VTK_Cell_Type;

CFD_INTERNAL VTK_Cell_Type cfd_type_to_vtk_type[] = {
    /* [CFD_CELL_POINT]      =*/ VTK_VERTEX,
    /* [CFD_CELL_BAR2]       =*/ VTK_LINE,
    /* [CFD_CELL_BAR3]       =*/ VTK_POLY_LINE,
    /* [CFD_CELL_TRIA3]      =*/ VTK_TRIANGLE,
    /* [CFD_CELL_TRIA6]      =*/ VTK_TRIANGLE_STRIP,
    /* [CFD_CELL_QUAD4]      =*/ VTK_QUAD,
    /* [CFD_CELL_QUAD8]      =*/ VTK_POLYGON,
    /* [CFD_CELL_TETRA4]     =*/ VTK_TETRA,
    /* [CFD_CELL_TETRA10]    =*/ VTK_UNKNOWN,
    /* [CFD_CELL_PYRAMID5]   =*/ VTK_PYRAMID,
    /* [CFD_CELL_PYRAMID13]  =*/ VTK_UNKNOWN,
    /* [CFD_CELL_PENTA6]     =*/ VTK_WEDGE,
    /* [CFD_CELL_PENTA15]    =*/ VTK_UNKNOWN,
    /* [CFD_CELL_HEXA8]      =*/ VTK_HEXAHEDRON,
    /* [CFD_CELL_HEXA20]     =*/ VTK_UNKNOWN,

    /* [CFD_CELL_NSIDED]     =*/ VTK_UNKNOWN,
    /* [CFD_CELL_NFACED]     =*/ VTK_UNKNOWN,
    /* [CFD_CELL_TYPE_COUNT] =*/ VTK_UNKNOWN,
    /* [CFD_CELL_UNKNOWN]    =*/ VTK_UNKNOWN,
};

// This function is only for debugging purposes!
CFD_LIB b32 cfd_unstructured_grid_to_vtk_file(CFD_UnstructuredGrid *mesh, const char *export_filename);

#ifdef CFD_LIB_IMPLEMENTATION
#include <stdio.h>
#include <string.h>

// This function is only for debugging purposes!
CFD_LIB b32 cfd_unstructured_grid_to_vtk_file(CFD_UnstructuredGrid *mesh, const char *export_filename) {
    FILE *f = fopen(export_filename, "wb");
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

    const u64 num_vertices = mesh->num_vertices;

    fprintf(f, "# vtk DataFile Version 3.0\n");
    fprintf(f, "Adesz's VTK exporter\n");
    fprintf(f, "ASCII\n");
    fprintf(f, "DATASET UNSTRUCTURED_GRID\n");
    fprintf(f, "POINTS %zu float\n", num_vertices);

    for (u64 i = 0; i < num_vertices; ++i) {
        V3 *v = &mesh->vertices[i];
        fprintf(f, "%f %f %f\n", (double)v->x, (double)v->y, (double)v->z);
    }

    u64 num_cells = 0, num_data = 0;
    for (u32 i = 0; i < mesh->num_cell_types; ++i) {
        const CFD_Cell_Group *restrict cell_group = &mesh->cell_groups[i];
        num_cells += cell_group->num_cells;
        num_data += cell_group->num_cells * ((u64)cfd_get_cell_node_count(cell_group->type) + 1);
    }

    fprintf(f, "CELLS %zu %zu\n", num_cells, num_data);
    for (u64 i = 0; i < mesh->num_cell_types; ++i) {
        const CFD_Cell_Group *restrict cell_group = &mesh->cell_groups[i];
        u8 num_nodes = cfd_get_cell_node_count(cell_group->type);
        u64 *conn_idx = cell_group->connectivity;
        for (u64 cell_idx = 0; cell_idx < cell_group->num_cells; ++cell_idx) {
            fprintf(f, "%d", num_nodes);
            for (u8 node_idx = 0; node_idx < num_nodes; ++node_idx)
                fprintf(f, " %zu", *(conn_idx++));
            fprintf(f, "\n");
        }
    }

    fprintf(f, "CELL_TYPES %zu\n", num_cells);
    for (u64 i = 0; i < mesh->num_cell_types; ++i) {
        const CFD_Cell_Group *restrict cell_group = &mesh->cell_groups[i];
        u32 cell_type = (u32)cfd_type_to_vtk_type[cell_group->type];
        for (u64 j = 0; j < cell_group->num_cells; ++j)
            fprintf(f, "%u\n", cell_type);
    }


    fclose(f);

    return true;
}

#endif /* CFD_LIB_IMPLEMENTATION */
#endif /* CFD_VTK_H */
