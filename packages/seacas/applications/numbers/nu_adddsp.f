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

C $Id: adddsp.f,v 1.1 1991/02/21 15:42:08 gdsjaar Exp $
C $Log: adddsp.f,v $
C Revision 1.1  1991/02/21 15:42:08  gdsjaar
C Initial revision
C
      SUBROUTINE ADDDSP (COORDS, DSP)
      DIMENSION COORDS (NUMNP,*), DSP(NUMNP,*)
CC
      include 'nu_numg.blk'
      IF (NDIM .EQ. 2) THEN
         DO 10 J=1,NUMNP
            DSP (J, 1) = COORDS (J, 1) + DSP (J, 1)
            DSP (J, 2) = COORDS (J, 2) + DSP (J, 2)
   10    CONTINUE
      ELSE
         DO 20 J=1,NUMNP
            DSP (J, 1) = COORDS (J, 1) + DSP (J, 1)
            DSP (J, 2) = COORDS (J, 2) + DSP (J, 2)
            DSP (J, 3) = COORDS (J, 3) + DSP (J, 3)
   20    CONTINUE
      END IF
CC
      RETURN
      END
