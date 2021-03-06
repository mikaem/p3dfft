/*
! This file is part of P3DFFT library
!
!    P3DFFT
!
!    Software Framework for Scalable Fourier Transforms in Three Dimensions
!
!    Copyright (C) 2006-2010 Dmitry Pekurovsky
!    Copyright (C) 2006-2010 University of California
!
!    This program is free software: you can redistribute it and/or modify
!    it under the terms of the GNU General Public License as published by
!    the Free Software Foundation, either version 3 of the License, or
!    (at your option) any later version.
!
!    This program is distributed in the hope that it will be useful,
!    but WITHOUT ANY WARRANTY; without even the implied warranty of
!    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
!    GNU General Public License for more details.
!
!    You should have received a copy of the GNU General Public License
!    along with this program.  If not, see <http://www.gnu.org/licenses/>.
!
!
!----------------------------------------------------------------------------
*/

/*
! This sample program illustrates the
! use of P3DFFT library for highly scalable parallel 3D FFT.
!
! This program initializes a 3D array with a 3D sine wave, then
! performs forward 3D Fourier transform IN PLACE, then backward
! transform IN PLACE, and checks that
! the results are correct, namely the same as in the start except
! for a normalization factor. It can be used both as a correctness
! test and for timing the library functions.
!
! The program expects 'stdin' file in the working directory, with
! a single line of numbers : Nx,Ny,Nz,Ndim,Nrep. Here Nx,Ny,Nz
! are box dimensions, Ndim is the dimentionality of processor grid
! (1 or 2), and Nrep is the number of repititions. Optionally
! a file named 'dims' can also be provided to guide in the choice
! of processor geometry in case of 2D decomposition. It should contain
! two numbers in a line, with their product equal to the total number
! of tasks. Otherwise processor grid geometry is chosen automatically.
! For better performance, experiment with this setting, varying
! iproc and jproc. In many cases, minimizing iproc gives best results.
! Setting it to 1 corresponds to one-dimensional decomposition.
!
! If you have questions please contact Dmitry Pekurovsky, dmitry@sdsc.edu
*/

#include "p3dfft.h"
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

double FORTNAME(t1),FORTNAME(t2),FORTNAME(t3),FORTNAME(t4),FORTNAME(tp1);
/* double t1,t2,t3,t4,tp1; */

