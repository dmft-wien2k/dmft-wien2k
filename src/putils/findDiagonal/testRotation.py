# @Copyright 2007 Kristjan Haule
from scipy import *
from scipy import linalg

def mprint(Us):
    for i in range(shape(Us)[0]):
        for j in range(shape(Us)[1]):
            print "%9.6f %9.6f  " % (real(Us[i,j]), imag(Us[i,j])),
        print

def StringToMatrix(cfstr):
    mm=[]
    for line in cfstr.split('\n'):
        line = line.strip()
        if line:
            data = array(map(float,line.split()))
            mm.append( data[0::2]+data[1::2]*1j )
    mm=matrix(mm)
    return mm

sT2C="""
  0.00000000  0.00000000    0.00000000 -0.70710679    0.00000000  0.00000000   0.00000000 -0.70710679    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000 
  0.00000000  0.00000000    0.70710679  0.00000000    0.00000000  0.00000000  -0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000 
 -0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000 
  0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000 -0.70710679    0.00000000  0.00000000   0.00000000 -0.70710679    0.00000000  0.00000000  
  0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.70710679  0.00000000    0.00000000  0.00000000  -0.70710679  0.00000000    0.00000000  0.00000000  
  0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000   -0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.70710679  0.00000000  
  0.00000000  0.00000000    0.00000000  0.00000000    1.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000 
  0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    1.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000  
  0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000 
  0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.00000000  0.00000000    0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   0.00000000  0.00000000    0.70710679  0.00000000  
"""
sT2C="""
 0.000000  0.000000    1.000000 -0.000000    0.000000  0.000000   -0.000000  0.000000   -0.000000 -0.000000    0.000000 -0.000000    0.000000  0.000000    0.000000  0.000000   -0.000000 -0.000000   -0.000000  0.000000  
 0.000000  0.000000    0.000000 -0.000000    0.000000 -0.000000   -0.000000  0.000000   -0.000000 -0.000000   -0.000000 -0.000000    0.000000 -0.000000    0.000000 -0.000000    1.000000  0.000000    0.000000  0.000000  
-0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.692963 -0.000000    0.000000  0.000000   -0.366607  0.354260    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.366607 -0.354260  
 0.366607  0.354260   -0.000000  0.000000    0.000000  0.000000   -0.000000  0.000000   -0.366607 -0.354260    0.000000 -0.000000    0.692963  0.000000    0.000000  0.000000   -0.000000 -0.000000   -0.000000  0.000000  
-0.000000  0.000000    0.000000 -0.000000    0.000000  0.000000    0.720973  0.000000    0.000000  0.000000    0.352364 -0.340497   -0.000000 -0.000000    0.000000  0.000000   -0.000000 -0.000000   -0.352364  0.340497  
-0.352364 -0.340497    0.000000 -0.000000    0.000000  0.000000   -0.000000  0.000000    0.352364  0.340497    0.000000 -0.000000    0.720973  0.000000    0.000000  0.000000   -0.000000 -0.000000   -0.000000  0.000000  
 0.000000  0.000000    0.000000  0.000000   -1.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000  
 0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    1.000000  0.000000    0.000000  0.000000    0.000000  0.000000  
 0.707107  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.707107  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000  
 0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.707107  0.000000    0.000000  0.000000    0.000000  0.000000    0.000000  0.000000    0.707107  0.000000  
"""

sT2C="""
 0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.72097658 -0.00000000    0.00000000  0.00000000    0.35236224 -0.34049559    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000   -0.35236224  0.34049559  
-0.35236224 -0.34049559    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.35236224  0.34049559    0.00000000 -0.00000000    0.72097658  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000  
 0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    1.00000000  0.00000000    0.00000000  0.00000000  
 0.00000000  0.00000000    1.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000  
 0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.69295944  0.00000000    0.00000000  0.00000000   -0.36660865  0.35426221    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.36660865 -0.35426221  
 0.36660865  0.35426221    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000 -0.00000000   -0.36660865 -0.35426221    0.00000000  0.00000000    0.69295944  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000 -0.00000000  
 0.00000000  0.00000000    0.00000000  0.00000000    1.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000  
 0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    1.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000  
 0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000  
 0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.70710679  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.00000000  0.00000000    0.70710679  0.00000000  
"""

sEimp0="""
   -1.60596     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.80166    -0.74845       0.00000     0.00000      -0.37295    -0.37079       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000   
    0.00000     0.00000      -2.28610     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.46960    -0.46848       0.00000     0.00000       0.00000     0.00000   
    0.00000     0.00000       0.00000     0.00000      -0.10607     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.46960    -0.46848       0.00000     0.00000   
    0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -1.77172     0.00000       0.00000     0.00000      -0.00101    -0.01135       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.37295    -0.37079   
    0.80166     0.74845       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.51129     0.00000       0.00000     0.00000      -0.00101    -0.01135       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000   
    0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.00101     0.01135       0.00000     0.00000      -0.51129     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.80166    -0.74845   
   -0.37295     0.37079       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.00101     0.01135       0.00000     0.00000      -1.77172     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000   
    0.00000     0.00000      -0.46960     0.46848       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.10607     0.00000       0.00000     0.00000       0.00000     0.00000   
    0.00000     0.00000       0.00000     0.00000      -0.46960     0.46848       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -2.28610     0.00000       0.00000     0.00000   
    0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -0.37295     0.37079       0.00000     0.00000       0.80166     0.74845       0.00000     0.00000       0.00000     0.00000       0.00000     0.00000      -1.60596     0.00000   
"""

Eimp0 = StringToMatrix(sEimp0)

T2C = StringToMatrix(sT2C)

print 'Det=', linalg.det(T2C)

REimp1  = conj(T2C) * Eimp0 * T2C.T

mprint( REimp1 )
