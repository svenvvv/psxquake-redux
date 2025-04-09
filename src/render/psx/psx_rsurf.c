/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_surf.c: surface-related refresh code

#include "quakedef.h"

int skytexturenum;

#ifndef GL_RGBA4
#define GL_RGBA4 0
#endif

unsigned blocklights[18 * 18];

typedef struct glRect_s {
    unsigned char l, t, w, h;
} glRect_t;

// For gl_texsort 0
msurface_t *skychain = NULL;
msurface_t *waterchain = NULL;

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights(msurface_t *surf)
{
    int lnum;
    int sd, td;
    float dist, rad, minlight;
    vec3_t impact, local;
    int s, t;
    int i;
    int smax, tmax;
    mtexinfo_t *tex;

    smax = (surf->extents[0] >> 4) + 1;
    tmax = (surf->extents[1] >> 4) + 1;
    tex = surf->texinfo;

    for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
        if (!(surf->dlightbits & (1 << lnum)))
            continue; // not lit by this light

        rad = cl_dlights[lnum].radius;
        dist = DotProduct(cl_dlights[lnum].origin, surf->plane->normal) - surf->plane->dist;
        rad -= fabsf(dist);
        minlight = cl_dlights[lnum].minlight;
        if (rad < minlight)
            continue;
        minlight = rad - minlight;

        for (i = 0; i < 3; i++) {
            impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;
        }

        local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3];
        local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3];

        local[0] -= surf->texturemins[0];
        local[1] -= surf->texturemins[1];

        for (t = 0; t < tmax; t++) {
            td = local[1] - t * 16;
            if (td < 0)
                td = -td;
            for (s = 0; s < smax; s++) {
                sd = local[0] - s * 16;
                if (sd < 0)
                    sd = -sd;
                if (sd > td)
                    dist = sd + (td >> 1);
                else
                    dist = td + (sd >> 1);
                if (dist < minlight)
                    blocklights[t * smax + s] += (rad - dist) * 256;
            }
        }
    }
}

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation(texture_t *base)
{
    int reletive;
    int count;

    if (currententity->frame) {
        if (base->alternate_anims)
            base = base->alternate_anims;
    }

    if (!base->anim_total)
        return base;

    reletive = (int)(cl.time / 100) % base->anim_total;

    count = 0;
    while (base->anim_min > reletive || base->anim_max <= reletive) {
        base = base->anim_next;
        if (!base)
            Sys_Error("R_TextureAnimation: broken cycle");
        if (++count > 100)
            Sys_Error("R_TextureAnimation: infinite cycle");
    }

    return base;
}

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

extern int solidskytexture;
extern int alphaskytexture;
extern uint32_t speedscale; // for top sky and bottom sky

void DrawGLWaterPoly(glpoly_t *p);
void DrawGLWaterPolyLightmap(glpoly_t *p);

lpMTexFUNC qglMTexCoord2fSGIS = NULL;
lpSelTexFUNC qglSelectTextureSGIS = NULL;

qboolean mtexenabled = false;

