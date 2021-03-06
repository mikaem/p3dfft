! This file is part of P3DFFT library
!
!    P3DFFT
!
!    Software Framework for Scalable Fourier Transforms in Three Dimensions
!
!    Copyright (C) 2006-2010 Dmitry Pekurovsky
!    Copyright (C) 2006-2010 University of California
!    Copyright (C) 2010-2011 Jens Henrik Goebbert
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

!========================================================
! Transpose back Y to X pencils

      subroutine bcomm2_many(source,dest,nv,t,tc)
!========================================================

      implicit none

      complex(p3dfft_type) dest(nxhp,jisize,kjsize,nv)
#ifdef STRIDE1
      complex(p3dfft_type) source(ny_fft,iisize,kjsize,nv)
#else
      complex(p3dfft_type) source(iisize,ny_fft,kjsize,nv)
#endif
      real(r8) t,tc
      integer x,y,z,i,ierr,ix,iy,x2,y2,l,j,nv,dim
      integer(i8) position,pos1,pos0,pos2
      integer sndcnts(0:iproc-1)
      integer rcvcnts(0:iproc-1)
      integer sndstrt(0:iproc-1)
      integer rcvstrt(0:iproc-1)

      tc = tc - MPI_Wtime()

! Pack and exchange x-z buffers in rows

!$OMP PARALLEL DO private(i,j,pos0,pos1,pos2,ix,iy,x2,y2,position,x,y,z) collapse(2)
      do i=0,iproc-1

	do j=1,nv

#ifdef USE_EVEN
         pos0 = (IfCntMax*nv*i +(j-1) * KrSndCnts(i))/(p3dfft_type*2) +1
#else
         pos0 = (KrSndStrt(i) * nv + (j-1) * KrSndCnts(i))/(p3dfft_type*2) +1
#endif

         do z=1,kjsize

#ifdef STRIDE1
            pos1 = pos0
            do y=jist(i),jien(i),nby1
               y2 = min(y+nby1-1,jien(i))
               do x=1,iisize,nbx
                  x2 = min(x+nbx-1,iisize)
                  pos2 = pos1 + x-1
                  do iy = y,y2
                     position = pos2
                     do ix=x,x2
                        buf1(position) = source(iy,ix,z,j)
                        position = position + 1
                     enddo
                     pos2 = pos2 + iisize
                  enddo
               enddo
               pos1 = pos1 + iisize*nby1
            enddo

#else
            position = pos0
            do y=jist(i),jien(i)
               do x=1,iisize
                  buf1(position) = source(x,y,z,j)
                  position = position + 1
               enddo
            enddo
#endif

            pos0 = pos0 + iisize*jisz(i)
         enddo
      enddo
      enddo

      tc = tc + MPI_Wtime()
      t = t - MPI_Wtime()

#ifdef USE_EVEN
      call mpi_alltoall (buf1,IfCntMax*nv,mpi_byte, buf2,IfCntMax*nv,mpi_byte,mpi_comm_row,ierr)
#else
      sndcnts = KrSndCnts * nv
      sndstrt = KrSndStrt * nv
      rcvcnts = KrRcvCnts * nv
      rcvstrt = KrRcvStrt * nv

      call mpi_alltoallv (buf1,SndCnts, SndStrt, mpi_byte, buf2,RcvCnts,RcvStrt,mpi_byte,mpi_comm_row,ierr)
#endif

      t = t + MPI_Wtime()
      tc = tc - MPI_Wtime()

! Unpack receive buffers into dest

!$OMP PARALLEL DO private(i,j,pos0,position,x,y,z)
      do i=0,iproc-1

#ifdef USE_EVEN
         pos0 = i*IfCntMax*nv/(p3dfft_type*2) + 1
#else
         pos0 = IfSndStrt(i)*nv/(p3dfft_type*2) + 1
