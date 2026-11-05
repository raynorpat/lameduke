/*
 * Polymost renderer implementation, split from polymost.c.
 * "POLYMOST" code written by Ken Silverman (http://www.advsys.net/ken).
 * See BUILDLIC.TXT. Included by polymost.c, not compiled separately.
 */

#define USEZBUFFER 1 //1:use zbuffer (slow, nice sprite rendering), 0:no zbuffer (fast, bad sprite rendering)
#define LINTERPSIZ 4 //log2 of interpolation size. 4:pretty fast&acceptable quality, 0:best quality/slow!
#define DEPTHDEBUG 0 //1:render distance instead of texture, for debugging only!, 0:default

#if (USEZBUFFER != 0)
int32_t *zbufmem = 0, zbufysiz = 0, zbufbpl = 0;
#endif
static int32_t zbufwrite = 1; //0: renderer 2's spans don't write the z-buffer

/*
================
drawline2d

Clip a debug line to the view and draw it in the indexed framebuffer.
Step the dominant axis in whole pixels and the other axis in 16.16.
================
*/
void drawline2d (float x0, float y0, float x1, float y1, char col)
{
	float f, dx, dy, fxres, fyres;
	int32_t e, inc, x, y;
	uint32_t up16;

	dx = x1-x0; dy = y1-y0; if ((dx == 0) && (dy == 0)) return;
	fxres = (float)xdimen; fyres = (float)ydimen;
		  if (x0 >= fxres) { if (x1 >= fxres) return; y0 += (fxres-x0)*dy/dx; x0 = fxres; }
	else if (x0 <      0) { if (x1 <      0) return; y0 += (    0-x0)*dy/dx; x0 =     0; }
		  if (x1 >= fxres) {                          y1 += (fxres-x1)*dy/dx; x1 = fxres; }
	else if (x1 <      0) {                          y1 += (    0-x1)*dy/dx; x1 =     0; }
		  if (y0 >= fyres) { if (y1 >= fyres) return; x0 += (fyres-y0)*dx/dy; y0 = fyres; }
	else if (y0 <      0) { if (y1 <      0) return; x0 += (    0-y0)*dx/dy; y0 =     0; }
		  if (y1 >= fyres) {                          x1 += (fyres-y1)*dx/dy; y1 = fyres; }
	else if (y1 <      0) {                          x1 += (    0-y1)*dx/dy; y1 =     0; }

	/* The unsigned range test below rejects negative and off-screen positions. */
	/* Step along the dominant axis so the fixed-point minor coordinate
	 * advances by at most about one pixel per iteration. */
	if (fabs(dx) > fabs(dy))
	{
		if (x0 > x1) { f = x0; x0 = x1; x1 = f; f = y0; y0 = y1; y1 = f; }
		y = (int32_t)(y0*65536.f)+32768;
		inc = (int32_t)(dy/dx*65536.f+.5f);
		x = (int32_t)(x0+.5); if (x < 0) { y -= inc*x; x = 0; } //if for safety
		e = (int32_t)(x1+.5); if (e > xdimen) e = xdimen;       //if for safety
		up16 = (ydimen<<16);
		for(;x<e;x++,y+=inc) if ((uint32_t)y < up16) *(char *)(ylookup[y>>16]+x+frameoffset) = col;
	}
	else
	{
		if (y0 > y1) { f = x0; x0 = x1; x1 = f; f = y0; y0 = y1; y1 = f; }
		x = (int32_t)(x0*65536.f)+32768;
		inc = (int32_t)(dx/dy*65536.f+.5f);
		y = (int32_t)(y0+.5); if (y < 0) { x -= inc*y; y = 0; } //if for safety
		e = (int32_t)(y1+.5); if (e > ydimen) e = ydimen;       //if for safety
		up16 = (xdimen<<16);
		for(;y<e;y++,x+=inc) if ((uint32_t)x < up16) *(char *)(ylookup[y]+(x>>16)+frameoffset) = col;
	}
}

