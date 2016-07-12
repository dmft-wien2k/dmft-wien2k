#! /usr/bin/env python
# -*- coding: utf-8 -*-

class Config:
  prefix      = "./install"    # Installation path

  compiler    = "INTEL"        # Compiler
  
  cflags      = "-O3"          # compiling flags for C++ programs
  fflags      = "-O3"          # compiling flags for Fortran programs
  ldflags     = ""             # linking flags for Fortran programs
  ompflag     = "-fopenmp"     # linker/compiler flag for openmp

  mpi_define  = "-D_MPI"       # should be -D_MPI for mpi code and empty for serial code.
  pcc         = "mpicc"        # C compiler 
  pcxx        = "mpicxx"       # C++ compiler 
  pfc         = "mpif90"       # Fortran compiler 
  
  blasname    = "MKL"             # BLAS   library
  blaslib     = "-mkl" # BLAS   library
  lapacklib   = ""             # LAPACK library
  fftwlib     = "-L/opt/fftw/lib64 -lfftw3_omp -lfftw3"     # FFTW   library
  gsl         = "-L/usr/lib64/  -lgslcblas -lgsl"  # GSL    library

  f2pylib     = "--f90flags='-openmp ' --opt='-fast'" # adding extra libraries for f2py
  f2pyflag    = "--opt='-O3' " # adding extra options to f2py

  arflags     = "rc"           # ar flags

  make        = "make"
  def __init__(self, version):
    self.version = version
  def __getattr__(self,key):
    return None
  