int main(int argc,char **argv)
{
#ifndef SINGLE_PREC
   double *A,*B,*p,*C;
#else
   float *A,*B,*p,*C;
#endif
   int i,j,k,x,y,z,nx,ny,nz,proc_id,nproc,dims[2],ndim,nu;
   int istart[3],isize[3],iend[3];
   int fstart[3],fsize[3],fend[3];
   int iproc,jproc,ng[3],kmax,iex,conf,m,n,nm;
   long int Ntot,Nglob;
   double pi,twopi,sinyz,cdiff,ccdiff,ans,prec;
   double *sinx,*siny,*sinz,factor;
   double rfast,rfast2,ravg,rtime1,rtime2,gt[12],gt1[12],gt2[12],timers[12];
   FILE *fp;
   unsigned char op_f[]="fft", op_b[]="tff";
   int memsize[3];

#ifndef SINGLE_PREC
   void print_all(double *,long int,int,long int),mult_array(double *,long int,double);
#else
   void print_all(float *,long int,int,long int),mult_array(float *,long int,double);
#endif

   MPI_Init(&argc,&argv);
   MPI_Comm_size(MPI_COMM_WORLD,&nproc);
   MPI_Comm_rank(MPI_COMM_WORLD,&proc_id);

   pi = atan(1.0)*4.0;
   twopi = 2.0*pi;

   for(i=0; i< 12; i++) {
     gt[i] = 0.0;
     gt1[i] = 0.0;
     gt2[i] = 1E10;
   }

   Cset_timers();

   if(proc_id == 0) {
     if((fp=fopen("stdin", "r"))==NULL){
        printf("Cannot open file. Setting to default nx=ny=nz=128, ndim=2, n=1.\n");
        nx=ny=nz=128; n=1;
     } else {
        fscanf(fp,"%d %d %d %d %d\n",&nx,&ny,&nz,&ndim,&n);
        fclose(fp);
     }
     printf("P3DFFT test, 3D wave input, in-place transform\n");
#ifndef SINGLE_PREC
     printf("Double precision\n (%d %d %d) grid\n %d proc. dimensions\n%d repetitions\n",nx,ny,nz,ndim,n);
#else
     printf("Single precision\n (%d %d %d) grid\n %d proc. dimensions\n%d repetitions\n",nx,ny,nz,ndim,n);
#endif
   }
   MPI_Bcast(&nx,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&ny,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&nz,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&n,1,MPI_INT,0,MPI_COMM_WORLD);
   MPI_Bcast(&ndim,1,MPI_INT,0,MPI_COMM_WORLD);

   if(ndim == 1) {
     dims[0] = 1; dims[1] = nproc;
   }
   else if(ndim == 2) {
     fp = fopen("dims","r");
     if(fp != NULL) {
       if(proc_id == 0)
         printf("Reading proc. grid from file dims\n");
       fscanf(fp,"%d %d\n",dims,dims+1);
       fclose(fp);
       if(dims[0]*dims[1] != nproc)
          dims[1] = nproc / dims[0];
     }
     else {
       if(proc_id == 0)
          printf("Creating proc. grid with mpi_dims_create\n");
       dims[0]=dims[1]=0;
       MPI_Dims_create(nproc,2,dims);
       if(dims[0] > dims[1]) {
          dims[0] = dims[1];
          dims[1] = nproc/dims[0];
       }
     }
   }

   if(proc_id == 0)
      printf("Using processor grid %d x %d\n",dims[0],dims[1]);

   /* Initialize P3DFFT */
   Cp3dfft_setup(dims,nx,ny,nz,MPI_Comm_c2f(MPI_COMM_WORLD),nx,ny,nz,1,memsize);
   /* Get dimensions for input array - real numbers, X-pencil shape.
      Note that we are following the Fortran ordering, i.e.
      the dimension  with stride-1 is X. */
   conf = 1;
   Cp3dfft_get_dims(istart,iend,isize,conf);
   conf = 2;
   Cp3dfft_get_dims(fstart,fend,fsize,conf);
   /* Get dimensions for output array - complex numbers, Z-pencil shape.
      Stride-1 dimension could be X or Z, depending on how the library
      was compiled (stride1 option) */

   sinx = malloc(sizeof(double)*nx);
   siny = malloc(sizeof(double)*ny);
   sinz = malloc(sizeof(double)*nz);

   for(z=0;z < isize[2];z++)
     sinz[z] = sin((z+istart[2]-1)*twopi/nz);
   for(y=0;y < isize[1];y++)
     siny[y] = sin((y+istart[1]-1)*twopi/ny);
   for(x=0;x < isize[0];x++)
     sinx[x] = sin((x+istart[0]-1)*twopi/nx);

   /* Allocate array A for both initial and final array */
   nm = isize[0]*isize[1]*isize[2];
   if(nm < fsize[0]*fsize[1]*fsize[2]*2)
     nm = fsize[0]*fsize[1]*fsize[2]*2;

#ifndef SINGLE_PREC
   A = (double *) malloc(sizeof(double) * nm);
#else
   A = (float *) malloc(sizeof(float) * nm);
#endif

   if(A == NULL)
     printf("%d: Error allocating array A (%d)\n",proc_id,nm);

   p = A;
   for(z=0;z < isize[2];z++)
     for(y=0;y < isize[1];y++) {
       sinyz = siny[y]*sinz[z];
       for(x=0;x < isize[0];x++)
          *p++ = sinx[x]*sinyz;
     }

   Ntot = fsize[0]*fsize[1];
   Ntot *= fsize[2]*2;
   Nglob = nx * ny;
   Nglob *= nz;
   factor = 1.0/Nglob;

   rtime1 = 0.0;
   rfast = 1e8;
   for(m=0;m < n;m++) {

     if(proc_id == 0)
        printf("Iteration %d\n",m);

     MPI_Barrier(MPI_COMM_WORLD);
     rtime1 = MPI_Wtime();
     /* Forward transform in-place */
     Cp3dfft_ftran_r2c(A,A,op_f);
     //rtime1 = rtime1 + MPI_Wtime();

     //if(proc_id == 0)
     //   printf("Result of forward transform\n");

     //print_all(A,Ntot,proc_id,Nglob);
     /* normallize */
     mult_array(A,Ntot,factor);

     //MPI_Barrier(MPI_COMM_WORLD);
     //rtime1 = rtime1 - MPI_Wtime();
     /* Backward transform in-place */
     Cp3dfft_btran_c2r(A,A,op_b);
     rtime1 = MPI_Wtime()-rtime1;
     rfast = ((rtime1 < rfast) ? rtime1 : rfast);
     ravg = ravg + rtime1;

   }

   /* Free work space */
  //Cp3dfft_clean();

  /* Check results */
  cdiff = 0.0; p = A;
  for(z=0;z < isize[2];z++)
    for(y=0;y < isize[1];y++)  {
       sinyz =siny[y]*sinz[z];
       for(x=0;x < isize[0];x++) {
          ans = sinx[x]*sinyz;
          if(cdiff < fabs(*p - ans))
           cdiff = fabs(*p - ans);
	   p++;
        }
    }

#ifndef SINGLE_PREC
   MPI_Reduce(&cdiff,&ccdiff,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
#else
   MPI_Reduce(&cdiff,&ccdiff,1,MPI_REAL,MPI_MAX,0,MPI_COMM_WORLD);
#endif

  get_timers(timers);

  if(proc_id == 0) {
#ifndef SINGLE_PREC
    prec = 1.0e-14;
#else
    prec = 1.0e-5;
#endif
    if(ccdiff > prec * Nglob*0.25)
      printf("Results are incorrect\n");
    else
      printf("Results are correct\n");

    printf("max diff =%g\n",ccdiff);
  }

  /* Gather timing statistics */
  MPI_Reduce(&rfast,&rfast2,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(&ravg,&rtime2,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
  
  for (i=0;i < 12;i++) {
    timers[i] = timers[i] / ((double) n);
  }

  MPI_Reduce(&timers,&gt,12,MPI_DOUBLE,MPI_SUM,0,MPI_COMM_WORLD);
  MPI_Reduce(&timers,&gt1,12,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);
  MPI_Reduce(&timers,&gt2,12,MPI_DOUBLE,MPI_MIN,0,MPI_COMM_WORLD);

  for (i=0;i < 12;i++) {
    gt[i] = gt[i]/ ((double) nproc);
  }

  if(proc_id == 0) {
     printf("Average=%lg\n",rtime2/((double) n));
     printf("Fastest=%lg\n",rfast2);
     for(i=0;i < 12;i++) {
       printf("timer[%d] (avg/max/min): %lE %lE %lE\n",i+1,gt[i],gt1[i],gt2[i]);
     }
  }


  MPI_Finalize();

}

#ifndef SINGLE_PREC
void mult_array(double *A,long int nar,double f)
#else
void mult_array(float *A,long int nar,double f)
#endif
{
  long int i;

  for(i=0;i < nar;i++)
    A[i] *= f;
}

#ifndef SINGLE_PREC
void print_all(double *A,long int nar,int proc_id,long int Nglob)
#else
void print_all(float *A,long int nar,int proc_id,long int Nglob)
#endif
{
  int x,y,z,conf,Fstart[3],Fsize[3],Fend[3];
  long int i;

  conf = 2;
  Cp3dfft_get_dims(Fstart,Fend,Fsize,conf);
  /*
  Fsize[0] *= 2;
  Fstart[0] = (Fstart[0]-1)*2;
  */
  for(i=0;i < nar;i+=2)
    if(fabs(A[i]) + fabs(A[i+1]) > Nglob *1.25e-4) {
      z = i/(2*Fsize[0]*Fsize[1]);
      y = i/(2*Fsize[0]) - z*Fsize[1];
      x = i/2-z*Fsize[0]*Fsize[1] - y*Fsize[0];
#ifndef SINGLE_PREC
      printf("(%d,%d,%d) %.16lg %.16lg\n",x+Fstart[0],y+Fstart[1],z+Fstart[2],A[i],A[i+1]);
#else
      printf("(%d,%d,%d) %.8lg %.8lg\n",x+Fstart[0],y+Fstart[1],z+Fstart[2],A[i],A[i+1]);
#endif
    }
}
