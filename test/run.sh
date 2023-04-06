#!/bin/sh
DATA_DIR=.

prepare_data(){

    cp ${DATA_DIR}/back-sword-test.2d ${DATA_DIR}/sword-test.2d
    cp ${DATA_DIR}/back-rw-test.2d ${DATA_DIR}/rw-test.2d
}

prepare_data
./tst-swd-fs1.out ${DATA_DIR}/sword-test.2d

prepare_data
./tst-swd-fs2.out ${DATA_DIR}/sword-test.2d

prepare_data
./tst-swd-fs3.out ${DATA_DIR}/rw-test.2d

prepare_data
./tst-swd-fs4.out ${DATA_DIR}/rw-test.2d

prepare_data
