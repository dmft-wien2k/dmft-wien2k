#!/usr/bin/env python
# @Copyright 2007-2017 Kristjan Haule
from scipy import *
from scipy import linalg
import copy
import sys

def mprint(Us):
    for i in range(shape(Us)[0]):
        for j in range(shape(Us)[1]):
            print "%11.8f %11.8f  " % (real(Us[i,j]), imag(Us[i,j])),
        print

def MakeOrthogonal(a, b, ii):
    a -= (a[ii]/b[ii])*b
    a *= 1/sqrt(dot(a,a.conj()))
    b -= dot(b,a.conj())*a
    b *= 1/sqrt(dot(b,b.conj()))
    return (a,b)

def StringToMatrix(cfstr):
    mm=[]
    for line in cfstr.split('\n'):
        line = line.strip()
        if line:
            data = array(map(float,line.split()))
            mm.append( data[0::2]+data[1::2]*1j )
    mm=matrix(mm)
    return mm

def to_normalize(a):
    return 1./sqrt(abs(dot(conj(a), a)))

def swap(a,b):
    an = copy.deepcopy(a)
    bn = copy.deepcopy(b)
    return (bn,an)

def FindDegeneracies(Es,small=1e-4):
    """ gives an index list of degenerate state. For example, if Es[0]==Es[1] and Es[2] is different,
        it gives [[0,1],[2]]
    """
    deg=[]
    n=0
    while (n<len(Es)):
        degc = [n]
        i=n+1
        for i in range(n+1,len(Es)):
            if abs(Es[i]-Es[i-1])<small:
                degc.append(i)
            else:
                n=i
                break
        deg.append(degc)
        if i==len(Es)-1:
            if abs(Es[i]-Es[i-1])>=small:
                deg.append([i])
            break
    return deg

def ComputeUtU(T,ig):
    """ Computes the quantity that shows how close to is the transformation to being purely real.
    If UtU is identity, then the transformation is real. The mathematical definition of UtU is:
          (U^T * U)_{i,j} = ( \sum_m T[i][-m] (-1)^m T[j][m] )^*
    """
    N = shape(T)[1] # N should go from [-2,-1,0,1,2]
    minus_1_m = (-1)**(arange(N)+(N-1.)/2.) # this works for s,p,d,f systems in w2k convention.
    #print 'minus_1_m=', minus_1_m
    # This will contain (U^T * U)_{i,j} = ( \sum_m T[i][-m] (-1)^m T[j][m] )^*
    UtU = zeros((len(ig),len(ig)),dtype=complex)
    for i in range(len(ig)):
        v2 = T[i,::-1] * minus_1_m  # This is T[i][-m] (-1)^m
        #print 'v2=', v2
        for j in range(len(ig)):
            tmt = v2 * T[j,:]        # Now we have T[i][-m] (-1)^m T[j][m]
            #print 'tmt=', sum(tmt)
            UtU[i,j] = conj(sum(tmt)) # finally, sum it and conjugate, to get U^T * U
    return UtU

def TransformToSimilar(ig, Us, Es):
    """ If two or more states are degenerate, we can take a linear combination of them to obtaine more desired set of orbitals.
    This routine transform them with a unitary transformatin so that they are close to previous set of eigenvectors.
    This is useful in particular when H has small off-diagonal elements, which we would like to eliminate,
    and we call this routine iteratively
    """
    i0 = ig[0]     # this is the first index to the degenerate set of eigenvectors
    i2 = ig[-1]+1  # this is the last index to the degenerate set
    print 'Transforming with similarity transformation the degenerate eigenset: ', Es[i0:i2]
    # Want to make this set of eigenvectors to be close to original eigenvectors, which were determined in previous iteration.
    # This is not necessary, but convenient to keep the character similar to previous iteration. This is useful
    # in particular when H has small off-diagonal elements, which we would like to eliminate, and we call this
    # routine iteratively
    O = transpose(conj(Us[i0:i2,i0:i2]))
    (u_,s_,v_) = linalg.svd(O)
    R = dot(u_,v_)
    Usn = copy.copy(Us)
    Usn[:,i0:i2] = dot(Us[:,i0:i2],R)
    return Usn