/*
================
polymost_soft_drawpoly

Fill a projected convex polygon in the software framebuffer.
Mode 1 uses a flat debug colour and outline; mode 2 maps paletted ART.
================
*/
static void polymost_soft_drawpoly(const polymost_polygon *poly)
{
	double ngdx, ngdy, ngdo, ngux, nguy, nguo, ngvx, ngvy, ngvo, dp, up, vp, rdp;
	double ngdx2, ngux2, ngvx2;
	double f, r, ox, oy, oz;
	int32_t i, j, x, y, z, ix0, ix1, mini, maxi, tsizxm1, tsizym1, ltsizy;
	int32_t xx, yy, xi, d0, u0, v0, d1, u1, v1, xmodnice, ymulnice;
	char dacol, *palptr, *vidp, *vide;
	double *px = poly->px, *py = poly->py, *dd = poly->dd, *uu = poly->uu, *vv = poly->vv;
	int32_t n = poly->n, method = poly->method, dorot = poly->dorot, tsizx = poly->tsizx;
	int32_t tsizy = poly->tsizy;
	char *walptr = poly->walptr;


	if (rendmode == 2)
	{
#if (USEZBUFFER != 0)
		if ((!zbufmem) || (zbufbpl != bytesperline) || (zbufysiz != ydim))
		{
			zbufbpl = bytesperline;
			zbufysiz = ydim;
			/* One depth value per framebuffer byte, including the row padding. */
			zbufmem = (int32_t *)realloc(zbufmem,zbufbpl*zbufysiz*4);
		}
#endif
		/* Without a translucency lookup, use the masked path rather than
		 * dereferencing a missing blend table. */
		if ((!transluc)) method = (method&~3)+1; //In case translucent table doesn't exist

		if (!dorot)
		{
			/* The half-pixel offset evaluates the mapping planes at pixel centres,
			 * matching the scan conversion below. */
			ngdx = gdx; ngdy = gdy; ngdo = gdo+(ngdx+ngdy)*.5;
			ngux = gux; nguy = guy; nguo = guo+(ngux+nguy)*.5;
			ngvx = gvx; ngvy = gvy; ngvo = gvo+(ngvx+ngvy)*.5;
		}
		else
		{
				//General equations:
				//dd[i] = (px[i]*gdx + py[i]*gdy + gdo)
				//uu[i] = (px[i]*gux + py[i]*guy + guo)/dd[i]
				//vv[i] = (px[i]*gvx + py[i]*gvy + gvo)/dd[i]
				//
				//px[0]*gdx + py[0]*gdy + 1*gdo = dd[0]
				//px[1]*gdx + py[1]*gdy + 1*gdo = dd[1]
				//px[2]*gdx + py[2]*gdy + 1*gdo = dd[2]
				//
				//px[0]*gux + py[0]*guy + 1*guo = uu[0]*dd[0] (uu[i] premultiplied by dd[i] above)
				//px[1]*gux + py[1]*guy + 1*guo = uu[1]*dd[1]
				//px[2]*gux + py[2]*guy + 1*guo = uu[2]*dd[2]
				//
				//px[0]*gvx + py[0]*gvy + 1*gvo = vv[0]*dd[0] (vv[i] premultiplied by dd[i] above)
				//px[1]*gvx + py[1]*gvy + 1*gvo = vv[1]*dd[1]
				//px[2]*gvx + py[2]*gvy + 1*gvo = vv[2]*dd[2]
			/* Solve the screen-space planes for 1/z, u/z and v/z from three vertices. */
			ox = py[1]-py[2]; oy = py[2]-py[0]; oz = py[0]-py[1];
			r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
			ngdx = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
			ngux = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
			ngvx = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
			ox = px[2]-px[1]; oy = px[0]-px[2]; oz = px[1]-px[0];
			ngdy = (ox*dd[0] + oy*dd[1] + oz*dd[2])*r;
			nguy = (ox*uu[0] + oy*uu[1] + oz*uu[2])*r;
			ngvy = (ox*vv[0] + oy*vv[1] + oz*vv[2])*r;
			ox = px[0]-.5; oy = py[0]-.5; //.5 centers texture nicely
			ngdo = dd[0] - ox*ngdx - oy*ngdy;
			nguo = uu[0] - ox*ngux - oy*nguy;
			ngvo = vv[0] - ox*ngvx - oy*ngvy;
		}
		palptr = &palookup[globalpal][min(max(globalshade,0),numpalookups-1)<<8]; //<-need to make shade not static!

		/* Power-of-two dimensions wrap with masks; other widths need imod. */
		tsizxm1 = tsizx-1; xmodnice = (!(tsizxm1&tsizx));
		tsizym1 = tsizy-1; ymulnice = (!(tsizym1&tsizy));
		if ((method&4) && (!xmodnice)) //Sprites don't need a mod on texture coordinates
			{ xmodnice = 1; for(tsizxm1=1;tsizxm1<tsizx;tsizxm1=(tsizxm1<<1)+1); }
		if (!ymulnice) { for(tsizym1=1;tsizym1+1<tsizy;tsizym1=(tsizym1<<1)+1); }
		ltsizy = (picsiz[globalpicnum]>>4);
	}
	else
	{
		dacol = palookup[0][(int32_t)(*(char *)(waloff[globalpicnum]))+(min(max(globalshade,0),numpalookups-1)<<8)];
	}

	/* Mirror X and reverse vertex order together to retain the expected
	 * winding; flip each mapping plane's X slope about the same screen width. */
	if (grhalfxdown10x < 0) //Hack for mirrors
	{
		for(i=((n-1)>>1);i>=0;i--)
		{
			r = px[i]; px[i] = ((double)xdimen)-px[n-1-i]; px[n-1-i] = ((double)xdimen)-r;
			r = py[i]; py[i] = py[n-1-i]; py[n-1-i] = r;
		}
		ngdo += ((double)xdimen)*ngdx; ngdx = -ngdx;
		nguo += ((double)xdimen)*ngux; ngux = -ngux;
		ngvo += ((double)xdimen)*ngvx; ngvx = -ngvx;
	}

	/* Advance depth and texture numerators by a whole interpolation block
	 * before dividing to recover the next endpoint. */
	ngdx2 = ngdx*(1<<LINTERPSIZ);
	ngux2 = ngux*(1<<LINTERPSIZ);
	ngvx2 = ngvx*(1<<LINTERPSIZ);

	/* Record one edge in lastx, then walk the other to emit horizontal spans. */
	mini = (py[0] >= py[1]); maxi = 1-mini;
	for(z=2;z<n;z++)
	{
		if (py[z] < py[mini]) mini = z;
		if (py[z] > py[maxi]) maxi = z;
	}

	i = maxi; dtol(py[i],&yy); if (yy > ydimen) yy = ydimen;
	do
	{
		j = i+1; if (j == n) j = 0;
		dtol(py[j],&y); if (y < 0) y = 0;
		if (y < yy)
		{
			/* Edge X uses 14 fractional bits. The half-pixel biases select span
			 * limits from samples through the centres of the raster rows. */
			f = (px[j]-px[i])/(py[j]-py[i]); dtol(f*16384.0,&xi);
			dtol((((double)yy-.5-py[j])*f + px[j])*16384.0+8192.0,&x);
			for(;yy>y;yy--,x-=xi) lastx[yy-1] = (x>>14);
		}
		i = j;
	} while (i != mini);
	do
	{
		j = i+1; if (j == n) j = 0;
		dtol(py[j],&yy); if (yy > ydimen) yy = ydimen;
		if (y < yy)
		{
			/* Edge X uses 14 fractional bits. The half-pixel biases select span
			 * limits from samples through the centres of the raster rows. */
			f = (px[j]-px[i])/(py[j]-py[i]); dtol(f*16384.0,&xi);
			dtol((((double)y+.5-py[j])*f + px[j])*16384.0+8192.0,&x);
			for(;y<yy;y++,x+=xi)
			{
				/* Clip the horizontal span to [0,xdimen); ix1 is an exclusive endpoint
				 * for both flat fills and textured blocks. */
				ix0 = lastx[y]; if (ix0 < 0) ix0 = 0;
				ix1 = (x>>14); if (ix1 > xdimen) ix1 = xdimen;
				if (ix0 < ix1)
				{
					if (rendmode == 1)
						memset((void *)(ylookup[y]+ix0+frameoffset),dacol,ix1-ix0);
					else
					{
						vidp = (char *)(ylookup[y]+frameoffset+ix0);
						dp = ngdx*(double)ix0 + ngdy*(double)y + ngdo;
						up = ngux*(double)ix0 + nguy*(double)y + nguo;
						vp = ngvx*(double)ix0 + ngvy*(double)y + ngvo;
						/* dp is the reciprocal-depth plane; division produces proportional depth
						 * and the perspective-correct texture coordinates in 16.16 form. */
						rdp = 65536.0/dp; dp += ngdx2;
						dtol(   rdp,&d0);
						dtol(up*rdp,&u0); up += ngux2;
						dtol(vp*rdp,&v0); vp += ngvx2;
						rdp = 65536.0/dp;

						/* Divide at block ends, then interpolate depth/u/v in 16.16 within
						 * each block. LINTERPSIZ trades perspective error for fewer divides. */
						switch (method&3)
						{
							/* Opaque spans write every texel; their specialized power-of-two path
							 * uses shifts and masks instead of general column multiplication/modulo. */
							case 0:
								if (xmodnice&ymulnice) //both u&v texture sizes are powers of 2 :)
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[min(ix1-xx,1<<LINTERPSIZ)];
										while (vidp < vide)
										{
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											zbufmem[vidp-(char *)frameplace] = d0+16384; //+ hack so wall&floor sprites don't disappear
#endif
											vidp[0] = palptr[(unsigned char)walptr[(((u0>>16)&tsizxm1)<<ltsizy) + ((v0>>16)&tsizym1)]]; //+((d0>>13)&0x3f00)];
#else
											vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										} while (vidp < vide);
									}
								}
								else
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[min(ix1-xx,1<<LINTERPSIZ)];
										while (vidp < vide)
										{
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											if (zbufwrite) zbufmem[vidp-(char *)frameplace] = d0;
#endif
											vidp[0] = palptr[(unsigned char)walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)]]; //+((d0>>13)&0x3f00)];
#else
											vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										} while (vidp < vide);
									}
								}
								break;
							/* Masked spans discard palette index 255 before touching colour or depth.
							 * With a Z buffer, zbufwrite controls updates independently of depth testing. */
							case 1:
								if (xmodnice) //both u&v texture sizes are powers of 2 :)
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[min(ix1-xx,1<<LINTERPSIZ)];
										while (vidp < vide)
										{
											dacol = walptr[(((u0>>16)&tsizxm1)*tsizy) + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											if ((dacol != 255) && (d0 <= zbufmem[vidp-(char *)frameplace]))
											{
												if (zbufwrite) zbufmem[vidp-(char *)frameplace] = d0;
												vidp[0] = palptr[((int32_t)dacol)]; //+((d0>>13)&0x3f00)];
											}
#else
											if (dacol != 255) vidp[0] = palptr[((int32_t)dacol)]; //+((d0>>13)&0x3f00)];
#endif
#else
											if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										} while (vidp < vide);
									}
								}
								else
								{
									for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
									{
										dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
										dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
										dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
										rdp = 65536.0/dp; vide = &vidp[min(ix1-xx,1<<LINTERPSIZ)];
										while (vidp < vide)
										{
											dacol = walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
											if ((dacol != 255) && (d0 <= zbufmem[vidp-(char *)frameplace]))
											{
												if (zbufwrite) zbufmem[vidp-(char *)frameplace] = d0;
												vidp[0] = palptr[((int32_t)dacol)]; //+((d0>>13)&0x3f00)];
											}
#else
											if (dacol != 255) vidp[0] = palptr[((int32_t)dacol)]; //+((d0>>13)&0x3f00)];
#endif
#else
											if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
											d0 += d1; u0 += u1; v0 += v1; vidp++;
										} while (vidp < vide);
									}
								}
								break;
							/* Normal translucency indexes the blend table by destination then
							 * shaded source; it blends palette indices rather than RGB channels. */
							case 2: //Transluscence #1
								for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
								{
									dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
									dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
									dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
									rdp = 65536.0/dp; vide = &vidp[min(ix1-xx,1<<LINTERPSIZ)];
									while (vidp < vide)
									{
										dacol = walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)];
										//dacol = walptr[(((u0>>16)&tsizxm1)<<ltsizy) + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
										if ((dacol != 255) && (d0 <= zbufmem[vidp-(char *)frameplace]))
										{
											if (zbufwrite) zbufmem[vidp-(char *)frameplace] = d0;
											vidp[0] = transluc[(((int32_t)vidp[0])<<8)+((int32_t)palptr[((int32_t)dacol)])]; //+((d0>>13)&0x3f00)])];
										}
