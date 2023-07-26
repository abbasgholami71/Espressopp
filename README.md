# How to install on Raven at MPCDF cluster
The following command lines are used to build E++ on Raven at MPCDF computer cluster:

	module purge
	
	module load cmake doxygen git
	
	module load intel/21.6.0 impi/2021.6
	
	module load anaconda/3/2021.11
	
	module load boost-mpi/1.79 fftw-mpi/3.3.10 hdf5-mpi/1.12.2
	
	rm -rf espressopp espressopp_build random123
	
	git clone https://github.com/xzhh/espressopp.git or this repository
	
	git clone https://github.com/DEShawResearch/random123.git
	
	cmake -S espressopp -B espressopp_build -DCMAKE_C_COMPILER=icc -DCMAKE_CXX_COMPILER=icpc -DCMAKE_BUILD_TYPE=Release -DRANDOM123_ROOT_DIR=random123
	
	cmake --build espressopp_build --parallel 32
	