#if 0
/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	texture_t	*t;

	//
	// normal lightmaped poly
	//
	if (! (s->flags & (SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER) ) )
	{
		p = s->polys;

		t = R_TextureAnimation (s->texinfo->texture);
		GL_Bind (t->gl_texturenum);
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			glTexCoord2f (v[3], v[4]);
			glVertex3fv (v);
		}
		glEnd ();

		GL_Bind (lightmap_textures + s->lightmaptexturenum);
		glEnable (GL_BLEND);
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			glTexCoord2f (v[5], v[6]);
			glVertex3fv (v);
		}
		glEnd ();

		glDisable (GL_BLEND);

		return;
	}

	//
	// subdivided water surface warp
	//
	if (s->flags & SURF_DRAWTURB)
	{
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);
		return;
	}

	//
	// subdivided sky warp
	//
	if (s->flags & SURF_DRAWSKY)
	{
		GL_Bind (solidskytexture);
		speedscale = realtime*8;
		speedscale -= (int)speedscale;

		EmitSkyPolys (s);

		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind (alphaskytexture);
		speedscale = realtime*16;
		speedscale -= (int)speedscale;
		EmitSkyPolys (s);
		if (gl_lightmap_format == GL_LUMINANCE)
			glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

		glDisable (GL_BLEND);
	}

	//
	// underwater warped with lightmap
	//
	p = s->polys;

	t = R_TextureAnimation (s->texinfo->texture);
	GL_Bind (t->gl_texturenum);
	DrawGLWaterPoly (p);

	GL_Bind (lightmap_textures + s->lightmaptexturenum);
	glEnable (GL_BLEND);
	DrawGLWaterPolyLightmap (p);
	glDisable (GL_BLEND);
}
#else
/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly(msurface_t *s)
{
    glpoly_t *p;
    float *v;
    int i;
    texture_t *t;
    vec3_t nv;
    glRect_t *theRect;

    //
    // normal lightmaped poly
    //

    if (!(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER))) {
        p = s->polys;

        t = R_TextureAnimation(s->texinfo->texture);
        GL_Bind(t->gl_texturenum);
        glBegin(GL_POLYGON);
        v = p->verts[0];
        for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
            glTexCoord2f(v[3], v[4]);
            glVertex3fv(v);
        }
        glEnd();
        return;
    }

    //
    // subdivided water surface warp
    //

    if (s->flags & SURF_DRAWTURB) {
        GL_Bind(s->texinfo->texture->gl_texturenum);
        EmitWaterPolys(s);
        return;
    }

    //
    // subdivided sky warp
    //
    if (s->flags & SURF_DRAWSKY) {
        GL_Bind(solidskytexture);
        speedscale = realtime;
        // speedscale -= (int)speedscale & ~127;

        EmitSkyPolys(s);

        glEnable(GL_BLEND);
        GL_Bind(alphaskytexture);
        speedscale = realtime * 2;
        // speedscale -= (int)speedscale & ~127;
        EmitSkyPolys(s);

        glDisable(GL_BLEND);
        return;
    }

    //
    // underwater warped with lightmap
    //
    p = s->polys;

    t = R_TextureAnimation(s->texinfo->texture);
    GL_Bind(t->gl_texturenum);
    DrawGLWaterPoly(p);
}
#endif

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly(glpoly_t *p)
{
    int i;
    float *v;
    vec3_t nv;
    float realtime_f = (float) realtime / MS_PER_S;

    glBegin(GL_TRIANGLE_FAN);
    v = p->verts[0];
    for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
        glTexCoord2f(v[3], v[4]);

        nv[0] = v[0] + 8 * sinf(v[1] * 0.05f + realtime_f) * sinf(v[2] * 0.05f + realtime_f);
        nv[1] = v[1] + 8 * sinf(v[0] * 0.05f + realtime_f) * sinf(v[2] * 0.05f + realtime_f);
        nv[2] = v[2];

        glVertex3fv(nv);
    }
    glEnd();
}

