#include "../include/lofs-dirstruct.h"
#include "../include/lofs-limits.h"
#include "../include/lofs-hdf2nc.h"
#include "../include/lofs-read.h"
#include "../include/lofs-macros.h"

void init_structs(cmdline *cmd,dir_meta *dm, grid *gd,ncstruct *nc, readahead *rh)
{
	int i;

	cmd->histpath        = (char *) malloc(MAXSTR*sizeof(char));
	cmd->base            = (char *) malloc(MAXSTR*sizeof(char));
	dm-> firstfilename   = (char *) malloc(MAXSTR*sizeof(char));
	dm-> saved_base      = (char *) malloc(MAXSTR*sizeof(char));
	dm-> topdir          = (char *) malloc(MAXSTR*sizeof(char));
	nc->ncfilename       = (char *) malloc(MAXSTR*sizeof(char));
	nc->varnameid        = (int  *) malloc(MAXVARIABLES*sizeof(int));

	dm-> regenerate_cache = 0;

	gd->X0=-1;
	gd->Y0=-1;
	gd->X1=-1;
	gd->Y1=-1;
	gd->Z0=-1;
	gd->Z1=-1;
	gd->saved_X0=0;
	gd->saved_X1=0;
	gd->saved_Y0=0;
	gd->saved_Y1=0;
	gd->saved_Z0=0;
	gd->saved_Z1=0;
	cmd->time=0.0;
	cmd->got_base=0;
	cmd->optcount=0;
	cmd->debug=0;
	cmd->gzip=0;
	cmd->verbose=0;
	cmd->do_allvars=0;
	cmd->use_box_offset=0;
	cmd->use_interp=0;
	cmd->do_swaths=0;
	cmd->filetype=NC_NETCDF4;
	nc->twodslice=0;

	nc->varname = (char **)malloc(MAXVARIABLES * sizeof(char *));
	for (i=0; i < MAXVARIABLES; i++) nc->varname[i] = (char *)(malloc(MAXSTR * sizeof(char)));

	rh->u=0; rh->v=0; rh->w=0;
	rh->vortmag=0; rh->hvort=0; rh->streamvort=0;//Not really readahead, used for mallocs
}

void dealloc_structs(cmdline *cmd,dir_meta *dm, grid *gd,ncstruct *nc, readahead *rh) {
}


void copy_grid_to_requested_cube (requested_cube *rc, grid gd)
{
	rc->X0=gd.X0; rc->X1=gd.X1;
	rc->Y0=gd.Y0; rc->Y1=gd.Y1;
	rc->Z0=gd.Z0; rc->Z1=gd.Z1;
	rc->NX=gd.NX;
	rc->NY=gd.NY;
	rc->NZ=gd.NZ;
}

void write_hdf2nc_command_txtfile(int argc, char *argv[],ncstruct nc)
{
	int i;
	FILE *fp;
	char *cmdfilename;
	cmdfilename = (char *)malloc(MAXSTR*sizeof(char));
	sprintf(cmdfilename,"%s%s",nc.ncfilename,".cmd");
	if ((fp = fopen(cmdfilename,"w")) != NULL)
	{
		for (i=0; i<argc; i++)
		{
			fprintf(fp,"%s ",argv[i]);
		}
	fprintf(fp,"\n");
	fclose(fp);
    }
	free(cmdfilename);
}


void get_saved_base(char *timedir, char *saved_base)
{
	// Just grab the basename so we can have it set automatically
	// for the netcdf files if so desired. Apparenlty the easiest
	// way to do this is to just remove the last 14 characters of
	// one of the timedir directories

	int i,tdlen;
//ORF TODO: make 14 a constant somewhere
//We may wish to allow flexibility in the floating point
//left and right of the decimal.
	tdlen=strlen(timedir)-14;
	for(i=0;i<tdlen;i++)saved_base[i]=timedir[i];
	//strncpy(saved_base,timedir,tdlen);
	saved_base[tdlen]='\0';
}

void set_span(grid *gd,hdf_meta hm,cmdline cmd)
{
	/* We do this because of stencils. Note, saved_Z0 is always 0 in LOFS.
	 * If we want to save only elevated data, set lower data to float(0.0) and compress*/

	gd->saved_X0+=1; gd->saved_X1-=1;
	gd->saved_Y0+=1; gd->saved_Y1-=1;
	gd->saved_Z1-=2;

	/* X and Y values are with respect to (0,0) of the actual saved data, not with respect
	 * to actual (0,0) (done because I often choose new netCDF subdomains from ncview).
	 * --offset allows this
	 */

	/* If we didn't specify values at the command line, value is -1
	 * Set them to values specifying all the saved data */

	if(cmd.use_box_offset)
	{
		if (gd->X0<0) gd->X0 = gd->saved_X0; else gd->X0 = gd->X0 + gd->saved_X0;
		if (gd->X1<0) gd->X1 = gd->saved_X1; else gd->X1 = gd->X1 + gd->saved_X0;
		if (gd->Y0<0) gd->Y0 = gd->saved_Y0; else gd->Y0 = gd->Y0 + gd->saved_Y0;
		if (gd->Y1<0) gd->Y1 = gd->saved_Y1; else gd->Y1 = gd->Y1 + gd->saved_Y0;
	}
	else
	{
		if (gd->X0<0) gd->X0 = gd->saved_X0;
		if (gd->X1<0) gd->X1 = gd->saved_X1;
		if (gd->Y0<0) gd->Y0 = gd->saved_Y0;
		if (gd->Y1<0) gd->Y1 = gd->saved_Y1;
	}
	if (gd->Z0<0) gd->Z0 = gd->saved_Z0;
	if (gd->Z1<0) gd->Z1 = gd->saved_Z1; //ORF -2 not -1 now

	if (gd->X0>gd->X1||gd->Y0>gd->Y1||gd->X1<gd->saved_X0||gd->Y1<gd->saved_Y0||gd->X0>gd->saved_X1||gd->Y0>gd->saved_Y1)
	{
		printf(" *** X0=%i saved_X0=%i Y0=%i saved_Y0=%i X1=%i saved_X1=%i Y1=%i saved_Y1=%i\n",
				gd->X0,gd->saved_X0,gd->Y0,gd->saved_Y0,gd->X1,gd->saved_X1,gd->Y1,gd->saved_Y1);
		ERROR_STOP("Your requested indices are wack, or you have missing cm1hdf5 files, goodbye!\n");
	}
	if(gd->X0<gd->saved_X0)
	{
		printf("Oops: requested out of box: Adjusting X0 (%i) to saved_X0 (%i)\n",gd->X0,gd->saved_X0);
		gd->X0=gd->saved_X0;
	}
	if(gd->Y0<gd->saved_Y0)
	{
		printf("Oops: requested out of box: Adjusting Y0 (%i) to saved_Y0 (%i)\n",gd->Y0,gd->saved_Y0);
		gd->Y0=gd->saved_Y0;
	}
	if(gd->X1>gd->saved_X1)
	{
		printf("Oops: requested out of box: Adjusting X1 (%i) to saved_X1 (%i)\n",gd->X1,gd->saved_X1);
		gd->X1=gd->saved_X1;
	}
	if(gd->Y1>gd->saved_Y1)
	{
		printf("Oops: requested out of box: Adjusting Y1 (%i) to saved_Y1 (%i)\n",gd->Y1,gd->saved_Y1);
		gd->Y1=gd->saved_Y1;
	}
}

