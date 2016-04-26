/****************************************************************************
 *                              ArtraCFD                                    *
 *                          <By Huangrui Mo>                                *
 * Copyright (C) Huangrui Mo <huangrui.mo@gmail.com>                        *
 * This file is part of ArtraCFD.                                           *
 * ArtraCFD is free software: you can redistribute it and/or modify it      *
 * under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation, either version 3 of the License, or        *
 * (at your option) any later version.                                      *
 ****************************************************************************/
/****************************************************************************
 * Required Header Files
 ****************************************************************************/
#include "paraview.h"
#include <stdio.h> /* standard library for input and output */
#include <string.h> /* manipulating strings */
#include "cfd_commons.h"
#include "commons.h"
/****************************************************************************
 * Static Function Declarations
 ****************************************************************************/
static int ReadCaseFile(Time *, ParaviewSet *);
static int ReadStructuredData(Space *, const Model *, ParaviewSet *);
static int PointPolyDataReader(Geometry *, const Time *);
static int ReadPointPolyData(const int, const int, Geometry *, ParaviewSet *);
static int PolygonPolyDataReader(Geometry *, const Time *);
static int ReadPolygonPolyData(const int, const int, Geometry *, ParaviewSet *);
/****************************************************************************
 * Function definitions
 ****************************************************************************/
