/*
MD3 and/or BSP to OBJ converter
Written by Leszek Godlewski <github@inequation.org>
Code in the public domain
*/

#ifdef _MSC_VER
	#define _CRT_SECURE_NO_WARNINGS
	#define strcasecmp	_stricmp
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _USE_MATH_DEFINES
	#define _USE_MATH_DEFINES
#endif
#include <math.h>

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];
typedef unsigned char byte;
#include "qfiles.h"
#include "surfaceflags.h"

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

const char *get_bsp_surface_type(mapSurfaceType_t t)
{
	switch(t)
	{
		case MST_BAD:			return "MST_BAD";
		case MST_PLANAR:		return "MST_PLANAR";
		case MST_PATCH:			return "MST_PATCH";
		case MST_TRIANGLE_SOUP:	return "MST_TRIANGLE_SOUP";
		case MST_FLARE:			return "MST_FLARE";
		case MST_FOLIAGE:		return "MST_FOLIAGE";
		default:				return "(unknown)";
	}
}

int convert_bsp_to_obj(const char *in_name, FILE *in, char *out_name)
{
	dheader_t *bsp;
	dmodel_t *model;
	dsurface_t *surf;
	dshader_t *shader;
	drawVert_t *vert;
	int *tri;
	int model_index, surf_index, surf_index_actual, vert_index, vert_index_cum;
	int count;
	unsigned char *buf;
	char *out_name_buf, *p;
	size_t out_name_buf_len;
	char format_buf[20];
	FILE *out;
	// TODO: Promote these to command-line switches.
	const int split_models = 0;
	const int skip_planar = 0;
	const int skip_tris = 0;
	const int skip_patches = 1;
	const int skip_collision = 1;

	// load the file contents into a buffer
	fseek(in, 0, SEEK_END);
	count = ftell(in);
	fseek(in, 0, SEEK_SET);
	buf = malloc(count);
	if (!buf)
	{
		printf("Memory allocation failed\n");
		return 11;
	}
	if (fread(buf, 1, count, in) != (size_t)count)
	{
		printf("Failed to read file (%d bytes) into buffer\n", count);
		return 12;
	}

	// allocate a string buffer large enough to hold the filename extended by
	// the maximum model index
	// max length of model index
	model_index = 1 + (int)floorf(log10f(MAX_MAP_MODELS));
	// max length of surface index
	surf_index = 1 + (int)floorf(log10f(10240));
	out_name_buf_len = strlen(out_name) + 1 + model_index + 1 + surf_index + 4 + 1;
	out_name_buf = malloc(out_name_buf_len);
	// MSVC is retarded and disallows just #defining snprintf.
#if _MSC_VER
	_snprintf
#else
	snprintf
#endif
		(format_buf, sizeof(format_buf),
			split_models ? "%%s_%%0%dd_%%0%dd.obj\x00" : "%%s_%%0%dd.obj\x00",
			model_index, surf_index);
	// find and cut the extension off
	if ((p = strrchr(out_name, '.')) != NULL)
		*p = 0;

	// BSP sanity checking
	bsp = (dheader_t *)buf;

	if (little_long(bsp->ident != BSP_IDENT))
	{
		printf("Not a valid BSP file\n");
		return 13;
	}

	if (little_long(bsp->version != BSP_VERSION))
	{
		printf("Unsupported BSP version\n");
		return 14;
	}

	// iterate over all the models
	for (model_index = 0, model = (dmodel_t *)(buf
		+ little_long(bsp->lumps[LUMP_MODELS].fileofs));
		model_index < little_long(bsp->lumps[LUMP_MODELS].filelen)
		/ (int)sizeof(dmodel_t);
		++model_index, ++model)
	{

		if (little_long(model->numSurfaces) < 1
			&& (skip_collision || little_long(model->numBrushes) < 1))
		{
			continue;
		}

		// count exportable surfaces
		for (surf_index = 0, count = 0, surf = (dsurface_t *)(buf
			+ little_long(bsp->lumps[LUMP_SURFACES].fileofs)
			+ little_long(model->firstSurface) * sizeof(dsurface_t));
			surf_index < little_long(model->numSurfaces);
			++surf_index, ++surf)
		{
			if ((skip_planar && little_long(surf->surfaceType) == MST_PLANAR)
				|| (skip_tris && little_long(surf->surfaceType) == MST_TRIANGLE_SOUP)
				|| (skip_patches && little_long(surf->surfaceType) == MST_PATCH))
			{
				continue;
			}
			else if (little_long(surf->surfaceType) != MST_PLANAR
				&& little_long(surf->surfaceType) != MST_TRIANGLE_SOUP
				&& little_long(surf->surfaceType) != MST_PATCH)
			{
				static char warned[1 << (sizeof(char) * 8)] = { 0 };
				if (surf->surfaceType > sizeof(warned) || !warned[surf->surfaceType])
				{
					warned[surf->surfaceType] = 1;
					printf("WARNING: cannot handle %s surfaces yet, skipping\n",
						get_bsp_surface_type(little_long(surf->surfaceType)));
				}
				continue;
			}
			++count;
		}

		// apparently there's nothing to export, get rid of the file
		if (count == 0)
		{
			fclose(out);
			//printf("\tNo exportable surfaces, skipping\n");
			continue;
		}

		printf("Processing model #%d: %d exportable surfaces\n", model_index, count);

		if (!split_models)
		{
			// Start the output.
			snprintf(out_name_buf, out_name_buf_len, format_buf, out_name, model_index);
			out = fopen(out_name_buf, "w");

			// Begin OBJ data.
			fprintf(out, "# generated by md3bsp2ase from %s model #%d\n", in_name, model_index);

			vert_index_cum = 0;
		}
		surf_index_actual = 0;

		// iterate over all the BSP drawable surfaces
		for (surf_index = 0, surf = (dsurface_t *)(buf
			+ little_long(bsp->lumps[LUMP_SURFACES].fileofs)
			+ little_long(model->firstSurface) * sizeof(dsurface_t));
			surf_index < little_long(model->numSurfaces);
			++surf_index, ++surf)
		{
			if ((skip_planar && little_long(surf->surfaceType) == MST_PLANAR)
				|| (skip_tris && little_long(surf->surfaceType) == MST_TRIANGLE_SOUP)
				|| (skip_patches && little_long(surf->surfaceType) == MST_PATCH))
			{
				continue;
			}
			else if (little_long(surf->surfaceType) != MST_PLANAR
				&& little_long(surf->surfaceType) != MST_TRIANGLE_SOUP
				&& little_long(surf->surfaceType) != MST_PATCH)
			{
				continue;
			}

			shader = (dshader_t *)(buf
				+ little_long(bsp->lumps[LUMP_SHADERS].fileofs)
				+ little_long(surf->shaderNum) * sizeof(dshader_t));

			// Skip non-drawable surfaces (i.e. collision).
			if (skip_collision && (little_long(shader->surfaceFlags) & SURF_NODRAW))
			{
				continue;
			}

			++surf_index_actual;

			if (split_models)
			{
				// Start the surface output.
				snprintf(out_name_buf, out_name_buf_len, format_buf, out_name, model_index, surf_index);
				out = fopen(out_name_buf, "w");

				// Begin OBJ data.
				fprintf(out, "# generated by md3bsp2ase from %s model #%d surface #%d\n", in_name, model_index, surf_index);

				vert_index_cum = 0;
			}

			printf("\tProcessing surface #%d: type %s, %d vertices, "
				"%d indices\n",
				surf_index, get_bsp_surface_type(little_long(surf->surfaceType)),
				little_long(surf->numVerts), little_long(surf->numIndexes));

			// start a group
			fprintf(out,
				"\n"
				"# surface %d/%d (#%d, %s)\n"
				"usemtl %s\n"
				"g surf%d\n"
				"o surf%d\n"
				"\n",
				surf_index_actual, count, surf_index, get_bsp_surface_type(little_long(surf->surfaceType)), shader->shader, surf_index, surf_index);

			// Tesselate patches.
			if (little_long(surf->surfaceType) == MST_PATCH)
			{
				// TODO
			}

			// output the vertex list
			vert = (drawVert_t *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWVERTS].fileofs)
				+ little_long(surf->firstVert) * sizeof(drawVert_t));
			for (vert_index = 0; vert_index < little_long(surf->numVerts); ++vert_index, ++vert)
			{
				fprintf(out,
					"v %f %f %f\n",
					(float)vert->xyz[0],
					(float)vert->xyz[1],
					(float)vert->xyz[2]);
			}

			fprintf(out, "\n");

			// output the texture vertex list
			vert = (drawVert_t *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWVERTS].fileofs)
				+ little_long(surf->firstVert) * sizeof(drawVert_t));
			for (vert_index = 0; vert_index < little_long(surf->numVerts); ++vert_index, ++vert)
			{
				fprintf(out, "vt %f %f\n", vert->st[0], 1.f - vert->st[1]);
			}

			fprintf(out, "\n");

			// output the normals
			vert = (drawVert_t *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWVERTS].fileofs)
				+ little_long(surf->firstVert) * sizeof(drawVert_t));
			for (vert_index = 0; vert_index < little_long(surf->numVerts); ++vert_index, ++vert)
			{
				fprintf(out,
					"vn %f %f %f\n",
					vert->normal[0], vert->normal[1], vert->normal[2]);
			}

			fprintf(out,
				"\n"
				"s 1\n");

			// output the triangle list
			tri = (int *)(buf
				+ little_long(bsp->lumps[LUMP_DRAWINDEXES].fileofs)
				+ little_long(surf->firstIndex) * sizeof(int));
			for (vert_index = 0; vert_index < little_long(surf->numIndexes) / 3; ++vert_index, tri += 3)
			{
				fprintf(out,
					"f %d/%d/%d %d/%d/%d %d/%d/%d\n",
					1 + little_long(tri[2]) + vert_index_cum,
					1 + little_long(tri[2]) + vert_index_cum,
					1 + little_long(tri[2]) + vert_index_cum,
					1 + little_long(tri[1]) + vert_index_cum,
					1 + little_long(tri[1]) + vert_index_cum,
					1 + little_long(tri[1]) + vert_index_cum,
					1 + little_long(tri[0]) + vert_index_cum,
					1 + little_long(tri[0]) + vert_index_cum,
					1 + little_long(tri[0]) + vert_index_cum);
			}

			// Keep track of cumulative vertex index so that multiple surfaces in the same file may coexist.
			vert_index_cum += little_long(surf->numVerts);
			
			if (split_models)
			{
				fclose(out);
			}
		}

		if (!split_models)
		{
			fclose(out);
		}
	}

	free(out_name_buf);

	return 0;
}