void allocate_1d_arrays(hdf_meta hm, grid gd, mesh *msh, sounding *snd) {

	msh->xhfull = (float *)malloc(hm.nx * sizeof(float));
	msh->yhfull = (float *)malloc(hm.ny * sizeof(float));
	msh->xffull = (float *)malloc((hm.nx+1) * sizeof(float));
	msh->yffull = (float *)malloc((hm.ny+1) * sizeof(float));
	msh->zh = (float *)malloc(hm.nz * sizeof(float));
	msh->zf = (float *)malloc(hm.nz * sizeof(float));

	msh->xhout = (float *)malloc(gd.NX * sizeof(float));
	msh->yhout = (float *)malloc(gd.NY * sizeof(float));
	msh->zhout = (float *)malloc(gd.NZ * sizeof(float));

	msh->xfout = (float *)malloc(gd.NX * sizeof(float));
	msh->yfout = (float *)malloc(gd.NY * sizeof(float));
	msh->zfout = (float *)malloc(gd.NZ * sizeof(float));

	msh->uh = (float *)malloc((gd.NX) * sizeof(float));
	msh->uf = (float *)malloc((gd.NX+1) * sizeof(float));
	msh->vh = (float *)malloc((gd.NY) * sizeof(float));
	msh->vf = (float *)malloc((gd.NY+1) * sizeof(float));
	msh->mh = (float *)malloc((gd.NZ) * sizeof(float));
	msh->mf = (float *)malloc((gd.NZ) * sizeof(float));


	snd->th0 = (float *)malloc(gd.NZ * sizeof(float));
	snd->qv0 = (float *)malloc(gd.NZ * sizeof(float));
	snd->u0 = (float *)malloc(gd.NZ * sizeof(float));
	snd->v0 = (float *)malloc(gd.NZ * sizeof(float));
	snd->pres0 = (float *)malloc(gd.NZ * sizeof(float));
	snd->pi0 = (float *)malloc(gd.NZ * sizeof(float));
	snd->rho0 = (float *)malloc(gd.NZ * sizeof(float));
}