def TransformToReal(final, ig, Es):
    """
    Here we will try to make the transformation real, so that ctqmc will have minimal sign problem, even when Coulomb='Full' is used.
    
    In the transformation final[i,m] m runs over angular momentum components. We will call final[i,m]==T[m].
    
    For the transformation to be real, it must obey T[m] = (-1)^m * T[-m]^* , because of the property of spherical harmonics.
      This is because our orbitals are Y_i = \sum_m T[i][m]*Y_{l,m}
      
    If the eigenvector T[m] is non-degenerate, than we are only allowed to multiply it with an arbirtrary phase,
      so that T[m] -> e^{i*p} T[m] .
      Then  e^{ip} T[m] = (-1)^m T[-m]^* e^{-ip}    and therefore
      e^{2*i*p} = (\sum_m  T[m] (-1)^m T[-m] )^*
    
    Because T[m] is unitary, we see that the phase to multiply such non-degenerate orbital is :
                    e^{i*p} = sqrt( (\sum_m  T[m] (-1)^m T[-m] )^* )
    Hopefully, all components then satisfy the above displayed condition for real orbitals.
    
    If two eigenvectors T[1][m] and T[2][m] (corresponding to two orbitals) are degenerate,
    we can try to find a 2x2 unitary transformation, which hopefully makes both orbitals real.
    We can trasform
    
       R[1:2][m] = e^{i*s*x}*T[1:2][m] with arbitrary x, and s being Pauli matrices.
       
       After that, R can be forced to satisfy
          R[1,m] = (-1)^m * R[1,-m]^* and
          R[2,m] = (-1)^m * R[2,-m]^*
    
       If we write R[i][m] = U[i,:] * T[:][m] where we call e^{i*s*x} == U, and U is an arbitrary unitary matrix, we have
    
           U[i,:] * T[:][m] = (-1)^m * (U[i,:] * T[:][-m])^*
    
           This is actually generalization of the above 2x2 problem to an arbitrary dimension, as long as U is unitary n*n matrix.
           Now, since T and U are unitary, we can write
    
           (U^T * U)_{i,j} = ( \sum_m T[i][-m] (-1)^m T[j][m] )^*
           
           which is the equation for the unknown U. The problem is that we can only get U^T*U and not U itself. Infact, this does not
           determine U uniquely, as every real unitary transformation would give U^T*U is indentity, and hence
           we could choose U to be identity. But we do not want to apply any unitary transformation which is real, and we are looking
           for the complex part of the transformation only, which will make transformation T real. 
           We will therefore proceed in the following way:
           
        We can easily prove that (U^T * U)==X is a complex symmetric matrix. But not hermitian, just symmetric complex.
        
        If it is diagonalizable, then it must satisfy Z^T * X * Z = D  (with D diagonal, and Z^T * Z = 1).
          ( see : http://www.netlib.org/utk/people/JackDongarra/etemplates/node263.html )
        
        Then we can take  Ut = Z e^{i*p} Z^T where Z^T * Z = 1, so that
        
          X = Ut^T * Ut = Z e^{2*i*p} Z^T    or  e^{2*i*p} = Z^T X Z
        
        Therefore, we can diagonalize X to get Z and eigenvalues e^{2*i*p}, and then take the square root of the eigenvalues, to get Ut.
        Unfortunately, such Ut is not necessary unitary. Indeed Ut is unitary only if Z is a real vector.
        
        Next we will find the closes possible unitary transformation to Ut. This can be found by SVD.
        So, once we have  Ut = Z e^{i*p} Z^T we can perform SVD decomposition on it, and take the unitary part of it, i.e.,
          Ut = u*s*v and than  U = u*v
    
    """
    i0 = ig[0]
    i2 = ig[-1]+1
    print 'Transforming the possibly degenerate eigenset to a real set of orbitals:', Es[i0:i2]
    
    T = final[i0:i2,:]
    UtU = ComputeUtU(T, ig)
    print 'UtU='
    mprint( UtU )
    
    if len(ig)==1: # one dimensional substace. Only overal phase is allowed to change.
        phase = sqrt(UtU[0,0])
        phase = phase/abs(phase)
        final[i0:i2] = phase*T[:,:]
    else:
        if allclose( UtU, identity(len(ig)), rtol=1e-05, atol=1e-05):
            print 'UtU is close to identity!'
        else:
            UtU = 0.5*(UtU + transpose(UtU)) # This matrix should be symmetric, but we now symmetrize it just to avoid numeric rounding
            ee = linalg.eig(UtU)
            wu = ee[0]
            Zu = ee[1]
            print 'wu=', wu
            print 'Zu='
            Zum = matrix(Zu)
            mprint( Zum )
            Iw = Zum.T * Zum
            print 'Should be identity:'
            mprint( Iw )
            swu = sqrt(wu)
            print 'e^{i*phi}=', swu
            Utry = Zum * diag(swu) * Zum.T
            print 'Utry='
            mprint(Utry)
            Iw = Utry.H * Utry
            print 'Is identity?'
            mprint( Iw )
            (u_,s_,v_) = linalg.svd(Utry)
            Ubest = dot(u_,v_)
            print 'singular values=', s_
            print 'Ubest='
            mprint(Ubest)
            R = Ubest * matrix(T)
            final[i0:i2,:] = R[:,:]
    return final

