#!/bin/bash

# add -ksp_view to view the configurations options of the solver and to view the memory consumption of the direct solver
# add -ksp_monitor_true_residual to plot the residual of the outer gmres
# add -log_summary to view a summary of the run

# Direct solver
# modify -nrefinement to increase or decrease the number of refinement
./fsitimedependent -input "./input/turek.neu" -n_timesteps 500 -autosave_time_interval 50 -nlevel 1 -rhof 1000 -muf 1 -rhos 10000 -E 1500000 -ni 0.5 -ic_bdc "../../../../lib64/libfsi2_td_2d_turek_hron_benchmark_bdc.so" -outer_ksp_solver "preonly" -max_outer_solver_iter 1 -nrefinement 4 -std_output stdOutput.txt > stdOutput.txt

# GMRES-MG-Richardson-ASM-ILU-MUMPS solver
#./fsitimedependent -input "./input/turek.neu" -n_timesteps 500 -autosave_time_interval 50 -rhof 1000 -muf 1 -rhos 10000 -E 1500000 -ni 0.5 -ic_bdc "../../../../lib64/libfsi2_td_2d_turek_hron_benchmark_bdc.so" -outer_ksp_solver "gmres" -max_outer_solver_iter 60 -ksp_monitor_true_residual -nrefinement 2  -nlevel 2  -std_output turek_mg.txt > turek_mg.txt