int convert_md3_to_obj(const char *in_name, FILE *in, FILE *out, int frame)
{
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
	if (!buf)
	{
		printf("Memory allocation failed\n");
		return 11;
	}
	if (fread(buf, 1, i, in) != (size_t)i)
	{
		printf("Failed to read file (%d bytes) into buffer\n", i);
		return 12;
	}

	// MD3 sanity checking
	md3 = (md3Header_t *)buf;

	if (little_long(md3->ident != MD3_IDENT))
	{
		printf("Not a valid MD3 file\n");
		return 6;
	}

	if (little_long(md3->version > MD3_VERSION))
	{
		printf("Unsupported MD3 version\n");
		return 7;
	}

	if (little_long(md3->numFrames) < 1)
	{
		printf("MD3 has no frames\n");
		return 8;
	}

	if (little_long(md3->numFrames) <= frame)
	{
		printf("Cannot extract frame #%d from a model that has %d frames\n",
			frame, little_long(md3->numFrames));
		return 9;
	}

	if (little_long(md3->numSurfaces) < 1)
	{
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
		++i, surf += little_long(surf->ofsEnd))
	{
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
		for (j = 0; j < little_long(surf->numVerts); ++j, ++vert)
		{
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
		{
			fprintf(out,
				"vt %f %f\n", st->st[0], 1.f - st->st[1]);
		}

		fprintf(out, "\n");

		// output the normals
		vert = (md3XyzNormal_t *)(((unsigned char *)surf)
			+ little_long(surf->ofsXyzNormals)
			+ frame * little_long(surf->numVerts));
		for (j = 0; j < little_long(surf->numVerts); ++j, ++vert)
		{
			float lat, lng;
			lat = (vert->normal >> 8)	/ 255.f * (float)M_PI * 2.f;
			lng = (vert->normal & 0xFF) / 255.f * (float)M_PI * 2.f;
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
		for (j = 0; j < little_long(surf->numTriangles); ++j, ++tri)
		{
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

int main(int argc, char *argv[])
{
	FILE *infile;
	char *in_ext, *out_ext;
	int retcode;

	if (argc < 3)
	{
		printf("Usage: %s <infile> <outfile> [frame number]\n", argv[0]);
		return 1;
	}

	in_ext = strrchr(argv[1], '.');
	if (in_ext == NULL)
	{
		printf("File %s appears to have no extension\n", argv[1]);
		return 2;
	}
	++in_ext;

	out_ext = strrchr(argv[2], '.');
	if (out_ext != NULL)
	{
		++out_ext;
	}

	if (!(infile = fopen(argv[1], "rb")))
	{
		printf("Failed to open file %s\n", argv[1]);
		return 3;
	}

	if (!strcasecmp(in_ext, "md3"))
	{
		FILE *outfile = fopen(argv[2], "w");
		if (!outfile)
		{
			printf("Failed to open file %s\n", argv[2]);
			fclose(infile);
			return 4;
		}
		retcode = convert_md3_to_obj(argv[1], infile, outfile,
			argc > 3 ? atoi(argv[3]) : 0);
		fclose(outfile);
	}
	else if (!strcasecmp(in_ext, "bsp"))
	{
		retcode = convert_bsp_to_obj(argv[1], infile, argv[2]);
	}
	else
	{
		printf("Unknown extension %s in file %s\n", in_ext, argv[1]);
		retcode = 5;
	}

	fclose(infile);

	return retcode;
}