#endif

	do j=1,nv
         do z=1,kjsize
            position = pos0
            do y=1,jisize
               do x=iist(i),iien(i)
                  dest(x,y,z,j) = buf2(position)
                  position = position +1
               enddo
            enddo
            pos0 = pos0 + iisz(i)*jisize
         enddo
	 enddo
      enddo
      do j=1,nv
      do z=1,kjsize
         do y=1,jisize
	    do x=nxhpc+1,nxhp
	       dest(x,y,z,j) = 0.
	    enddo
	 enddo
      enddo
      enddo

      tc = tc + MPI_Wtime()

      return
      end subroutine

      subroutine bcomm2(source,dest,t,tc)
!========================================================

      implicit none

      complex(p3dfft_type) dest(nxhp,jisize,kjsize)
#ifdef STRIDE1
      complex(p3dfft_type) source(ny_fft,iisize,kjsize)
#else
      complex(p3dfft_type) source(iisize,ny_fft,kjsize)
#endif
      real(r8) t,tc
      integer x,y,z,i,ierr,ix,iy,x2,y2,l
      integer(i8) position,pos1,pos0,pos2

      tc = tc - MPI_Wtime()

! Pack and exchange x-z buffers in rows

!$OMP PARALLEL DO private(i,pos0,pos1,pos2,ix,iy,x2,y2,position,x,y,z) collapse(2)
      do i=0,iproc-1
         do z=1,kjsize

#ifdef STRIDE1
#ifdef USE_EVEN
         pos1 = i*IfCntMax/(p3dfft_type*2)+1 +(z-1)*iisize*jisz(i)
#else
         pos1 = IfRcvStrt(i)/(p3dfft_type*2)+1 +(z-1)*iisize*jisz(i)
#endif
            do y=jist(i),jien(i),nby1
               y2 = min(y+nby1-1,jien(i))
               do x=1,iisize,nbx
                  x2 = min(x+nbx-1,iisize)
                  pos2 = pos1 + x-1
                  do iy = y,y2
                     position = pos2
                     do ix=x,x2
                        buf1(position) = source(iy,ix,z)
                        position = position + 1
                     enddo
                     pos2 = pos2 + iisize
                  enddo
               enddo
               pos1 = pos1 + iisize*nby1
            enddo

#else
#ifdef USE_EVEN
         position = i*IfCntMax/(p3dfft_type*2)+1 + (z-1)*iisize*jisz(i)
#else
         position = IfRcvStrt(i)/(p3dfft_type*2)+1+(z-1)*iisize*jisz(i)
#endif
            do y=jist(i),jien(i)
               do x=1,iisize
                  buf1(position) = source(x,y,z)
                  position = position + 1
               enddo
            enddo
#endif
         enddo
      enddo

      tc = tc + MPI_Wtime()
      t =  MPI_Wtime()

#ifdef USE_EVEN
      call mpi_alltoall (buf1,IfCntMax,mpi_byte, buf2,IfCntMax,mpi_byte,mpi_comm_row,ierr)
#else
      call mpi_alltoallv (buf1,KrSndCnts, KrSndStrt, mpi_byte, buf2,KrRcvCnts,KrRcvStrt,mpi_byte,mpi_comm_row,ierr)
#endif

      t = MPI_Wtime() - t
      tc = tc - MPI_Wtime()

! Unpack receive buffers into dest

!$OMP PARALLEL DO private(i,pos0,position,x,y,z)
      do i=0,iproc-1
#ifdef USE_EVEN
         pos0 = i*IfCntMax/(p3dfft_type*2) + 1
#else
         pos0 = IfSndStrt(i)/(p3dfft_type*2) + 1
#endif
         do z=1,kjsize
            position = pos0
            do y=1,jisize
               do x=iist(i),iien(i)
                  dest(x,y,z) = buf2(position)
                  position = position +1
               enddo
            enddo
            pos0 = pos0 + iisz(i)*jisize
         enddo
      enddo
      do z=1,kjsize
         do y=1,jisize
	    do x=nxhpc+1,nxhp
	       dest(x,y,z) = 0.
	    enddo
	 enddo
      enddo

      tc = tc + MPI_Wtime()

      return
      end subroutine