void set_1d_arrays(hdf_meta hm, grid gd, mesh *msh, sounding *snd, hid_t *f_id)
{
	int ix,iy,iz,k;

	get0dfloat (*f_id,(char *)"mesh/dx",&msh->dx); msh->rdx=1.0/msh->dx;
	get0dfloat (*f_id,(char *)"mesh/dy",&msh->dy); msh->rdy=1.0/msh->dy;
	get0dfloat (*f_id,(char *)"mesh/dz",&msh->dz); msh->rdz=1.0/msh->dz;
	get0dfloat (*f_id,(char *)"mesh/umove",&msh->umove);
	get0dfloat (*f_id,(char *)"mesh/vmove",&msh->vmove);
	get1dfloat (*f_id,(char *)"mesh/xhfull",msh->xhfull,0,hm.nx);
	get1dfloat (*f_id,(char *)"mesh/yhfull",msh->yhfull,0,hm.ny);
	get1dfloat (*f_id,(char *)"mesh/xffull",msh->xffull,0,hm.nx+1);
	get1dfloat (*f_id,(char *)"mesh/yffull",msh->yffull,0,hm.ny+1);
	get1dfloat (*f_id,(char *)"mesh/zh",msh->zh,0,hm.nz);
	get1dfloat (*f_id,(char *)"mesh/zf",msh->zf,0,hm.nz);
	get1dfloat (*f_id,(char *)"basestate/qv0",snd->qv0,gd.Z0,gd.NZ);
	//ASSUMES Z0=0!!
	for (k=gd.Z0; k<gd.NZ; k++) snd->qv0[k-gd.Z0] *= 1000.0; // g/kg now
	get1dfloat (*f_id,(char *)"basestate/th0",snd->th0,gd.Z0,gd.NZ);
	get1dfloat (*f_id,(char *)"basestate/u0",snd->u0,gd.Z0,gd.NZ);
	get1dfloat (*f_id,(char *)"basestate/v0",snd->v0,gd.Z0,gd.NZ);
	get1dfloat (*f_id,(char *)"basestate/pres0",snd->pres0,gd.Z0,gd.NZ);
	get1dfloat (*f_id,(char *)"basestate/rh0",snd->rho0,gd.Z0,gd.NZ);//NOTE: rh0 is kind of a typo, should be rho0; rh means rel. hum. in CM1
	get1dfloat (*f_id,(char *)"basestate/pi0",snd->pi0,gd.Z0,gd.NZ);

// We recreate George's mesh/derivative calculation paradigm even though
// we are usually isotropic. We need to have our code here match what
// CM1 does internally for stretched and isotropic meshes.
//
// Becuase C cannot do have negative array indices (i.e., uh[-1]) like
// F90 can, we have to offset everything to keep the same CM1-like code
// We malloc enough space for the "ghost zones" and then make sure we
// offset by the correct amount on each side. The macros take care of
// the offsetting.

	// Carefully consider the UH,UF etc. macros and make sure they are appropriate for where you are in
	// the code (should be in pointer land)
	//

	for (ix=gd.X0; ix<=gd.X1; ix++) UHp(ix-gd.X0) = msh->dx/(msh->xffull[ix+1]-msh->xffull[ix]);
	for (ix=gd.X0; ix<=gd.X1+1; ix++) UFp(ix-gd.X0) = msh->dx/(msh->xhfull[ix]-msh->xhfull[ix-1]);
	for (iy=gd.Y0; iy<=gd.Y1; iy++) VHp(iy-gd.Y0) = msh->dy/(msh->yffull[iy+1]-msh->yffull[iy]);
	for (iy=gd.Y0; iy<=gd.Y1+1; iy++) VFp(iy-gd.Y0) = msh->dy/(msh->yhfull[iy]-msh->yhfull[iy-1]);
	for (iz=gd.Z0; iz<=gd.Z1; iz++) MHp(iz-gd.Z0) = msh->dz/(msh->zf[iz+1]-msh->zf[iz]);
	// the iz-1 will index to -1 if we don't start from iz=1
	// this is how things are set in CM1 Param.F line 6874
	// ORF this only makes sense when Z0=0 for real
	if(gd.Z0==0)
	{
		for (iz=gd.Z0+1; iz<=gd.Z1; iz++) MFp(iz-gd.Z0) = msh->dz/(msh->zh[iz]-msh->zf[iz-1]);
		MFp(0) = MFp(1);
	}
	else
		for (iz=gd.Z0; iz<=gd.Z1; iz++) MFp(iz-gd.Z0) = msh->dz/(msh->zh[iz]-msh->zf[iz-1]);
	
	/* I prefer mesh values in km in netCDF files */
	/* Note mixing ratios are g/kg in netCDF files as well */
	for (iz=gd.Z0; iz<=gd.Z1; iz++) msh->zfout[iz-gd.Z0] = 0.001*msh->zf[iz]; 
	for (iz=gd.Z0; iz<=gd.Z1; iz++) msh->zhout[iz-gd.Z0] = 0.001*msh->zh[iz];
	for (iy=gd.Y0; iy<=gd.Y1; iy++) msh->yfout[iy-gd.Y0] = 0.001*msh->yffull[iy];
	for (ix=gd.X0; ix<=gd.X1; ix++) msh->xfout[ix-gd.X0] = 0.001*msh->xffull[ix];
	for (iy=gd.Y0; iy<=gd.Y1; iy++) msh->yhout[iy-gd.Y0] = 0.001*msh->yhfull[iy];
	for (ix=gd.X0; ix<=gd.X1; ix++) msh->xhout[ix-gd.X0] = 0.001*msh->xhfull[ix];
}


void set_nc_meta(int ncid, int varnameid, char *namestandard, char *name, char *units)
{
	int len,status;

	len=strlen(name);
	status = nc_put_att_text(ncid, varnameid,namestandard,len,name);
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");

	len=strlen(units);
	status = nc_put_att_text(ncid,varnameid, "units",len,units);
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
}

// We need these global variables only for the H5Giter function 
int n2d;
const char **twodvarname;
int *twodvarid;
int ncid_g;
int d2[3];

herr_t twod_first_pass_hdf2nc(hid_t loc_id, const char *name, void *opdata)
{
    H5G_stat_t statbuf;
    H5Gget_objinfo(loc_id, name, FALSE, &statbuf);
    n2d++;
    return 0;
}

herr_t twod_second_pass_hdf2nc(hid_t loc_id, const char *name, void *opdata)
{
    H5G_stat_t statbuf;
    H5Gget_objinfo(loc_id, name, FALSE, &statbuf);
    strcpy((char *)twodvarname[n2d],name);
    nc_def_var (ncid_g, twodvarname[n2d], NC_FLOAT, 3, d2, &(twodvarid[n2d]));
    n2d++;
    return 0;
}