int ReadStructuredDataParaview(Space *space, Time *time, const Model *model)
{
    ParaviewSet paraSet = { /* initialize environment */
        .rootName = "field", /* data file root name */
        .baseName = {'\0'}, /* data file base name */
        .fileName = {'\0'}, /* data file name */
        .fileExt = ".vts", /* data file extension */
        .intType = "Int32", /* paraview int type */
        .floatType = "Float32", /* paraview float type */
        .byteOrder = "LittleEndian" /* byte order of data */
    };
    snprintf(paraSet.baseName, sizeof(ParaviewString), "%s%05d", 
            paraSet.rootName, time->countOutput); 
    ReadCaseFile(time, &paraSet);
    ReadStructuredData(space, model, &paraSet);
    return 0;
}
static int ReadCaseFile(Time *time, ParaviewSet *paraSet)
{
    snprintf(paraSet->fileName, sizeof(ParaviewString), "%s.pvd", 
            paraSet->baseName); 
    FILE *filePointer = fopen(paraSet->fileName, "r");
    if (NULL == filePointer) {
        FatalError("failed to open case file...");
    }
    /* read information from file */
    String currentLine = {'\0'}; /* store current line */
    ReadInLine(&filePointer, "<!--");
    /* set format specifier according to the type of Real */
    char format[10] = {'\0'}; /* format information */
    strncpy(format, "%*s %lg", sizeof format); /* default is double type */
    if (sizeof(Real) == sizeof(float)) { /* if set Real as float */
        strncpy(format, "%*s %g", sizeof format); /* float type */
    }
    fgets(currentLine, sizeof currentLine, filePointer);
    sscanf(currentLine, format, &(time->now)); 
    fgets(currentLine, sizeof currentLine, filePointer);
    sscanf(currentLine, "%*s %d", &(time->countStep)); 
    fclose(filePointer); /* close current opened file */
    return 0;
}
static int ReadStructuredData(Space *space, const Model *model, ParaviewSet *paraSet)
{
    snprintf(paraSet->fileName, sizeof(ParaviewString), "%s%s", paraSet->baseName, paraSet->fileExt); 
    FILE *filePointer = fopen(paraSet->fileName, "r");
    if (NULL == filePointer) {
        FatalError("failed to open data file...");
    }
    ParaviewReal data = 0.0; /* paraview scalar data */
    /* set format specifier according to the type of Real */
    char format[5] = "%lg"; /* default is double type */
    if (sizeof(ParaviewReal) == sizeof(float)) {
        strncpy(format, "%g", sizeof format); /* float type */
    }
    int idx = 0; /* linear array index math variable */
    Node *node = space->node;
    Real *restrict U = NULL;
    const Partition *restrict part = &(space->part);
    /* get rid of redundant lines */
    String currentLine = {'\0'}; /* store current line */
    ReadInLine(&filePointer, "<PointData>");
    for (int count = 0; count < DIMU; ++count) {
        fgets(currentLine, sizeof currentLine, filePointer);
        for (int k = part->ns[PIN][Z][MIN]; k < part->ns[PIN][Z][MAX]; ++k) {
            for (int j = part->ns[PIN][Y][MIN]; j < part->ns[PIN][Y][MAX]; ++j) {
                for (int i = part->ns[PIN][X][MIN]; i < part->ns[PIN][X][MAX]; ++i) {
                    idx = IndexNode(k, j, i, part->n[Y], part->n[X]);
                    U = node[idx].U[C];
                    fscanf(filePointer, format, &data);
                    switch (count) {
                        case 0: /* rho */
                            U[0] = data;
                            break;
                        case 1: /* u */
                            U[1] = U[0] * data;
                            break;
                        case 2: /* v */
                            U[2] = U[0] * data;
                            break;
                        case 3: /* w */
                            U[3] = U[0] * data;
                            break;
                        case 4: /* p */
                            U[4] = 0.5 * (U[1] * U[1] + U[2] * U[2] + U[3] * U[3]) / U[0] + 
                                data / (model->gamma - 1.0);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        fgets(currentLine, sizeof currentLine, filePointer); /* get rid of the end of line of data */
        fgets(currentLine, sizeof currentLine, filePointer);
    }
    fclose(filePointer); /* close current opened file */
    return 0;
}
int ReadPolyDataParaview(Geometry *geo, const Time *time)
{
    if (0 != geo->sphereN) {
        PointPolyDataReader(geo, time);
    }
    if (0 != geo->stlN) {
        PolygonPolyDataReader(geo, time);
    }
    return 0;
}
static int PointPolyDataReader(Geometry *geo, const Time *time)
{
    ParaviewSet paraSet = { /* initialize environment */
        .rootName = "geo_sph", /* data file root name */
        .baseName = {'\0'}, /* data file base name */
        .fileName = {'\0'}, /* data file name */
        .fileExt = ".vtp", /* data file extension */
        .intType = "Int32", /* paraview int type */
        .floatType = "Float32", /* paraview float type */
        .byteOrder = "LittleEndian" /* byte order of data */
    };
    snprintf(paraSet.baseName, sizeof(ParaviewString), "%s%05d", 
            paraSet.rootName, time->countOutput); 
    ReadPointPolyData(0, geo->sphereN, geo, &paraSet);
    return 0;
}
static int ReadPointPolyData(const int start, const int end, Geometry *geo, ParaviewSet *paraSet)
{
    snprintf(paraSet->fileName, sizeof(ParaviewString), "%s%s", paraSet->baseName, paraSet->fileExt); 
    FILE *filePointer = fopen(paraSet->fileName, "r");
    if (NULL == filePointer) {
        FatalError("failed to open data file...");
    }
    ReadInLine(&filePointer, "<!--");
    ReadPolyhedronStateData(start, end, &filePointer, geo);
    fclose(filePointer); /* close current opened file */
    return 0;
}
static int PolygonPolyDataReader(Geometry *geo, const Time *time)
{
    ParaviewSet paraSet = { /* initialize environment */
        .rootName = "geo_stl", /* data file root name */
        .baseName = {'\0'}, /* data file base name */
        .fileName = {'\0'}, /* data file name */
        .fileExt = ".vtp", /* data file extension */
        .intType = "Int32", /* paraview int type */
        .floatType = "Float32", /* paraview float type */
        .byteOrder = "LittleEndian" /* byte order of data */
    };
    snprintf(paraSet.baseName, sizeof(ParaviewString), "%s%05d", 
            paraSet.rootName, time->countOutput); 
    ReadPolygonPolyData(geo->sphereN, geo->totalN, geo, &paraSet);
    return 0;
}
static int ReadPolygonPolyData(const int start, const int end, Geometry *geo, ParaviewSet *paraSet)
{
    snprintf(paraSet->fileName, sizeof(ParaviewString), "%s%s", paraSet->baseName, paraSet->fileExt); 
    FILE *filePointer = fopen(paraSet->fileName, "r");
    if (NULL == filePointer) {
        FatalError("failed to open data file...");
    }
    ParaviewReal data = 0.0; /* paraview scalar data */
    /* set format specifier according to the type of Real */
    char format[5] = "%lg"; /* default is double type */
    if (sizeof(ParaviewReal) == sizeof(float)) {
        strncpy(format, "%g", sizeof format); /* float type */
    }
    /* get rid of redundant lines */
    String currentLine = {'\0'}; /* store current line */
    ReadInLine(&filePointer, "<PolyData>");
    for (int n = start; n < end; ++n) {
        fgets(currentLine, sizeof currentLine, filePointer);
        sscanf(currentLine, "%*s %*s %*s NumberOfPolys=\"%d\"", &(geo->list[n].facetN)); 
        geo->list[n].facet = AssignStorage(geo->list[n].facetN, "Facet");
        fgets(currentLine, sizeof currentLine, filePointer);
        fgets(currentLine, sizeof currentLine, filePointer);
        fgets(currentLine, sizeof currentLine, filePointer);
        fgets(currentLine, sizeof currentLine, filePointer);
        fgets(currentLine, sizeof currentLine, filePointer);
        fgets(currentLine, sizeof currentLine, filePointer);
        fgets(currentLine, sizeof currentLine, filePointer);
        for (int m = 0; m < geo->list[n].facetN; ++m) {
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P1[X] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P1[Y] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P1[Z] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P2[X] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P2[Y] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P2[Z] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P3[X] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P3[Y] = data;
            fscanf(filePointer, format, &data);
            geo->list[n].facet[m].P3[Z] = data;
        }
        ReadInLine(&filePointer, "</Piece>");
    }
    ReadInLine(&filePointer, "<!--");
    ReadPolyhedronStateData(start, end, &filePointer, geo);
    fclose(filePointer); /* close current opened file */
    return 0;
}
int ReadPolyhedronStateData(const int start, const int end, FILE **filePointerPointer, Geometry *geo)
{
    FILE *filePointer = *filePointerPointer; /* get the value of file pointer */
    String currentLine = {'\0'}; /* store the current read line */
    /* set format specifier according to the type of Real */
    char format[100] = "%lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %lg, %d";
    if (sizeof(Real) == sizeof(float)) { /* if set Real as float */
        strncpy(format, "%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %d", sizeof format);
    }
    for (int n = start; n < end; ++n) {
        fgets(currentLine, sizeof currentLine, filePointer);
        sscanf(currentLine, format,
                &(geo->list[n].O[X]), &(geo->list[n].O[Y]), &(geo->list[n].O[Z]), &(geo->list[n].r),
                &(geo->list[n].V[X]), &(geo->list[n].V[Y]), &(geo->list[n].V[Z]),
                &(geo->list[n].F[X]), &(geo->list[n].F[Y]), &(geo->list[n].F[Z]),
                &(geo->list[n].rho), &(geo->list[n].T), &(geo->list[n].cf),
                &(geo->list[n].area), &(geo->list[n].volume), &(geo->list[n].matID));
        if (geo->sphereN > n) {
            geo->list[n].facetN = 0; /* analytical sphere tag */
            geo->list[n].facet = NULL;
        }
    }
    *filePointerPointer = filePointer; /* updated file pointer */
    return 0;
}
/* a good practice: end file with a newline */

