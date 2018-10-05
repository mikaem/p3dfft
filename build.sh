module load cray-fftw
export PREFIX="/scratch/x_mortenm/Software/cray"
./configure  --enable-cray --enable-oned --enable-fftw --with-fftw=$FFTW_DIR/.. --enable-measure --prefix=$PREFIX --exec-prefix=$PREFIX FC=ftn CC=cc
make
make install