void set_netcdf_attributes(ncstruct *nc, grid gd, cmdline *cmd, buffers *b, hdf_meta *hm, hid_t *f_id)
{
	int status;
	int nv;
	int ivar;
	char var[MAXSTR];

	status = nc_def_dim (nc->ncid, "xh", gd.NX, &nc->nxh_dimid); if (status != NC_NOERR) ERROR_STOP("nc_def_dim failed"); 
	status = nc_def_dim (nc->ncid, "yh", gd.NY, &nc->nyh_dimid); if (status != NC_NOERR) ERROR_STOP("nc_def_dim failed");
	status = nc_def_dim (nc->ncid, "zh", gd.NZ, &nc->nzh_dimid); if (status != NC_NOERR) ERROR_STOP("nc_def_dim failed");
	//ORF shave off one point (was NX+1 etc.)
	status = nc_def_dim (nc->ncid, "xf", gd.NX, &nc->nxf_dimid); if (status != NC_NOERR) ERROR_STOP("nc_def_dim failed"); 
	status = nc_def_dim (nc->ncid, "yf", gd.NY, &nc->nyf_dimid); if (status != NC_NOERR) ERROR_STOP("nc_def_dim failed");
	status = nc_def_dim (nc->ncid, "zf", gd.NZ, &nc->nzf_dimid); if (status != NC_NOERR) ERROR_STOP("nc_def_dim failed");
	status = nc_def_dim (nc->ncid, "time", NC_UNLIMITED, &nc->time_dimid); if (status != NC_NOERR) ERROR_STOP("nc_def_dim failed");
	status = nc_def_var (nc->ncid, "xh", NC_FLOAT, 1, &nc->nxh_dimid, &nc->xhid); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "yh", NC_FLOAT, 1, &nc->nyh_dimid, &nc->yhid); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "zh", NC_FLOAT, 1, &nc->nzh_dimid, &nc->zhid); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "xf", NC_FLOAT, 1, &nc->nxf_dimid, &nc->xfid); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "yf", NC_FLOAT, 1, &nc->nyf_dimid, &nc->yfid); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "zf", NC_FLOAT, 1, &nc->nzf_dimid, &nc->zfid); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "time", NC_DOUBLE, 1, &nc->time_dimid, &nc->timeid); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_put_att_text(nc->ncid, nc->xhid, "long_name", strlen("x-coordinate in Cartesian system"), "x-coordinate in Cartesian system");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->xhid, "units", strlen("km"), "km");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->xhid, "axis", strlen("X"), "X");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->yhid, "long_name", strlen("y-coordinate in Cartesian system"), "y-coordinate in Cartesian system");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->yhid, "units", strlen("km"), "km");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->yhid, "axis", strlen("Y"), "Y");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->zhid, "long_name", strlen("z-coordinate in Cartesian system"), "z-coordinate in Cartesian system");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->zhid, "units", strlen("km"), "km");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->zhid, "axis", strlen("Z"), "Z");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->xfid, "units", strlen("km"), "km");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->yfid, "units", strlen("km"), "km");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->zfid, "units", strlen("km"), "km");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->timeid, "units", strlen("s"), "s");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->timeid, "axis", strlen("T"), "T");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->timeid, "long_name", strlen("time"), "time");if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_def_var (nc->ncid, "X0", NC_INT, 0, nc->dims, &nc->x0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "Y0", NC_INT, 0, nc->dims, &nc->y0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "X1", NC_INT, 0, nc->dims, &nc->x1id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "Y1", NC_INT, 0, nc->dims, &nc->y1id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "Z0", NC_INT, 0, nc->dims, &nc->z0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_def_var (nc->ncid, "Z1", NC_INT, 0, nc->dims, &nc->z1id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	status = nc_put_att_text(nc->ncid, nc->x0id, "long_name", strlen("westmost grid index from LOFS data"), "westmost grid index from LOFS data");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->x1id, "long_name", strlen("eastmost grid index from LOFS data"), "eastmost grid index from LOFS data");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->y0id, "long_name", strlen("southmost grid index from LOFS data"), "southmost grid index from LOFS data");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->y1id, "long_name", strlen("northmost grid index from LOFS data"), "northmost grid index from LOFS data");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->z0id, "long_name", strlen("bottom grid index from LOFS data"), "bottom grid index from LOFS data");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");
	status = nc_put_att_text(nc->ncid, nc->z1id, "long_name", strlen("top grid index from LOFS data"), "top grid index from LOFS data");
	if (status != NC_NOERR) ERROR_STOP("nc_put_att_text failed");

	nc->d2[0] = nc->time_dimid;
	nc->d2[1] = nc->nyh_dimid;
	nc->d2[2] = nc->nxh_dimid;

	//d2 global (to this file) routine for H5Giter
	d2[0]=nc->d2[0];
	d2[1]=nc->d2[1];
	d2[2]=nc->d2[2];

	if (cmd->do_swaths)
	{
		int i2d,bufsize;

		n2d = 0;
		H5Giterate(*f_id, "/00000/2D/static",NULL,twod_first_pass_hdf2nc,NULL);
		H5Giterate(*f_id, "/00000/2D/swath",NULL,twod_first_pass_hdf2nc,NULL);

		twodvarname = (const char **)malloc(n2d*sizeof(char *));
		twodvarid =   (int *)        malloc(n2d*sizeof(int));

		hm->n2dswaths = n2d;
		printf("There are %i 2D static/swath fields.\n",n2d);

		for (i2d=0; i2d<n2d; i2d++)
		{
			twodvarname[i2d] = (char *)malloc(50*sizeof(char)); // 50 characters per variable
		}

		n2d = 0;
		ncid_g=nc->ncid; //Needed for iteration func.
		H5Giterate(*f_id, "/00000/2D/static",NULL,twod_second_pass_hdf2nc,NULL);
		H5Giterate(*f_id, "/00000/2D/swath",NULL,twod_second_pass_hdf2nc,NULL);

		for (i2d=0; i2d<n2d; i2d++) free((void *)twodvarname[i2d]); //shaddap compiler
		free(twodvarname);

		/* And, like magic, we have populated our netcdf id arrays for all the swath slices */
	}


//OK we are going for simple here. We create a single varname
//array that contains all available variables, plus any
//additional requested variables. We then check for duplicates
//and remove them - this alphabetizes the order of the variables,
//however, which could be annoying and/or useful

	if (!cmd->do_allvars)
	{
		cmd->nvar = cmd->nvar_cmdline;
		nv=0; /* liar! */
	}
	else nv=hm->nvar_available;