void DrawGLWaterPolyLightmap(glpoly_t *p)
{
    int i;
    float *v;
    vec3_t nv;
    float realtime_f = (float) realtime / MS_PER_S;

    glBegin(GL_TRIANGLE_FAN);
    v = p->verts[0];
    for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
        glTexCoord2f(v[5], v[6]);

        nv[0] = v[0] + 8 * sinf(v[1] * 0.05f + realtime_f) * sinf(v[2] * 0.05f + realtime_f);
        nv[1] = v[1] + 8 * sinf(v[0] * 0.05f + realtime_f) * sinf(v[2] * 0.05f + realtime_f);
        nv[2] = v[2];

        glVertex3fv(nv);
    }
    glEnd();
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly(glpoly_t *p)
{
    int i;
    float *v;

    glBegin(GL_POLYGON);
    v = p->verts[0];
    for (i = 0; i < p->numverts; i++, v += VERTEXSIZE) {
        glTexCoord2f(v[3], v[4]);
        glVertex3fv(v);
    }
    glEnd();
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly(msurface_t *fa)
{
    texture_t *t;

    c_brush_polys++;

    if (fa->flags & SURF_DRAWSKY) { // warp texture, no lightmaps
        EmitBothSkyLayers(fa);
        return;
    }

    t = R_TextureAnimation(fa->texinfo->texture);
    GL_Bind(t->gl_texturenum);

    if (fa->flags & SURF_DRAWTURB) { // warp texture, no lightmaps
        EmitWaterPolys(fa);
        return;
    }

    if (fa->flags & SURF_UNDERWATER)
        DrawGLWaterPoly(fa->polys);
    else
        DrawGLPoly(fa->polys);
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain(msurface_t *s)
{
    if (mirror)
        return;
    mirror = true;
    mirror_plane = s->plane;
}

#if 0
/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	if (r_wateralpha.value == 1.0)
		return;

	//
	// go back to the world matrix
	//
    glLoadMatrixf (r_world_matrix);

	glEnable (GL_BLEND);
	glColor4f (1,1,1,r_wateralpha.value);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if ( !(s->flags & SURF_DRAWTURB) )
			continue;

		// set modulate mode explicitly
		GL_Bind (t->gl_texturenum);

		for ( ; s ; s=s->texturechain)
			R_RenderBrushPoly (s);

		t->texturechain = NULL;
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glColor4f (1,1,1,1);
	glDisable (GL_BLEND);
}
#else
/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces(void)
{
    int i;
    msurface_t *s;
    texture_t *t;

    if (r_wateralpha.value == 1.0f && gl_texsort.value)
        return;

    //
    // go back to the world matrix
    //

    glLoadMatrixf(r_world_matrix);

    if (r_wateralpha.value < 1.0f) {
        glEnable(GL_BLEND);
        glColor4f(1, 1, 1, r_wateralpha.value);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    if (!gl_texsort.value) {
        if (!waterchain)
            return;

        for (s = waterchain; s; s = s->texturechain) {
            GL_Bind(s->texinfo->texture->gl_texturenum);
            EmitWaterPolys(s);
        }

        waterchain = NULL;
    } else {
        for (i = 0; i < cl.worldmodel->numtextures; i++) {
            t = cl.worldmodel->textures[i];
            if (!t)
                continue;
            s = t->texturechain;
            if (!s)
                continue;
            if (!(s->flags & SURF_DRAWTURB))
                continue;

            // set modulate mode explicitly

            GL_Bind(t->gl_texturenum);

            for (; s; s = s->texturechain)
                EmitWaterPolys(s);

            t->texturechain = NULL;
        }
    }

    if (r_wateralpha.value < 1.0f) {
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

        glColor4f(1, 1, 1, 1);
        glDisable(GL_BLEND);
    }
}

#endif

/*
================
DrawTextureChains
================
*/
void DrawTextureChains(void)
{
    int i;
    msurface_t *s;
    texture_t *t;

    if (!gl_texsort.value) {
        if (skychain) {
            R_DrawSkyChain(skychain);
            skychain = NULL;
        }

        return;
    }

    for (i = 0; i < cl.worldmodel->numtextures; i++) {
        t = cl.worldmodel->textures[i];
        if (!t)
            continue;
        s = t->texturechain;
        if (!s)
            continue;
        if (i == skytexturenum)
            R_DrawSkyChain(s);
        else if (i == mirrortexturenum && r_mirroralpha.value != 1.0f) {
            R_MirrorChain(s);
            continue;
        } else {
            if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0f)
                continue; // draw translucent water later
            for (; s; s = s->texturechain)
                R_RenderBrushPoly(s);
        }

        t->texturechain = NULL;
    }
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel(entity_t *e)
{
    int k;
    vec3_t mins, maxs;
    int i;
    msurface_t *psurf;
    float dot;
    mplane_t *pplane;
    model_t *clmodel;
    qboolean rotated;

    currententity = e;
    currenttexture = -1;

    clmodel = e->model;

    if (e->angles[0] || e->angles[1] || e->angles[2]) {
        rotated = true;
        for (i = 0; i < 3; i++) {
            mins[i] = e->origin[i] - clmodel->radius;
            maxs[i] = e->origin[i] + clmodel->radius;
        }
    } else {
        rotated = false;
        VectorAdd(e->origin, clmodel->mins, mins);
        VectorAdd(e->origin, clmodel->maxs, maxs);
    }

    if (R_CullBox(mins, maxs))
        return;

    glColor3f(1, 1, 1);

    VectorSubtract(r_refdef.vieworg, e->origin, modelorg);
    if (rotated) {
        vec3_t temp;
        vec3_t forward, right, up;

        VectorCopy(modelorg, temp);
        AngleVectors(e->angles, forward, right, up);
        modelorg[0] = DotProduct(temp, forward);
        modelorg[1] = -DotProduct(temp, right);
        modelorg[2] = DotProduct(temp, up);
    }

    psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    // calculate dynamic lighting for bmodel if it's not an
    // instanced model
    if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value) {
        for (k = 0; k < MAX_DLIGHTS; k++) {
            if ((cl_dlights[k].die < cl.time) || (!cl_dlights[k].radius))
                continue;

            R_MarkLights(&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
        }
    }

    glPushMatrix();
    e->angles[0] = -e->angles[0]; // stupid quake bug
    R_RotateForEntity(e);
    e->angles[0] = -e->angles[0]; // stupid quake bug

    //
    // draw texture
    //
    for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
        // find which side of the node we are on
        pplane = psurf->plane;

        dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

        // draw the polygon
        if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
            (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
            if (gl_texsort.value)
                R_RenderBrushPoly(psurf);
            else
                R_DrawSequentialPoly(psurf);
        }
    }

    glPopMatrix();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode(mnode_t *node)
{
    int c, side;
    mplane_t *plane;
    msurface_t *surf, **mark;
    mleaf_t *pleaf;
    float dot;

    if (node->contents == CONTENTS_SOLID)
        return; // solid

    if (node->visframe != r_visframecount)
        return;
    if (R_CullBox(node->minmaxs, node->minmaxs + 3))
        return;

    // if a leaf node, draw stuff
    if (node->contents < 0) {
        pleaf = (mleaf_t *)node;

        mark = pleaf->firstmarksurface;
        c = pleaf->nummarksurfaces;

        if (c) {
            do {
                (*mark)->visframe = r_framecount;
                mark++;
            } while (--c);
        }

        // deal with model fragments in this leaf
        if (pleaf->efrags)
            R_StoreEfrags(&pleaf->efrags);

        return;
    }

    // node is just a decision point, so go down the apropriate sides

    // find which side of the node we are on
    plane = node->plane;

    switch (plane->type) {
    case PLANE_X:
        dot = modelorg[0] - plane->dist;
        break;
    case PLANE_Y:
        dot = modelorg[1] - plane->dist;
        break;
    case PLANE_Z:
        dot = modelorg[2] - plane->dist;
        break;
    default:
        dot = DotProduct(modelorg, plane->normal) - plane->dist;
        break;
    }

    if (dot >= 0)
        side = 0;
    else
        side = 1;

    // recurse down the children, front side first
    R_RecursiveWorldNode(node->children[side]);

    // draw stuff
    c = node->numsurfaces;

    if (c) {
        surf = cl.worldmodel->surfaces + node->firstsurface;

        if (dot < 0 - BACKFACE_EPSILON)
            side = SURF_PLANEBACK;
        else if (dot > BACKFACE_EPSILON)
            side = 0;
        {
            for (; c; c--, surf++) {
                if (surf->visframe != r_framecount)
                    continue;

                // don't backface underwater surfaces, because they warp
                if (!(surf->flags & SURF_UNDERWATER) && ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)))
                    continue; // wrong side

                // if sorting by texture, just store it out
                if (gl_texsort.value) {
                    if (!mirror || surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum]) {
                        surf->texturechain = surf->texinfo->texture->texturechain;
                        surf->texinfo->texture->texturechain = surf;
                    }
                } else if (surf->flags & SURF_DRAWSKY) {
                    surf->texturechain = skychain;
                    skychain = surf;
                } else if (surf->flags & SURF_DRAWTURB) {
                    surf->texturechain = waterchain;
                    waterchain = surf;
                } else
                    R_DrawSequentialPoly(surf);
            }
        }
    }

    // recurse down the back side
    R_RecursiveWorldNode(node->children[!side]);
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld(void)
{
    entity_t ent;

    memset(&ent, 0, sizeof(ent));
    ent.model = cl.worldmodel;

    VectorCopy(r_refdef.vieworg, modelorg);

    currententity = &ent;
    currenttexture = -1;

    glColor3f(1, 1, 1);
#ifdef QUAKE2
    R_ClearSkyBox();
#endif

    R_RecursiveWorldNode(cl.worldmodel->nodes);

    DrawTextureChains();

#ifdef QUAKE2
    R_DrawSkyBox();
#endif
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves(void)
{
    byte *vis;
    mnode_t *node;
    int i;
    byte solid[4096];

    if (r_oldviewleaf == r_viewleaf && !r_novis.value)
        return;

    if (mirror)
        return;

    r_visframecount++;
    r_oldviewleaf = r_viewleaf;

    if (r_novis.value) {
        vis = solid;
        memset(solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
    } else
        vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel);

    for (i = 0; i < cl.worldmodel->numleafs; i++) {
        if (vis[i >> 3] & (1 << (i & 7))) {
            node = (mnode_t *)&cl.worldmodel->leafs[i + 1];
            do {
                if (node->visframe == r_visframecount)
                    break;
                node->visframe = r_visframecount;
                node = node->parent;
            } while (node);
        }
    }
}

mvertex_t *r_pcurrentvertbase;
model_t *currentmodel;

int nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList(msurface_t *fa)
{
    int i, lindex, lnumverts;
    medge_t *pedges, *r_pedge;
    float *vec;
    float s, t;
    glpoly_t *poly;

    // reconstruct the polygon
    pedges = currentmodel->edges;
    lnumverts = fa->numedges;

    //
    // draw texture
    //
    poly = Hunk_Alloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
    poly->next = fa->polys;
    poly->flags = fa->flags;
    fa->polys = poly;
    poly->numverts = lnumverts;

    for (i = 0; i < lnumverts; i++) {
        lindex = currentmodel->surfedges[fa->firstedge + i];

        if (lindex > 0) {
            r_pedge = &pedges[lindex];
            vec = r_pcurrentvertbase[r_pedge->v[0]].position;
        } else {
            r_pedge = &pedges[-lindex];
            vec = r_pcurrentvertbase[r_pedge->v[1]].position;
        }
        s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
        s /= fa->texinfo->texture->width;

        t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
        t /= fa->texinfo->texture->height;

        VectorCopy(vec, poly->verts[i]);
        poly->verts[i][3] = s;
        poly->verts[i][4] = t;

        //
        // lightmap texture coordinates
        // not supported on PSX
        //
        poly->verts[i][5] = s;
        poly->verts[i][6] = t;
    }

    //
    // remove co-linear points - Ed
    //
    if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER)) {
        for (i = 0; i < lnumverts; ++i) {
            vec3_t v1, v2;
            float *prev, *this, *next;

            prev = poly->verts[(i + lnumverts - 1) % lnumverts];
            this = poly->verts[i];
            next = poly->verts[(i + 1) % lnumverts];

            VectorSubtract(this, prev, v1);
            VectorNormalize(v1);
            VectorSubtract(next, prev, v2);
            VectorNormalize(v2);

// skip co-linear points
#define COLINEAR_EPSILON 0.001
            if ((fabs(v1[0] - v2[0]) <= COLINEAR_EPSILON) && (fabs(v1[1] - v2[1]) <= COLINEAR_EPSILON) &&
                (fabs(v1[2] - v2[2]) <= COLINEAR_EPSILON)) {
                int j;
                for (j = i + 1; j < lnumverts; ++j) {
                    int k;
                    for (k = 0; k < VERTEXSIZE; ++k)
                        poly->verts[j - 1][k] = poly->verts[j][k];
                }
                --lnumverts;
                ++nColinElim;
                // retry next vertex next time, which is now current vertex
                --i;
            }
        }
    }
    poly->numverts = lnumverts;
}