#else
										if (dacol != 255)
											vidp[0] = transluc[(((int32_t)vidp[0])<<8)+((int32_t)palptr[((int32_t)dacol)])]; //+((d0>>13)&0x3f00)])];
#endif
#else
										if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
										d0 += d1; u0 += u1; v0 += v1; vidp++;
									} while (vidp < vide);
								}
								break;
							/* Reverse translucency exchanges the source and destination byte
							 * positions in that same blend table. */
							case 3: //Transluscence #2
								for(xx=ix0;xx<ix1;xx+=(1<<LINTERPSIZ))
								{
									dtol(   rdp,&d1); dp += ngdx2; d1 = ((d1-d0)>>LINTERPSIZ);
									dtol(up*rdp,&u1); up += ngux2; u1 = ((u1-u0)>>LINTERPSIZ);
									dtol(vp*rdp,&v1); vp += ngvx2; v1 = ((v1-v0)>>LINTERPSIZ);
									rdp = 65536.0/dp; vide = &vidp[min(ix1-xx,1<<LINTERPSIZ)];
									while (vidp < vide)
									{
										dacol = walptr[imod(u0>>16,tsizx)*tsizy + ((v0>>16)&tsizym1)];
										//dacol = walptr[(((u0>>16)&tsizxm1)<<ltsizy) + ((v0>>16)&tsizym1)];
#if (DEPTHDEBUG == 0)
#if (USEZBUFFER != 0)
										if ((dacol != 255) && (d0 <= zbufmem[vidp-(char *)frameplace]))
										{
											if (zbufwrite) zbufmem[vidp-(char *)frameplace] = d0;
											vidp[0] = transluc[((int32_t)vidp[0])+(((int32_t)palptr[((int32_t)dacol)/*+((d0>>13)&0x3f00)*/])<<8)];
										}
#else
										if (dacol != 255)
											vidp[0] = transluc[((int32_t)vidp[0])+(((int32_t)palptr[((int32_t)dacol)/*+((d0>>13)&0x3f00)*/])<<8)];
#endif
#else
										if ((dacol != 255) && (vidp[0] > (d0>>16))) vidp[0] = ((d0>>16)&255);
#endif
										d0 += d1; u0 += u1; v0 += v1; vidp++;
									} while (vidp < vide);
								}
								break;
						}
					}
				}
			}
		}
		i = j;
	} while (i != maxi);

	if (rendmode == 1)
	{
		for(i=0,j=n-1;i<n;j=i,i++) drawline2d(px[i],py[i],px[j],py[j],31); //hopefully color index 31 is white

		//ox = 0; oy = 0;
		//for(i=0;i<n;i++) { ox += px[i]; oy += py[i]; }
		//ox /= (double)n; oy /= (double)n;
		//for(i=0,j=n-1;i<n;j=i,i++) drawline2d(px[i]+(ox-px[i])*.125,py[i]+(oy-py[i])*.125,px[j]+(ox-px[j])*.125,py[j]+(oy-py[j])*.125,31);
	}
}