//With faking nvar_available to zero this loop now sorts/uniqs
//all possible variables lists (just command line, just allvars,
//or a combination of the two)
	if (cmd->nvar !=0||cmd->do_allvars) 
	{
		char **varname_tmp;
		int ndupes = 0;
		int i,j;

		varname_tmp = (char **)malloc(MAXVARIABLES * sizeof(char *));
		for (i=0; i < MAXVARIABLES; i++) varname_tmp[i] = (char *)(malloc(MAXSTR * sizeof(char)));

		for (i=0; i<nv; i++)
		{
			strcpy(varname_tmp[i],hm->varname_available[i]);
		}
		for (i=0;i<cmd->nvar_cmdline; i++)
		{
			strcpy(varname_tmp[i+nv],cmd->varname_cmdline[i]);
		}

		sortchararray (varname_tmp,nv+cmd->nvar_cmdline);

		strcpy(nc->varname[0],varname_tmp[0]);
		j=1;
		/* Get rid of accidental duplicates */
		for (i=0; i<nv+cmd->nvar_cmdline-1; i++)
		{
			if(strcmp(varname_tmp[i],varname_tmp[i+1]))
			{
				strcpy(nc->varname[j],varname_tmp[i+1]); //THIS IS WHERE VARNAME IS SET!
				j++;
			}
		}
		cmd->nvar = j;
		ndupes = nv+cmd->nvar_cmdline-cmd->nvar;
		if(ndupes!=0)printf("We got rid of %i duplicate requested variables\n",ndupes);
		for (i=0; i < MAXVARIABLES; i++) free(varname_tmp[i]);
		free(varname_tmp);
	}

// Now that we have nvar, allocate the netcdf varnameid array

	nc->varnameid        = (int *) malloc(cmd->nvar*sizeof(int));

