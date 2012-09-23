/*
MD3 and/or BSP to ASE converter
Written by Leszek Godlewski <github@inequation.org>
Code in the public domain
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];
typedef unsigned char byte;
#include "qfiles.h"

#ifndef BYTE_ORDER
	#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
		#define LITTLE_ENDIAN	1
		#define BIG_ENDIAN		2
	#endif

	// if not provided by system headers, you should #define byte order here
	#define BYTE_ORDER			LITTLE_ENDIAN
#endif // BYTE ORDER

#if BYTE_ORDER == BIG_ENDIAN
	#define little_short(x)		((((x) << 8) & 0xFF00) | (((x) >> 8) & 0x00FF))
	#define little_long(x)		((((x) << 24) & 0xFF000000)			\
									| (((x) << 8) & 0x00FF0000)		\
									| (((x) >> 8) & 0x0000FF00)		\
									| (((x) >> 24) & 0x000000FF))
#else
	#define little_short
	#define little_long
#endif // BIG_ENDIAN

void print_ase_header(FILE *out, const char *in_name) {
	fprintf(out,
		"*3DSMAX_ASCIIEXPORT 200\n"
		"*COMMENT \"MD3 and/or BSP to ASE converter [%s]\"\n"
		"*SCENE {\n"
			"\t*SCENE_FILENAME \"%s\"\n"
			"\t*SCENE_FIRSTFRAME 0\n"
			"\t*SCENE_LASTFRAME 0\n"
			"\t*SCENE_FRAMESPEED 30\n"
			"\t*SCENE_TICKSPERFRAME 160\n"
			"\t*SCENE_BACKGROUND_STATIC 0.0000 0.0000 0.0000\n"
			"\t*SCENE_AMBIENT_STATIC 0.0000 0.0000 0.0000\n"
		"}\n\n",
		__DATE__, in_name);
}

int convert_md3_to_ase(const char *in_name, FILE *in, FILE *out, int frame) {
	md3Header_t *md3;
	md3Shader_t *shader;
	md3Surface_t *surf;
	md3XyzNormal_t *vert;
	md3Triangle_t *tri;
	md3St_t *st;
	int i, j;
	unsigned char *buf;

	// load the file contents into a buffer
	fseek(in, 0, SEEK_END);
	i = ftell(in);
	fseek(in, 0, SEEK_SET);
	buf = malloc(i);
	if (!buf) {
		printf("Memory allocation failed\n");
		return 11;
	}
	if (fread(buf, 1, i, in) != (size_t)i) {
		printf("Failed to read file (%d bytes) into buffer\n", i);
		return 12;
	}

	// MD3 sanity checking
	md3 = (md3Header_t *)buf;

	if (little_long(md3->ident != MD3_IDENT)) {
		printf("Not a valid MD3 file\n");
		return 6;
	}

	if (little_long(md3->version > MD3_VERSION)) {
		printf("Unsupported MD3 version\n");
		return 7;
	}

	if (little_long(md3->numFrames) < 1) {
		printf("MD3 has no frames\n");
		return 8;
	}

	if (little_long(md3->numFrames) <= frame) {
		printf("Cannot extract frame #%d from a model that has %d frames\n",
			frame, little_long(md3->numFrames));
		return 9;
	}

	if (little_long(md3->numSurfaces) < 1) {
		printf("MD3 has no surfaces\n");
		return 10;
	}

	printf("MD3 stats:\n"
		"%d surfaces\n"
		"%d tags\n"
		"%d frames\n",
		little_long(md3->numSurfaces), little_long(md3->numTags),
		little_long(md3->numFrames));

	// begin ASE data
	print_ase_header(out, in_name);

	// material placeholders - iterate over all the MD3 surfaces
	fprintf(out,
		"\n"
		"*MATERIAL_LIST {\n"
			"\t*MATERIAL_COUNT %d\n",
		little_long(md3->numSurfaces));
	for (i = 0, surf = (md3Surface_t *)(buf + little_long(md3->ofsSurfaces));
		i < little_long(md3->numSurfaces);
		++i, surf += little_long(surf->ofsEnd)) {
		shader = (md3Shader_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsShaders));
		// begin ASE material
		fprintf(out,
			"\t*MATERIAL %d {\n"
				"\t\t*MATERIAL_NAME \"Material #%d\"\n"
				"\t\t*MATERIAL_CLASS \"Standard\"\n"
				"\t\t*MATERIAL_AMBIENT  0.1791 0.0654 0.0654\n"
				"\t\t*MATERIAL_DIFFUSE  0.5373 0.1961 0.1961\n"
				"\t\t*MATERIAL_SPECULAR 0.9000 0.9000 0.9000\n"
				"\t\t*MATERIAL_SHINE 0.2500\n"
				"\t\t*MATERIAL_SHINESTRENGTH 0.0500\n"
				"\t\t*MATERIAL_TRANSPARENCY 0.0000\n"
				"\t\t*MATERIAL_WIRESIZE 1.0000\n"
				"\t\t*MATERIAL_SHADING Blinn\n"
				"\t\t*MATERIAL_XP_FALLOFF 0.0000\n"
				"\t\t*MATERIAL_SELFILLUM 0.0000\n"
				"\t\t*MATERIAL_FALLOFF In\n"
				"\t\t*MATERIAL_XP_TYPE Filter\n"
				"\t\t*MAP_DIFFUSE {\n"
					"\t\t\t*MAP_NAME \"Map #%d\"\n"
					"\t\t\t*MAP_CLASS \"Bitmap\"\n"
					"\t\t\t*MAP_SUBNO 1\n"
					"\t\t\t*MAP_AMOUNT 1.0000\n"
					"\t\t\t*BITMAP \"%s\"\n"
					"\t\t\t*MAP_TYPE Screen\n"
					"\t\t\t*UVW_U_OFFSET 0.0000\n"
					"\t\t\t*UVW_V_OFFSET 0.0000\n"
					"\t\t\t*UVW_U_TILING 1.0000\n"
					"\t\t\t*UVW_V_TILING 1.0000\n"
					"\t\t\t*UVW_ANGLE 0.0000\n"
					"\t\t\t*UVW_BLUR 1.0000\n"
					"\t\t\t*UVW_BLUR_OFFSET 0.0000\n"
					"\t\t\t*UVW_NOUSE_AMT 1.0000\n"
					"\t\t\t*UVW_NOISE_SIZE 1.0000\n"
					"\t\t\t*UVW_NOISE_LEVEL 1\n"
					"\t\t\t*UVW_NOISE_PHASE 0.0000\n"
					"\t\t\t*BITMAP_FILTER Pyramidal\n"
				"\t\t}\n"
			"\t}\n",
			i, i + 1, i + 1, shader->name);
	}
	fprintf(out,
		"}\n");

	// geometry - iterate over all the MD3 surfaces
	for (i = 0, surf = (md3Surface_t *)(buf + little_long(md3->ofsSurfaces));
		i < little_long(md3->numSurfaces);
		++i, surf += little_long(surf->ofsEnd)) {

		printf("Processing surface #%d, \"%s\": %d vertices, %d triangles\n",
			i, surf->name, little_long(surf->numVerts),
			little_long(surf->numTriangles));

		// begin ASE geom object
		fprintf(out,
			"*GEOMOBJECT {\n"
				"\t*NODE_NAME \"%s\"\n"
			"\n"
				"\t*NODE_TM {\n"
					"\t\t*NODE_NAME \"%s\"\n"
					"\t\t*INHERIT_POS 0 0 0\n"
					"\t\t*INHERIT_ROT 0 0 0\n"
					"\t\t*INHERIT_SCL 0 0 0\n"
					"\t\t*TM_ROW0 1.0000 0.0000 0.0000\n"
					"\t\t*TM_ROW1 0.0000 1.0000 0.0000\n"
					"\t\t*TM_ROW2 0.0000 0.0000 1.0000\n"
					"\t\t*TM_ROW3 0.0000 0.0000 0.0000\n"
					"\t\t*TM_POS 0.0000 0.0000 0.0000\n"
					"\t\t*TM_ROTAXIS 0.0000 0.0000 0.0000\n"
					"\t\t*TM_ROTANGLE 0.0000\n"
					"\t\t*TM_SCALE 1.0000 1.0000 1.0000\n"
					"\t\t*TM_SCALEAXIS 0.0000 0.0000 0.0000\n"
					"\t\t*TM_SCALEAXISANG 0.0000\n"
				"\t}\n"
			"\n"
				"\t*MESH {\n"
					"\t\t*TIMEVALUE 0\n"
					"\t\t*MESH_NUMVERTEX %d\n"
					"\t\t*MESH_NUMFACES %d\n",
			surf->name, surf->name, little_long(surf->numVerts),
				little_long(surf->numTriangles));

		// output the vertex list
		fprintf(out,
					"\t\t*MESH_VERTEX_LIST {\n");
		vert = (md3XyzNormal_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsXyzNormals)
			+ frame * little_long(surf->numVerts));
		for (j = 0; j < little_long(surf->numVerts); ++j, ++vert) {
			fprintf(out,
						"\t\t\t*MESH_VERTEX %d %f %f %f\n",
				j, (float)vert->xyz[0] * MD3_XYZ_SCALE,
					(float)vert->xyz[1] * MD3_XYZ_SCALE,
					(float)vert->xyz[2] * MD3_XYZ_SCALE);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// output the triangle list
		fprintf(out,
					"\t\t*MESH_FACE_LIST {\n");
		tri = (md3Triangle_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsTriangles));
		for (j = 0; j < little_long(surf->numTriangles); ++j, ++tri) {
			fprintf(out,
						"\t\t\t*MESH_FACE %d: A: %d B: %d C: %d "
							"AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING %d *MESH_MTLID %d\n",
				j, little_long(tri->indexes[2]), little_long(tri->indexes[1]),
					little_long(tri->indexes[0]), i, i);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// output the texture vertex list
		fprintf(out,
					"\t\t*MESH_TVERTLIST {\n");
		st = (md3St_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsSt));
		for (j = 0; j < little_long(surf->numVerts); ++j, ++st) {
			fprintf(out,
						"\t\t\t*MESH_TVERT %d %f %f 0.0000\n",
				j, st->st[0], 1.f - st->st[1]);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// output the texture triangle list (same as the original triangles)
		fprintf(out,
					"\t\t*MESH_TFACELIST {\n");
		tri = (md3Triangle_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsTriangles));
		for (j = 0; j < little_long(surf->numTriangles); ++j, ++tri) {
			fprintf(out,
						"\t\t\t*MESH_TFACE %d %d %d %d\n",
				j, tri->indexes[2], tri->indexes[1], tri->indexes[0]);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// output the normals
		fprintf(out,
					"\t\t*MESH_NORMALS {\n");
		vert = (md3XyzNormal_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsXyzNormals)
			+ frame * little_long(surf->numVerts));
		for (j = 0; j < little_long(surf->numVerts); ++j, ++vert) {
			float lat, lng;
			lat = (vert->normal >> 8)	/ 255.f * M_PI * 2.f;
			lng = (vert->normal & 0xFF)	/ 255.f * M_PI * 2.f;
			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )
			fprintf(out,
						"\t\t\t*MESH_VERTEXNORMAL %d %f %f %f\n",
				j, cos(lat) * sin(lng), sin(lat) * sin(lng), cos(lng));
		}
		fprintf(out,
					"\t\t}\n");

		// close ASE geom object
		fprintf(out,
				"\t}\n"
			"\n"
				"\t*PROP_MOTIONBLUR 0\n"
				"\t*PROP_CASTSHADOW 1\n"
				"\t*PROP_RECVSHADOW 1\n"
				"\t*MATERIAL_REF %d\n"
			"}\n",
			i);
	}

	return 0;
}

const char *get_bsp_surface_type(mapSurfaceType_t t) {
	switch(t) {
		case MST_BAD:			return "MST_BAD";
		case MST_PLANAR:		return "MST_PLANAR";
		case MST_PATCH:			return "MST_PATCH";
		case MST_TRIANGLE_SOUP:	return "MST_TRIANGLE_SOUP";
		case MST_FLARE:			return "MST_FLARE";
		case MST_FOLIAGE:		return "MST_FOLIAGE";
		default:				return "(unknown)";
	}
}

int convert_bsp_to_ase(const char *in_name, FILE *in, char *out_name) {
	dheader_t *bsp;
	dmodel_t *model;
	dsurface_t *surf;
	dshader_t *shader;
	drawVert_t *vert;
	int *tri;
	int i, j, k, count;
	unsigned char *buf;
	char *out_name_buf, *p;
	size_t out_name_buf_len;
	char format_buf[20];
	FILE *out;
	const int skip_planar = 1;

	// load the file contents into a buffer
	fseek(in, 0, SEEK_END);
	i = ftell(in);
	fseek(in, 0, SEEK_SET);
	buf = malloc(i);
	if (!buf) {
		printf("Memory allocation failed\n");
		return 11;
	}
	if (fread(buf, 1, i, in) != (size_t)i) {
		printf("Failed to read file (%d bytes) into buffer\n", i);
		return 12;
	}

	// allocate a string buffer large enough to hold the filename extended by
	// the maximum model index
	// max length of model index
	i = 1 + (int)floorf(log10f(MAX_MAP_MODELS));
	// max length of surface index
	j = 1 + (int)floorf(log10f(10240));
	out_name_buf_len = strlen(out_name) + 1 + i + 1 + j + 4 + 1;
	out_name_buf = malloc(out_name_buf_len);
	snprintf(format_buf, sizeof(format_buf), "%%s_%%0%dd_%%0%dd.ase%c", i, j,
		0);
	// find and cut the extension off
	if ((p = strrchr(out_name, '.')) != NULL)
		*p = 0;

	// BSP sanity checking
	bsp = (dheader_t *)buf;

	if (little_long(bsp->ident != BSP_IDENT)) {
		printf("Not a valid BSP file\n");
		return 13;
	}

	if (little_long(bsp->version != BSP_VERSION)) {
		printf("Unsupported BSP version\n");
		return 14;
	}

	// iterate over all the models
	for (i = 0, model = (dmodel_t *)(buf
			+ little_long(bsp->lumps[LUMP_MODELS].fileofs));
		i < little_long(bsp->lumps[LUMP_MODELS].filelen)
			/ (int)sizeof(dmodel_t);
		++i, ++model) {

		if (little_long(model->numSurfaces) < 1) {
			//printf("\tNo surfaces in model %d, skipping\n", i);
			continue;
		}

		// count exportable surfaces
		for (j = 0, count = 0, surf = (dsurface_t *)(buf
				+ little_long(bsp->lumps[LUMP_SURFACES].fileofs)
				+ little_long(model->firstSurface) * sizeof(dsurface_t));
			j < little_long(model->numSurfaces);
			++j, ++surf) {

			if (skip_planar && little_long(surf->surfaceType) == MST_PLANAR)
				continue;
			else if (little_long(surf->surfaceType) != MST_TRIANGLE_SOUP) {
				printf("WARNING: cannot handle %s surfaces yet, skipping\n",
					get_bsp_surface_type(little_long(surf->surfaceType)));
				continue;
			}
			++count;
		}

		// apparently there's nothing to export, get rid of the file
		if (count == 0) {
			fclose(out);
			//printf("\tNo exportable surfaces, skipping\n");
			continue;
		}

		printf("Processing model #%d: %d exportable surfaces\n", i, count);

		// iterate over all the BSP drawable surfaces
		for (j = 0, surf = (dsurface_t *)(buf
				+ little_long(bsp->lumps[LUMP_SURFACES].fileofs)
				+ little_long(model->firstSurface) * sizeof(dsurface_t));
			j < little_long(model->numSurfaces);
			++j, ++surf) {

			if (skip_planar && little_long(surf->surfaceType) == MST_PLANAR)
				continue;
			else if (little_long(surf->surfaceType) != MST_TRIANGLE_SOUP)
				// I can only handle triangle soups so far
				continue;

			// start the output
			snprintf(out_name_buf, out_name_buf_len, format_buf, out_name, i, j);
			out = fopen(out_name_buf, "w");

			// begin ASE data
			print_ase_header(out, in_name);

			printf("\tProcessing surface #%d: type %s, %d vertices, "
				"%d indices\n",
				j, get_bsp_surface_type(little_long(surf->surfaceType)),
				little_long(surf->numVerts), little_long(surf->numIndexes));

			shader = (dshader_t *)(buf
				+ little_long(bsp->lumps[LUMP_SHADERS].fileofs)
				+ little_long(surf->shaderNum) * sizeof(dshader_t));

			// write out the material placeholder
			fprintf(out,
				"\n"
				"*MATERIAL_LIST {\n"
					"\t*MATERIAL_COUNT 1\n"
					"\t*MATERIAL 0 {\n"
						"\t\t*MATERIAL_NAME \"Material #1\"\n"
						"\t\t*MATERIAL_CLASS \"Standard\"\n"
						"\t\t*MATERIAL_AMBIENT  0.1791 0.0654 0.0654\n"
						"\t\t*MATERIAL_DIFFUSE  0.5373 0.1961 0.1961\n"
						"\t\t*MATERIAL_SPECULAR 0.9000 0.9000 0.9000\n"
						"\t\t*MATERIAL_SHINE 0.2500\n"
						"\t\t*MATERIAL_SHINESTRENGTH 0.0500\n"
						"\t\t*MATERIAL_TRANSPARENCY 0.0000\n"
						"\t\t*MATERIAL_WIRESIZE 1.0000\n"
						"\t\t*MATERIAL_SHADING Blinn\n"
						"\t\t*MATERIAL_XP_FALLOFF 0.0000\n"
						"\t\t*MATERIAL_SELFILLUM 0.0000\n"
						"\t\t*MATERIAL_FALLOFF In\n"
						"\t\t*MATERIAL_XP_TYPE Filter\n"
						"\t\t*MAP_DIFFUSE {\n"
							"\t\t\t*MAP_NAME \"Map #1\"\n"
							"\t\t\t*MAP_CLASS \"Bitmap\"\n"
							"\t\t\t*MAP_SUBNO 1\n"
							"\t\t\t*MAP_AMOUNT 1.0000\n"
							"\t\t\t*BITMAP \"%s\"\n"
							"\t\t\t*MAP_TYPE Screen\n"
							"\t\t\t*UVW_U_OFFSET 0.0000\n"
							"\t\t\t*UVW_V_OFFSET 0.0000\n"
							"\t\t\t*UVW_U_TILING 1.0000\n"
							"\t\t\t*UVW_V_TILING 1.0000\n"
							"\t\t\t*UVW_ANGLE 0.0000\n"
							"\t\t\t*UVW_BLUR 1.0000\n"
							"\t\t\t*UVW_BLUR_OFFSET 0.0000\n"
							"\t\t\t*UVW_NOUSE_AMT 1.0000\n"
							"\t\t\t*UVW_NOISE_SIZE 1.0000\n"
							"\t\t\t*UVW_NOISE_LEVEL 1\n"
							"\t\t\t*UVW_NOISE_PHASE 0.0000\n"
							"\t\t\t*BITMAP_FILTER Pyramidal\n"
						"\t\t}\n"
					"\t}\n"
				"}\n",
				shader->shader);

			// begin ASE geom object
			fprintf(out,
				"*GEOMOBJECT {\n"
					"\t*NODE_NAME \"surface %d\"\n"
				"\n"
					"\t*NODE_TM {\n"
						"\t\t*NODE_NAME \"surface %d\"\n"
						"\t\t*INHERIT_POS 0 0 0\n"
						"\t\t*INHERIT_ROT 0 0 0\n"
						"\t\t*INHERIT_SCL 0 0 0\n"
						"\t\t*TM_ROW0 1.0000 0.0000 0.0000\n"
						"\t\t*TM_ROW1 0.0000 1.0000 0.0000\n"
						"\t\t*TM_ROW2 0.0000 0.0000 1.0000\n"
						"\t\t*TM_ROW3 0.0000 0.0000 0.0000\n"
						"\t\t*TM_POS 0.0000 0.0000 0.0000\n"
						"\t\t*TM_ROTAXIS 0.0000 0.0000 0.0000\n"
						"\t\t*TM_ROTANGLE 0.0000\n"
						"\t\t*TM_SCALE 1.0000 1.0000 1.0000\n"
						"\t\t*TM_SCALEAXIS 0.0000 0.0000 0.0000\n"
						"\t\t*TM_SCALEAXISANG 0.0000\n"
					"\t}\n"
				"\n"
					"\t*MESH {\n"
						"\t\t*TIMEVALUE 0\n"
						"\t\t*MESH_NUMVERTEX %d\n"
						"\t\t*MESH_NUMFACES %d\n",
				j, j, little_long(surf->numVerts),
					little_long(surf->numIndexes) / 3);

			// output the vertex list
			fprintf(out,
						"\t\t*MESH_VERTEX_LIST {\n");
			vert = (drawVert_t *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWVERTS].fileofs)
				+ little_long(surf->firstVert) * sizeof(drawVert_t));
			for (k = 0; k < little_long(surf->numVerts); ++k, ++vert) {
				fprintf(out,
							"\t\t\t*MESH_VERTEX %d %f %f %f\n",
					k, (float)vert->xyz[0],
						(float)vert->xyz[1],
						(float)vert->xyz[2]);
			}
			fprintf(out,
						"\t\t}\n"
				"\n");

			// output the triangle list
			fprintf(out,
						"\t\t*MESH_FACE_LIST {\n");
			tri = (int *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWINDEXES].fileofs)
				+ little_long(surf->firstIndex) * sizeof(int));
			for (k = 0; k < little_long(surf->numIndexes) / 3; ++k, tri += 3) {
				fprintf(out,
							"\t\t\t*MESH_FACE %d: A: %d B: %d C: %d "
								"AB: 1 BC: 1 CA: 1 *MESH_SMOOTHING %d *MESH_MTLID %d\n",
					k, little_long(tri[2]),
						little_long(tri[1]),
						little_long(tri[0]), j, j);
			}
			fprintf(out,
						"\t\t}\n"
				"\n");

			// output the texture vertex list
			fprintf(out,
						"\t\t*MESH_TVERTLIST {\n");
			vert = (drawVert_t *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWVERTS].fileofs)
				+ little_long(surf->firstVert) * sizeof(drawVert_t));
			for (k = 0; k < little_long(surf->numVerts); ++k, ++vert) {
				fprintf(out,
							"\t\t\t*MESH_TVERT %d %f %f 0.0000\n",
					k, vert->st[0], 1.f - vert->st[1]);
			}
			fprintf(out,
						"\t\t}\n"
				"\n");

			// output the texture triangle list (same as the original triangles)
			fprintf(out,
						"\t\t*MESH_TFACELIST {\n");
			tri = (int *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWINDEXES].fileofs)
				+ little_long(surf->firstIndex) * sizeof(int));
			for (k = 0; k < little_long(surf->numIndexes) / 3; ++k, tri += 3) {
				fprintf(out,
							"\t\t\t*MESH_TFACE %d %d %d %d\n",
					k, little_long(tri[2]),
						little_long(tri[1]),
						little_long(tri[0]));
			}
			fprintf(out,
						"\t\t}\n"
				"\n");

			// output the normals
			fprintf(out,
						"\t\t*MESH_NORMALS {\n");
			vert = (drawVert_t *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWVERTS].fileofs)
				+ little_long(surf->firstVert) * sizeof(drawVert_t));
			for (k = 0; k < little_long(surf->numVerts); ++k, ++vert) {
				fprintf(out,
							"\t\t\t*MESH_VERTEXNORMAL %d %f %f %f\n",
					k, vert->normal[0], vert->normal[1], vert->normal[2]);
			}
			fprintf(out,
						"\t\t}\n");

			// close ASE geom object
			fprintf(out,
					"\t}\n"
				"\n"
					"\t*PROP_MOTIONBLUR 0\n"
					"\t*PROP_CASTSHADOW 1\n"
					"\t*PROP_RECVSHADOW 1\n"
					"\t*MATERIAL_REF 0\n"
				"}\n");

			fclose(out);
		}
	}

	free(out_name_buf);

	return 0;
}

int convert_md3_to_obj(const char *in_name, FILE *in, FILE *out, int frame) {
	md3Header_t *md3;
	md3Surface_t *surf;
	md3XyzNormal_t *vert;
	md3Triangle_t *tri;
	md3St_t *st;
	int i, j;
	unsigned char *buf;

	// load the file contents into a buffer
	fseek(in, 0, SEEK_END);
	i = ftell(in);
	fseek(in, 0, SEEK_SET);
	buf = malloc(i);
	if (!buf) {
		printf("Memory allocation failed\n");
		return 11;
	}
	if (fread(buf, 1, i, in) != (size_t)i) {
		printf("Failed to read file (%d bytes) into buffer\n", i);
		return 12;
	}

	// MD3 sanity checking
	md3 = (md3Header_t *)buf;

	if (little_long(md3->ident != MD3_IDENT)) {
		printf("Not a valid MD3 file\n");
		return 6;
	}

	if (little_long(md3->version > MD3_VERSION)) {
		printf("Unsupported MD3 version\n");
		return 7;
	}

	if (little_long(md3->numFrames) < 1) {
		printf("MD3 has no frames\n");
		return 8;
	}

	if (little_long(md3->numFrames) <= frame) {
		printf("Cannot extract frame #%d from a model that has %d frames\n",
			frame, little_long(md3->numFrames));
		return 9;
	}

	if (little_long(md3->numSurfaces) < 1) {
		printf("MD3 has no surfaces\n");
		return 10;
	}

	printf("MD3 stats:\n"
		"%d surfaces\n"
		"%d tags\n"
		"%d frames\n",
		little_long(md3->numSurfaces), little_long(md3->numTags),
		little_long(md3->numFrames));

	// begin OBJ data
	fprintf(out,
		"# generated by md3bsp2ase from %s\n", in_name);

	// geometry - iterate over all the MD3 surfaces
	for (i = 0, surf = (md3Surface_t *)(buf + little_long(md3->ofsSurfaces));
		i < little_long(md3->numSurfaces);
		++i, surf += little_long(surf->ofsEnd)) {

		printf("Processing surface #%d, \"%s\": %d vertices, %d triangles\n",
			i, surf->name, little_long(surf->numVerts),
			little_long(surf->numTriangles));

		// start a group
		fprintf(out,
			"\n"
			"# surface #%d\n"
			"g %s\n"
			"o %s\n"
			"\n",
				i, surf->name, surf->name);

		// output the vertex list
		vert = (md3XyzNormal_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsXyzNormals)
			+ frame * little_long(surf->numVerts));
		for (j = 0; j < little_long(surf->numVerts); ++j, ++vert) {
			fprintf(out,
				"v %f %f %f\n",
					(float)vert->xyz[0] * MD3_XYZ_SCALE,
					(float)vert->xyz[2] * MD3_XYZ_SCALE,
					(float)vert->xyz[1] * MD3_XYZ_SCALE);
		}

		fprintf(out, "\n");

		// output the texture vertex list
		st = (md3St_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsSt));
		for (j = 0; j < little_long(surf->numVerts); ++j, ++st)
			fprintf(out,
				"vt %f %f\n", st->st[0], 1.f - st->st[1]);

		fprintf(out, "\n");

		// output the normals
		vert = (md3XyzNormal_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsXyzNormals)
			+ frame * little_long(surf->numVerts));
		for (j = 0; j < little_long(surf->numVerts); ++j, ++vert) {
			float lat, lng;
			lat = (vert->normal >> 8)	/ 255.f * M_PI * 2.f;
			lng = (vert->normal & 0xFF)	/ 255.f * M_PI * 2.f;
			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )
			// swap Y with Z for Blender
			fprintf(out,
				"vn %f %f %f\n",
					cos(lat) * sin(lng), cos(lng), sin(lat) * sin(lng));
		}

		fprintf(out,
			"\n"
			"s 1\n");

		// output the triangle list
		tri = (md3Triangle_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsTriangles));
		for (j = 0; j < little_long(surf->numTriangles); ++j, ++tri) {
			fprintf(out,
				"f %d/%d/%d %d/%d/%d %d/%d/%d\n",
					1 + little_long(tri->indexes[0]),
					1 + little_long(tri->indexes[0]),
					1 + little_long(tri->indexes[0]),
					1 + little_long(tri->indexes[1]),
					1 + little_long(tri->indexes[1]),
					1 + little_long(tri->indexes[1]),
					1 + little_long(tri->indexes[2]),
					1 + little_long(tri->indexes[2]),
					1 + little_long(tri->indexes[2]));
		}
	}

	return 0;
}

int main(int argc, char *argv[]) {
	FILE *infile;
	char *in_ext, *out_ext;
	int retcode;

	if (argc < 3) {
		printf("Usage: %s <infile> <outfile> [frame number]\n", argv[0]);
		return 1;
	}

	in_ext = strrchr(argv[1], '.');
	if (in_ext == NULL) {
		printf("File %s appears to have no extension\n", argv[1]);
		return 2;
	}
	++in_ext;

	out_ext = strrchr(argv[2], '.');
	if (out_ext != NULL)
		++out_ext;

	if (!(infile = fopen(argv[1], "rb"))) {
		printf("Failed to open file %s\n", argv[1]);
		return 3;
	}

	if (!strcasecmp(in_ext, "md3")) {
		FILE *outfile = fopen(argv[2], "w");
		if (!outfile) {
			printf("Failed to open file %s\n", argv[2]);
			fclose(infile);
			return 4;
		}
		if (!strcasecmp(out_ext, "obj"))
			retcode = convert_md3_to_obj(argv[1], infile, outfile,
				argc > 3 ? atoi(argv[3]) : 0);
		else
			retcode = convert_md3_to_ase(argv[1], infile, outfile,
				argc > 3 ? atoi(argv[3]) : 0);
		fclose(outfile);
	} else if (!strcasecmp(in_ext, "bsp"))
		retcode = convert_bsp_to_ase(argv[1], infile, argv[2]);
	else {
		printf("Unknown extension %s in file %s\n", in_ext, argv[1]);
		retcode = 5;
	}

	fclose(infile);

	return retcode;
}
