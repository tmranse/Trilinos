C    Copyright (c) 2014, Sandia Corporation.
C    Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
C    the U.S. Governement retains certain rights in this software.
C    
C    Redistribution and use in source and binary forms, with or without
C    modification, are permitted provided that the following conditions are
C    met:
C    
C        * Redistributions of source code must retain the above copyright
C          notice, this list of conditions and the following disclaimer.
C    
C        * Redistributions in binary form must reproduce the above
C          copyright notice, this list of conditions and the following
C          disclaimer in the documentation and/or other materials provided
C          with the distribution.
C    
C        * Neither the name of Sandia Corporation nor the names of its
C          contributors may be used to endorse or promote products derived
C          from this software without specific prior written permission.
C    
C    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
C    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
C    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
C    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
C    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
C    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
C    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
C    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
C    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
C    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
C    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
C    

C $Id: trblk.f,v 1.2 1996/07/01 14:09:12 gdsjaar Exp $
C $Log: trblk.f,v $
C Revision 1.2  1996/07/01 14:09:12  gdsjaar
C Trap exodus files with non-hex (3d) and non-quad (2d) element
C blocks. Exits with error message instead of core dump
C
C Revision 1.1.1.1  1991/02/21 15:46:06  gdsjaar
C NUMBERS: Greg Sjaardema, initial Unix release
C
c Revision 1.1  1991/02/21  15:46:05  gdsjaar
c Initial revision
c
      SUBROUTINE TRBLK (IDELB, NUMELB, NUMLNK, MAT, NELBLK, NNODES)
      DIMENSION IDELB(*), NUMELB(*), NUMLNK(*), MAT(6,*)
C
      IBEG = 0
      IEND = 0
      DO 10 I=1, NELBLK
        if (numlnk(i) .ne. nnodes) then
          CALL PRTERR('FATAL',
     *      'Numbers only handles 3D hex or 2D quadrilateral elements')
          STOP 'Element Error'
        else
         IBEG = IEND + 1
         IEND = IEND + NUMELB(I)
         MAT(1,I) = IDELB(I)
         MAT(2,I) = NUMELB(I)
         MAT(3,I) = IBEG
         MAT(4,I) = IEND
         MAT(5,I) = 1
         MAT(6,I) = I
       end if
   10 CONTINUE
      RETURN
      END
