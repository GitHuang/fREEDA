Notes for FreeBSD
=================

Ports used
----------
- [g95](http://www.freshports.org/lang/g95/) - Fortran compiler: cd /usr/ports/lang/g95/ && make install clean

  - does not build with FreeBSD clang version 3.0 (branches/release_30 142614) 20111021
  - does not compile trilinos:

      - /usr/local/bin/g95 CMakeFiles/cmTryCompileExec.dir/testFortranCompiler.f.o -o cmTryCompileExec
      - ld: cannot find -lf95
      - gmake[1]: *** [cmTryCompileExec] Error 1
      - gmake: *** [cmTryCompileExec/fast] Error 2

- [BLAS](http://www.freshports.org/math/blas/) - Basic Linear Algebra Subroutines: cd /usr/ports/math/blas/ && make install clean
- [LAPACK](http://www.freshports.org/math/lapack/) - A library of Fortran 77 subroutines for linear algebra: cd /usr/ports/math/lapack/ && make install clean
- gcc (gfortran46)

useful links
------------

- http://www.netlib.org/lapack95/
- http://gcc.gnu.org/fortran/
- http://gcc.gnu.org/wiki/GFortran
- http://gfortran.com/

status
------

- not in ports collection
- building gfortran:

  - devel/gcc and option "Fortran" set
  - *OR*
  - [wiki instructions](http://gcc.gnu.org/wiki/GFortranSource) and [Installing GCC](http://gcc.gnu.org/install/)

- building trilinos fails with g95