def GiveNewT2C(Hc, T2C):
    """ The main routine, which computes T2C that diagonalized Hc, and is close to the previous
    T2C transformation, if possible, and it makes the new orbitals Y_i = \sum_m T2C[i,m] Y_{lm} real,
    if possible.
    """
    ee = linalg.eigh(Hc)
    Es = ee[0]
    Us0= ee[1]
    Us = matrix(ee[1])
    
    #print 'In Eigensystem:'
    #mprint(Us.H * Hc * Us)
    # Us.H * Hc * Us  === diagonal
    print 'Eigenvalues=', Es.tolist()
    
    print 'Starting with transformation in crystal harmonics='
    mprint(Us)
    print
    
    # Finds if there are any degeneracies in eigenvalues.
    deg = FindDegeneracies(Es)
    print 'deg=', deg
    
    for ig in deg:
        if len(ig)>1:
            # Two or more states are degenerate, we transform them with a unitary transformation,
            # so that they are close to previous set of eigenvectors.
            # This is not necessary, but convenient to keep the character similar to previous iteration. This is useful
            # in particular when H has small off-diagonal elements, which we would like to eliminate, and we call this
            # routine iteratively
            Us = TransformToSimilar(ig, Us, Es)
    print 'Next, the transformation in crystal harmonics='
    mprint(Us)
    print
    final = array( Us.T*T2C )
    print 'And the same transformation in spheric harmonics='
    mprint( final )
    
    #  Here we will try to make the transformation real, so that ctqmc will have minimal sign problem even when Full is used.
    for ig in deg:
        final = TransformToReal(final, ig, Es)

    # finally checking if all transformations are real
    for ig in deg:
        i0 = ig[0]
        i2 = ig[-1]+1
        #print 'Checking the set of orbitals:', Es[i0:i2]
        UtU = ComputeUtU(final[i0:i2,:], ig)
        if allclose( UtU, identity(len(ig)),  rtol=1e-04, atol=1e-04 ):
            print ':SUCCESS For orbital', ig, 'the final transformation is real'
        else:
            print """:WARNING: The set of rbitals """, ig, """ could not be made purely real. You should use only Coulomb='Ising' and avoid Coulomb='Full' """
            print 'UtU=',
            mprint(UtU)
    print
    return final


if __name__ == '__main__':
    
    #strHc="""
    #0.47118858     0.00000000       0.28653635     0.00000000      -0.00000000     0.28653635   
    #0.28653635    -0.00000000       0.47118858    -0.00000000      -0.00000000     0.28653635   
    #0.00000000    -0.28653635      -0.00000000    -0.28653635       0.47118858     0.00000000   
    #"""
    #strT2C="""
    #  0.00000000 0.00000000   0.70710679 0.00000000   0.00000000 0.00000000  -0.70710679 0.00000000   0.00000000 0.00000000 
    #  0.00000000 0.00000000   0.00000000 0.70710679   0.00000000 0.00000000   0.00000000 0.70710679   0.00000000 0.00000000 
    # -0.70710679 0.00000000   0.00000000 0.00000000   0.00000000 0.00000000   0.00000000 0.00000000   0.70710679 0.00000000 
    #"""
    #strT2Crest="""
    #  0.00000000 0.00000000   0.00000000 0.00000000   1.00000000 0.00000000   0.00000000 0.00000000   0.00000000 0.00000000 
    #  0.70710679 0.00000000   0.00000000 0.00000000   0.00000000 0.00000000   0.00000000 0.00000000   0.70710679 0.00000000 
    #"""
    
    if len(sys.argv)<2:
        print
        print "Give input file which conatins impurity levels (strHc) and transformation (strT2C)"
        print
        sys.exit(0)
    fpar=sys.argv[1]
    execfile(fpar)
    
    Hc = StringToMatrix(strHc)
    Hc = 0.5*(Hc+transpose(conjugate(Hc))) # Should be Hermitian, so we enforce Hermicity
    print 'shape(Hc)=', shape(Hc)
    
    T2C0 = StringToMatrix(strT2C)
    print 'shape(T2C0)=', shape(T2C0)
    T2C = T2C0[:len(Hc),:]
    print 'shape(T2C)=', shape(T2C)
    T2Crest = T2C0[len(Hc):,:]
    print 'shape(T2Crest)=', shape(T2Crest)
    
    final = GiveNewT2C(Hc, T2C)

    #mprint( matrix(final) * matrix(final).H )
    
    print 'Rotation to input : '
    mprint( final )
    mprint( T2Crest )
    
