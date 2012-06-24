/*
MD3 and/or BSP to ASE converter
Written by Leszek Godlewski <github@inequation.org>
Code in the public domain
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];
typedef unsigned char byte;
#include "qfiles.h"

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

int convert_md3(const char *in_name, FILE *in, FILE *out, int frame) {
	md3Header_t md3;
	md3Surface_t surf;
	md3XyzNormal_t vert;
	md3Triangle_t tri;
	md3St_t st;
	int surf_start, i, j;

	// MD3 sanity checking

	if (!fread(&md3, sizeof(md3), 1, in)
		|| little_long(md3.ident != MD3_IDENT)) {
		printf("Not a valid MD3 file\n");
		return 6;
	}

	if (little_long(md3.version > MD3_VERSION)) {
		printf("Unsupported MD3 version\n");
		return 7;
	}

	if (little_long(md3.numFrames) < 1) {
		printf("MD3 has no frames\n");
		return 8;
	}

	if (little_long(md3.numFrames) <= frame) {
		printf("Cannot extract frame #%d from a model that has %d frames\n",
			frame, little_long(md3.numFrames));
		return 9;
	}

	if (little_long(md3.numSurfaces) < 1) {
		printf("MD3 has no surfaces\n");
		return 10;
	}

	// begin ASE data

	fprintf(out,
		"*3DSMAX_ASCIIEXPORT 200\n"
		"*COMMENT \"MD3 and/or BSP to ASE converter %s\"\n"
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

	// iterate over all the MD3 surfaces
	surf_start = little_long(md3.ofsSurfaces);
	for (i = 0; i < little_long(md3.numSurfaces);
		++i, surf_start += little_long(surf.ofsEnd)) {

		fseek(in, surf_start, SEEK_SET);
		if (!fread(&surf, sizeof(surf), 1, in)) {
			printf("Failed to read surface #%d data\n", i);
			return 11;
		}

		printf("Processing surface #%d, \"%s\": %d vertices, %d triangles\n",
			i, surf.name, little_long(surf.numVerts),
			little_long(surf.numTriangles));

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
			surf.name, surf.name, surf.numVerts, surf.numTriangles);

		// output the vertex list
		fprintf(out,
					"\t\t*MESH_VERTEX_LIST {\n");
		fseek(in, surf_start + little_long(surf.ofsXyzNormals)
			+ frame * little_long(surf.numVerts), SEEK_SET);
		for (j = 0; j < little_long(surf.numVerts); ++j) {
			if (!fread(&vert, sizeof(vert), 1, in)) {
				printf("Failed to read vertex #%d in surface #%d\n", j, i);
				return 12;
			}
			fprintf(out,
						"\t\t\t*MESH_VERTEX %d %f %f %f\n",
				j, (float)vert.xyz[0] * MD3_XYZ_SCALE,
					(float)vert.xyz[1] * MD3_XYZ_SCALE,
					(float)vert.xyz[2] * MD3_XYZ_SCALE);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// output the triangle list
		fprintf(out,
					"\t\t*MESH_FACE_LIST {\n");
		fseek(in, surf_start + little_long(surf.ofsTriangles), SEEK_SET);
		for (j = 0; j < little_long(surf.numTriangles); ++j) {
			if (!fread(&tri, sizeof(tri), 1, in)) {
				printf("Failed to read triangle #%d in surface #%d\n", j, i);
				return 13;
			}
			fprintf(out,
						"\t\t\t*MESH_FACE %d: A: %d B: %d C: %d "
							"AB: 1 BC: 1 CA: 1\n",
				j, tri.indexes[0], tri.indexes[1], tri.indexes[2]);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// output the texture vertex list
		fprintf(out,
					"\t\t*MESH_TVERT_LIST {\n");
		fseek(in, surf_start + little_long(surf.ofsSt), SEEK_SET);
		for (j = 0; j < little_long(surf.numVerts); ++j) {
			if (!fread(&st, sizeof(st), 1, in)) {
				printf("Failed to read texture coordinates #%d in surface "
					"#%d\n", j, i);
				return 14;
			}
			fprintf(out,
						"\t\t\t*MESH_TVERT %d %f %f 0.0000\n",
				j, st.st[0], st.st[1]);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// output the texture triangle list (same as the original triangles)
		fprintf(out,
					"\t\t*MESH_TFACE_LIST {\n");
		fseek(in, surf_start + little_long(surf.ofsTriangles), SEEK_SET);
		for (j = 0; j < little_long(surf.numTriangles); ++j) {
			if (!fread(&tri, sizeof(tri), 1, in)) {
				printf("Failed to read triangle #%d in surface #%d\n", j, i);
				return 13;
			}
			fprintf(out,
						"\t\t\t*MESH_TFACE %d %d %d %d\n",
				j, tri.indexes[0], tri.indexes[1], tri.indexes[2]);
		}
		fprintf(out,
					"\t\t}\n"
			"\n");

		// TODO: decode the vertex normals from polar coordinates
		/*lat = ( newNormals[0] >> 8 ) & 0xff;
		lng = ( newNormals[0] & 0xff );
		lat *= ( FUNCTABLE_SIZE / 256 );
		lng *= ( FUNCTABLE_SIZE / 256 );

		// decode X as cos( lat ) * sin( long )
		// decode Y as sin( lat ) * sin( long )
		// decode Z as cos( long )

		outNormal[0] = tr.sinTable[( lat + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK] * tr.sinTable[lng];
		outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
		outNormal[2] = tr.sinTable[( lng + ( FUNCTABLE_SIZE / 4 ) ) & FUNCTABLE_MASK];*/

		// close ASE geom object
		fprintf(out,
				"\t}\n"
			"\n"
				"\t*PROP_MOTIONBLUR 0\n"
				"\t*PROP_CASTSHADOW 1\n"
				"\t*PROP_RECVSHADOW 1\n"
			"}\n");
	}

	return 0;
}

int convert_bsp(FILE *in, FILE *out) {
	printf("Not implemented\n");
	return -1;
}

int main(int argc, char *argv[]) {
	FILE *infile, *outfile;
	char *ext;
	int retcode;

	if (argc < 3) {
		printf("Usage: %s <infile> <outfile> [frame number]\n", argv[0]);
		return 1;
	}

	ext = strrchr(argv[1], '.');
	if (ext == NULL) {
		printf("File %s appears to have no extension\n", argv[1]);
		return 2;
	}
	++ext;

	if (!(infile = fopen(argv[1], "rb"))) {
		printf("Failed to open file %s\n", argv[1]);
		return 3;
	}

	if (!(outfile = fopen(argv[2], "w"))) {
		printf("Failed to open file %s\n", argv[2]);
		fclose(infile);
		return 4;
	}

	if (!strcasecmp(ext, "md3"))
		retcode = convert_md3(argv[1], infile, outfile,
			argc > 3 ? atoi(argv[3]) : 0);
	else if (!strcasecmp(ext, "bsp"))
		retcode = convert_bsp(infile, outfile);
	else {
		printf("Unknown extension %s in file %s\n", ext, argv[1]);
		retcode = 5;
	}

	fclose(infile);
	fclose(outfile);

    return retcode;
}
