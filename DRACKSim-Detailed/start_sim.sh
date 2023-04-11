# Node-to-Pool configurations (4:1,8:1,4:2,8:2)
# Local-to_remote memory ratio configuration (75:25, 50:50, 75:25)

# WL-1 mg,sp,bt,ft
# WL-2 lulesh,miniFE,SimpleMOC,XSBench


# Used for producing reported results in the experimentation section

# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 1 -- ./workloads/mg.B.x &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 2 -- ./workloads/sp.C.x &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 3 -- ./workloads/bt.C.x &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 4 -- ./workloads/ft.C.x &

# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 5 -- ./workloads/mg.B.x &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 6 -- ./workloads/sp.C.x &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 7 -- ./workloads/bt.C.x &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 8 -- ./workloads/ft.C.x &

# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 1 -S 10000000 -- ./workloads/lulesh2.0 -s 120 &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 2 -S 210000000 -- ./workloads/miniFE.x -nx 140 -ny 140 -nz 140 &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 3 -- ./workloads/SimpleMOC -s -t 4 &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 4 -- ./workloads/XSBench -s small -t 4 &

# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 5 -S 10000000 -- ./workloads/lulesh2.0 -s 120 &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 6 -S 210000000 -- ./workloads/miniFE.x -nx 140 -ny 140 -nz 140 &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 7 -- ./workloads/SimpleMOC -s -t 4 &
# OMP_NUM_THREADS=4 ../../../pin -t obj-intel64/Detailed.so -N 8 -- ./workloads/XSBench -s small -t 4 &



#!/bin/bash

# Used for validation with gem5
#4-cores
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./FFT -p4 -m16	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./FMM <input.4.256	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./BARNES <n256-p4
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./OCEAN -n66 -p4	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RADIOSITY -p 4 -batch -room	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RAYTRACE -a4 -p4 ./teapot.env	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./WATER-NSQUARED <w512-p4
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./WATER-SPATIAL <w512-p4
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./CHOLESKY <tk14.O
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./LU -n128 -p4 -b16
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RADIX -p4 -n131072

#2-cores
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./FFT -p2 -m16	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./FMM <input.2.256	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./BARNES <n256-p2
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./OCEAN -n66 -p2	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RADIOSITY -p 4 -batch -room	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RAYTRACE -a4 -p2 ./teapot.env	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./WATER-NSQUARED <n512-p2
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./WATER-SPATIAL <n512-p2
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./CHOLESKY <tk14.O
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./LU -n128 -p2 -b16
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RADIX -p2 -n131072


#1-cores
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./FFT -p1 -m16	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./FMM <input.1.256	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./BARNES <n256-p1
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./OCEAN -n66 -p1	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RADIOSITY -p 4 -batch -room	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RAYTRACE -a4 -p1 ./teapot.env	#50000000
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./WATER-NSQUARED <n512-p1
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./WATER-SPATIAL <n512-p1
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./CHOLESKY <tk14.O
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./LU -n128 -p1 -b16
# ../../../pin -t obj-intel64/Detailed.so -N 1 -S 0 -- ./RADIX -p1 -n131072