// This is our main "loop over all requested variable names" loop that
// sets all the metadata shit.

	for (ivar = 0; ivar < cmd->nvar; ivar++)
	{
		int var_is_u = 0, var_is_v = 0, var_is_w = 0;

		strcpy(var,nc->varname[ivar]);

		if (same(var,"u")) var_is_u=1;
		if (same(var,"v")) var_is_v=1;
		if (same(var,"w")) var_is_w=1;

		/* u v and w live on their own mesh (Arakawa C grid)*/

		/* Recommend, however, for making netcdf files, to just
		 * request uinterp vinterp winterp which NOW are cacluated
		 * HERE rather than saved in LOFS */

		/* We are going to preserve u v w with the extra point for
		 * saving u v and w easily while facilitating averaging
		 * which requires 1 fewer point */


		/* ORF 2020-04-16
		 * NOTE start/edges that are set here are ignored. I've changed over to writing
		 * netCDF files in Z slices, and set the appropirate start and edges values in
		 * do_requested_variables. However the dimid stuff must still be done here and is
		 * used */

		if(var_is_u)
		{
			nc->dims[0] = nc->time_dimid;
			nc->dims[1] = nc->nzh_dimid;
			nc->dims[2] = nc->nyh_dimid;
			nc->dims[3] = nc->nxf_dimid;

			nc->start[0] = 0;
			nc->start[1] = 0;
			nc->start[2] = 0;
			nc->start[3] = 0;

			nc->edges[0] = 1;
			nc->edges[1] = gd.NZ;
			nc->edges[2] = gd.NY;
			nc->edges[3] = gd.NX;
		}
		else if (var_is_v)
		{
			nc->dims[0] = nc->time_dimid;
			nc->dims[1] = nc->nzh_dimid;
			nc->dims[2] = nc->nyf_dimid;
			nc->dims[3] = nc->nxh_dimid;

			nc->start[0] = 0;
			nc->start[1] = 0;
			nc->start[2] = 0;
			nc->start[3] = 0;

			nc->edges[0] = 1;
			nc->edges[1] = gd.NZ;
			nc->edges[2] = gd.NY;
			nc->edges[3] = gd.NX;
		}
		else if (var_is_w)
		{
			nc->dims[0] = nc->time_dimid;
			nc->dims[1] = nc->nzf_dimid;
			nc->dims[2] = nc->nyh_dimid;
			nc->dims[3] = nc->nxh_dimid;

			nc->start[0] = 0;
			nc->start[1] = 0;
			nc->start[2] = 0;
			nc->start[3] = 0;

			nc->edges[0] = 1;
			nc->edges[1] = gd.NZ;
			nc->edges[2] = gd.NY;
			nc->edges[3] = gd.NX;
		}
		else /* scalar grid */
		{
			nc->dims[0] = nc->time_dimid;
			nc->dims[1] = nc->nzh_dimid;
			nc->dims[2] = nc->nyh_dimid;
			nc->dims[3] = nc->nxh_dimid;

			nc->start[0] = 0;
			nc->start[1] = 0;
			nc->start[2] = 0;
			nc->start[3] = 0;

			nc->edges[0] = 1;
			nc->edges[1] = gd.NZ;
			nc->edges[2] = gd.NY;
			nc->edges[3] = gd.NX;
		}

// If we request a 2D slice set parameters accordingly

		if(gd.X0==gd.X1)
		{
			nc->twodslice = TRUE;
			nc->dims[0] = nc->time_dimid;
			nc->dims[1] = (var_is_w)?nc->nzf_dimid:nc->nzh_dimid;
			nc->dims[2] = (var_is_v)?nc->nyf_dimid:nc->nyh_dimid;
			nc->start[0] = 0;
			nc->start[1] = 0;
			nc->start[2] = 0;
			nc->edges[0] = 1;
			nc->edges[1] = gd.NZ;
			nc->edges[2] = gd.NY;
			status = nc_def_var (nc->ncid, nc->varname[ivar], NC_FLOAT, 3, nc->dims, &(nc->varnameid[ivar]));
		}
		else if(gd.Y0==gd.Y1)
		{
			nc->twodslice = TRUE;
			nc->dims[0] = nc->time_dimid;
			nc->dims[1] = (var_is_w)?nc->nzf_dimid:nc->nzh_dimid;
			nc->dims[2] = (var_is_u)?nc->nxf_dimid:nc->nxh_dimid;
			nc->start[0] = 0;
			nc->start[1] = 0;
			nc->start[2] = 0;
			nc->edges[0] = 1;
			nc->edges[1] = gd.NZ;
			nc->edges[2] = gd.NX;
			status = nc_def_var (nc->ncid, nc->varname[ivar], NC_FLOAT, 3, nc->dims, &(nc->varnameid[ivar]));
		}
		else if(gd.Z0==gd.Z1)
		{
			nc->twodslice = TRUE;
			nc->dims[0] = nc->time_dimid;
			nc->dims[1] = (var_is_v)?nc->nyf_dimid:nc->nyh_dimid;
			nc->dims[2] = (var_is_u)?nc->nxf_dimid:nc->nxh_dimid;
			nc->start[0] = 0;
			nc->start[1] = 0;
			nc->start[2] = 0;
			nc->edges[0] = 1;
			nc->edges[1] = gd.NY;
			nc->edges[2] = gd.NX;
			status =  nc_def_var (nc->ncid, nc->varname[ivar], NC_FLOAT, 3, nc->dims, &nc->varnameid[ivar]);
		}
		else status = nc_def_var (nc->ncid, nc->varname[ivar], NC_FLOAT, 4, nc->dims, &nc->varnameid[ivar]);

		if (status != NC_NOERR) 
		{
			printf ("Cannot nc_def_var for var #%i %s, status = %i, message = %s\n", ivar, nc->varname[ivar],status,nc_strerror(status));
			ERROR_STOP("nc_def_var failed");
		}

// We are still before the nc_enddef call, in case you are lost

		if(same(var,"uinterp"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","eastward_wind","m/s");
		else if(same(var,"vinterp"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","northward_wind","m/s");
		else if(same(var,"winterp"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","upward_wind","m/s");
		else if(same(var,"prespert"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","pressure_perturbation","hPa");
		else if(same(var,"thpert"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","potential_temperature_perturbation","K");
		else if(same(var,"rhopert"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","density_perturbation","kg/m^3");
		else if(same(var,"khh"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","horizontal_subgrid_eddy_scalar_diffusivity","m^2/s");
		else if(same(var,"khv"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","vertical_subgrid_eddy_scalar_diffusivity","m^2/s");
		else if(same(var,"kmh"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","horizontal_subgrid_eddy_momentum_viscosity","m^2/s");
		else if(same(var,"kmv"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","vertical_subgrid_eddy_momentum_viscosity","m^2/s");
		else if(same(var,"xvort"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","x_vorticity","s^-1");
		else if(same(var,"yvort"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","y_vorticity","s^-1");
		else if(same(var,"zvort"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","z_vorticity","s^-1");
		else if(same(var,"vortmag"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","vorticity_magnitude","s^-1");
		else if(same(var,"hvort"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","horizontal_vorticity_magnitude","s^-1");
		else if(same(var,"streamvort"))set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","streamwise_vorticity","s^-1");
		else if(same(var,"dbz"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","radar_reflectivity_simulated","dBZ");
		else if(same(var,"qvpert"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","water_vapor_perturbation_mixing_ratio","cm^-3");
		else if(same(var,"qc"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","cloud_water_mixing_ratio","g/kg");
		else if(same(var,"qr"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","rain_water_mixing_ratio","g/kg");
		else if(same(var,"qi"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","cloud_ice_mixing_ratio","g/kg");
		else if(same(var,"qs"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","now_mixing_ratio","g/kg");
		else if(same(var,"qg"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","hail_mixing_ratio","g/kg");
		else if(same(var,"nci"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","ice_number_concenctration","cm^-3");
		else if(same(var,"ncr"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","rain_number_concenctration","cm^-3");
		else if(same(var,"ncs"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","snow_number_concenctration","cm^-3");
		else if(same(var,"ncg"))		set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","hail_number_concenctration","cm^-3");
		else if(same(var,"hwin_sr"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","storm_relative_horizontal_wind_speed","m/s");
		else if(same(var,"windmag_sr"))set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","storm_relative_wind_speed","m/s");
		else if(same(var,"hwin_gr"))	set_nc_meta(nc->ncid,nc->varnameid[ivar],"standard_name","ground_relative_horizontal_wind_speed","m/s");

		/* ORF 2020-04-16
		 * After switching to writing data in Z slices, it appears the gzip compression
		 * performance got much worse, even compared to the already bad performance of
		 * doing 3D writes. Experimentation with the netcdf external program nccopy has
		 * revealed much better performance (time wise) with the added benefit that it
		 * compresses all variables including 2D. So from now on, if you pass --compress,
		 * it will compress the whole netCDF file usging nccopy. Passing the verbose flag
		 * will give compresion ratio information. Since netcdf is required for hdf2nc,
		 * nccopy must have been built and it must be in your path for this to work. */

//	NO MORE CRAPPY PERFORMANCE
//	if (cmd->gzip>0) status=nc_def_var_deflate(nc->ncid, nc->varnameid[ivar], 1, 1, cmd->gzip); //shuffle=1,deflate=1,deflate_level=cmd->gzip value
	}

/* Write sounding data */

	status = nc_def_var (nc->ncid, "u0", NC_FLOAT, 1, &nc->nzh_dimid, &nc->u0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	set_nc_meta(nc->ncid,nc->u0id,"standard_name","base_state_u","m/s");
	status = nc_def_var (nc->ncid, "v0", NC_FLOAT, 1, &nc->nzh_dimid, &nc->v0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	set_nc_meta(nc->ncid,nc->v0id,"standard_name","base_state_v","m/s");
	status = nc_def_var (nc->ncid, "th0", NC_FLOAT, 1, &nc->nzh_dimid, &nc->th0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	set_nc_meta(nc->ncid,nc->th0id,"standard_name","base_state_theta","m/s");
	status = nc_def_var (nc->ncid, "pres0", NC_FLOAT, 1, &nc->nzh_dimid, &nc->pres0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	set_nc_meta(nc->ncid,nc->pres0id,"standard_name","base_state_pressure","m/s");
	status = nc_def_var (nc->ncid, "pi0", NC_FLOAT, 1, &nc->nzh_dimid, &nc->pi0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	set_nc_meta(nc->ncid,nc->pi0id,"standard_name","base_state_pi","m/s");
	status = nc_def_var (nc->ncid, "qv0", NC_FLOAT, 1, &nc->nzh_dimid, &nc->qv0id); if (status != NC_NOERR) ERROR_STOP("nc_def_var failed");
	set_nc_meta(nc->ncid,nc->pi0id,"standard_name","base_state_qv","m/s");
}

void nc_write_1d_data (ncstruct nc, grid gd, mesh msh, sounding snd, cmdline cmd)
{
	double timearray[1];
	//ORF for writing single time in unlimited time dimension/variable
	const size_t timestart = 0;
	const size_t timecount = 1;
	int status;

	status = nc_put_var_float (nc.ncid,nc.xhid,msh.xhout); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.yhid,msh.yhout); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.zhid,msh.zhout); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.xfid,msh.xfout); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.yfid,msh.yfout); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.zfid,msh.zfout); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.u0id,snd.u0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.v0id,snd.v0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.th0id,snd.th0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.pres0id,snd.pres0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.pi0id,snd.pi0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_float (nc.ncid,nc.qv0id,snd.qv0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_float failed");
	status = nc_put_var_int (nc.ncid,nc.x0id,&gd.X0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_int failed");
	status = nc_put_var_int (nc.ncid,nc.y0id,&gd.Y0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_int failed");
	status = nc_put_var_int (nc.ncid,nc.x1id,&gd.X1); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_int failed");
	status = nc_put_var_int (nc.ncid,nc.y1id,&gd.Y1); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_int failed");
	status = nc_put_var_int (nc.ncid,nc.z0id,&gd.Z0); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_int failed");
	status = nc_put_var_int (nc.ncid,nc.z1id,&gd.Z1); if (status != NC_NOERR) ERROR_STOP ("nc_put_var_int failed");
	timearray[0] = cmd.time;
	status = nc_put_vara_double (nc.ncid,nc.timeid,&timestart,&timecount,timearray);
	if (status != NC_NOERR) ERROR_STOP ("nc_put_var_int failed");
}

void set_readahead(readahead *rh,ncstruct nc, cmdline cmd)
{
	int ivar;
	char *var;

	var = (char *) malloc (MAXSTR * sizeof (char));

	for (ivar = 0; ivar < cmd.nvar; ivar++)
	{
		var=nc.varname[ivar];

		if(same(var,"uinterp")) {rh->u=1;}
		if(same(var,"vinterp")) {rh->v=1;}
		if(same(var,"winterp")) {rh->w=1;}
		if(same(var,"hwin_sr")) {rh->u=1;rh->v=1;}
		if(same(var,"hwin_gr")) {rh->u=1;rh->v=1;}
		if(same(var,"windmag_sr")) {rh->u=1;rh->v=1;rh->w=1;}
		if(same(var,"hdiv")) {rh->u=1;rh->v=1;}
		if(same(var,"xvort")) {rh->v=1;rh->w=1;}
		if(same(var,"yvort")) {rh->u=1;rh->w=1;}
		if(same(var,"zvort")) {rh->u=1;rh->v=1;}
		if(same(var,"hvort")) {rh->u=1;rh->v=1;rh->w=1;rh->hvort=1;}
		if(same(var,"vortmag")) {rh->u=1;rh->v=1;rh->w=1;rh->vortmag=1;}
		if(same(var,"streamvort")) {rh->u=1;rh->v=1;rh->w=1;rh->streamvort=1;}
	}
	//free(var);
}

void malloc_3D_arrays (buffers *b, grid gd, readahead rh,cmdline cmd)
{
	if (cmd.nvar>0)
	{
		long bufsize,totbufsize;

		bufsize = (long) (gd.NX+2) * (long) (gd.NY+2) * (long) (gd.NZ+1) * (long) sizeof(float);
		totbufsize = bufsize;

		if ((b->buf0 = b->buf = (float *) malloc ((size_t)bufsize)) == NULL)
			ERROR_STOP("Cannot allocate our 3D variable buffer array");
		if (rh.u)
		{
			if ((b->ustag = (float *) malloc ((size_t)bufsize)) == NULL)
				ERROR_STOP("Cannot allocate our ustag buffer array");
			totbufsize+=bufsize;
		}
		if (rh.v)
		{
			if ((b->vstag = (float *) malloc ((size_t)bufsize)) == NULL)
				ERROR_STOP("Cannot allocate our vstag buffer array");
			totbufsize+=bufsize;
		}
		if (rh.w)
		{
			if ((b->wstag = (float *) malloc ((size_t)bufsize)) == NULL)
				ERROR_STOP("Cannot allocate our wstag buffer array");
			totbufsize+=bufsize;
		}
		if (rh.u||rh.v||rh.w)
		{
			if ((b->dum00 = b->dum0 = (float *) malloc ((size_t)bufsize)) == NULL)
				ERROR_STOP("Cannot allocate our first 3D temp calculation array");
			totbufsize+=bufsize;
		}
		if (rh.vortmag||rh.hvort||rh.streamvort)//Not really readahead, but if we calculated these we need another array
		{
			if ((b->dum10 = b->dum1 = (float *) malloc ((size_t)bufsize)) == NULL)
				ERROR_STOP("Cannot allocate our second 3D temp calculation array");
			totbufsize+=bufsize;
		}
		printf("We allocated a total of %6.2f GB of memory for floating point buffers\n",1.0e-9*totbufsize);
	}
}

void free_3D_arrays (buffers *b, grid gd, readahead rh,cmdline cmd)
{
	if (cmd.nvar>0)
	{
		free (b->buf);
		if (rh.u) free (b->ustag);
		if (rh.v) free (b->vstag);
		if (rh.w) free (b->wstag);
		if (rh.u||rh.v||rh.w) free (b->dum0);
		if (rh.vortmag||rh.hvort||rh.streamvort)free(b->dum1);
	}
}

void do_the_swaths(hdf_meta hm, ncstruct nc, dir_meta dm, grid gd, cmdline cmd)
{
	int i2d,ix,iy,status;
	float *twodfield,*swathbuf,*writeptr;
	float bufsize;
	requested_cube rc;
	size_t s2[3] = {0,0,0};
	size_t e2[3] = {1,gd.NY,gd.NX};

	copy_grid_to_requested_cube(&rc,gd);

	printf("swaths: writing...");FL; 

	bufsize = (long) (rc.NX) * (long) (rc.NY) * (long) sizeof(float);
	if ((twodfield = (float *) malloc ((size_t)bufsize)) == NULL)
		ERROR_STOP("Cannot allocate our 2D swath buffer array");
	bufsize = (long) (rc.NX) * (long) (rc.NY) * (long) (hm.n2dswaths) * (long) sizeof(float);
	if ((swathbuf = (float *) malloc ((size_t)bufsize)) == NULL)
		ERROR_STOP("Cannot allocate our 3D swaths buffer array");

	read_lofs_buffer(swathbuf,"swaths",dm,hm,rc,cmd);

	if(cmd.verbose)printf("Writing swaths...\n");
	for (i2d=0;i2d<hm.n2dswaths;i2d++)
	{
		for (iy=0; iy<rc.NY; iy++)
			for (ix=0; ix<rc.NX; ix++)
				twodfield[P2(ix,iy,rc.NX)] = swathbuf[P3(ix,iy,i2d,rc.NX,rc.NY)];
		writeptr = twodfield;
		status = nc_put_vara_float (nc.ncid, twodvarid[i2d], s2, e2, writeptr);
	}
	free(swathbuf);
	free(twodfield);
	BL;
}

void do_readahead(buffers *b,grid gd,readahead rh,dir_meta dm,hdf_meta hm,cmdline cmd)
{
	requested_cube rc;

	/* We select extra data for doing spatial averaging */
	/* By shrinking in saved_x0,saved_x1 etc by 1 point on either side (see set_span), this will not fail
	 * if we do not specify X0, X1 etc.*/

	if (rh.u)
	{
		rc.X0=gd.X0-1; rc.Y0=gd.Y0-1; rc.Z0=gd.Z0;
		rc.X1=gd.X1+1; rc.Y1=gd.Y1+1; rc.Z1=gd.Z1;
		rc.NX=gd.X1-gd.X0+1; rc.NY=gd.Y1-gd.Y0+1; rc.NZ=gd.Z1-gd.Z0+1;
		read_lofs_buffer(b->ustag,"u",dm,hm,rc,cmd);
	}
	if (rh.v)
	{
		rc.X0=gd.X0-1; rc.Y0=gd.Y0-1; rc.Z0=gd.Z0;
		rc.X1=gd.X1+1; rc.Y1=gd.Y1+1; rc.Z1=gd.Z1;
		rc.NX=gd.X1-gd.X0+1; rc.NY=gd.Y1-gd.Y0+1; rc.NZ=gd.Z1-gd.Z0+1;
		read_lofs_buffer(b->vstag,"v",dm,hm,rc,cmd);
	}
	if (rh.w)
	{
		rc.X0=gd.X0-1; rc.Y0=gd.Y0-1; rc.Z0=gd.Z0;
		rc.X1=gd.X1+1; rc.Y1=gd.Y1+1; rc.Z1=gd.Z1+1;
		rc.NX=gd.X1-gd.X0+1; rc.NY=gd.Y1-gd.Y0+1; rc.NZ=gd.Z1-gd.Z0+1;
		read_lofs_buffer(b->wstag,"w",dm,hm,rc,cmd);
	}

}

// It is much, much faster to do this externally and also has the benefit of compressing all
// arrays, not just the 3D ones. So for the best performance, just make sure nccopy is in
// your path!
//
// TODO: return value checks, check for nccopy in path
void compress_with_nccopy(ncstruct nc,cmdline cmd)
{
	char strbuf[MAXSTR];
	off_t unc_fsize,comp_fsize;
	float ratio;
	struct stat st;
	int retval;

	printf("compressing...\n");fflush(stdout);
	sprintf(strbuf,"mv %s %s.uncompressed",nc.ncfilename,nc.ncfilename);
	retval=system(strbuf);
	if(retval!=0)
	{
		fprintf(stderr,"Command: %s ...failed!\n",strbuf);
		return;
	}
	sprintf(strbuf,"nccopy -d9 -s %s.uncompressed %s",nc.ncfilename,nc.ncfilename);
	if(cmd.verbose)printf("Calling external compression program: executing: %s ...\n",strbuf);fflush(stdout);
	retval=system(strbuf); //we have compressed the file. Get file savings info.
	if(retval!=0)
	{
		fprintf(stderr,"Command: %s ...failed!\n",strbuf);
		//nccopy failed so move file name back to .nc, warn and bail
		sprintf(strbuf,"mv %s.uncompressed %s",nc.ncfilename,nc.ncfilename);
		retval=system(strbuf);
		fprintf(stderr,"%s will not be compressed.\n",nc.ncfilename);
		return;
	}

	sprintf(strbuf,"%s.uncompressed",nc.ncfilename);
	stat(strbuf,&st);
	unc_fsize=st.st_size;

	sprintf(strbuf,"%s",nc.ncfilename);
	stat(strbuf,&st);
	comp_fsize=st.st_size;

	ratio = (float)unc_fsize/(float)comp_fsize;

	sprintf(strbuf,"\n%12i bytes %s.uncompressed\n%12i bytes %s\nFile compression ratio of %7.2f:1\n",unc_fsize,nc.ncfilename,comp_fsize,nc.ncfilename,ratio);
	printf("%s",strbuf);

	sprintf(strbuf,"rm %s.uncompressed",nc.ncfilename);
	retval=system(strbuf);
	if(retval!=0)
	{
		fprintf(stderr,"Command: %s ...failed!\n",strbuf);
		return;
	}
}
