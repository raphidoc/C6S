gfortran -c testF.f90
g++ -c testC.cpp
g++ -o test testF.o testC.o -lgfortran
