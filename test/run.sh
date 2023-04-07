#!/bin/bash
TEST_DIR=$(cd $(dirname $0);pwd)

prepare_data(){

    cp ${TEST_DIR}/back-sword-test.2d ${TEST_DIR}/sword-test.2d
    cp ${TEST_DIR}/back-rw-test.2d ${TEST_DIR}/rw-test.2d
}

prepare_data
${TEST_DIR}/tst-swd-fs1.out ${TEST_DIR}/sword-test.2d

prepare_data
${TEST_DIR}/tst-swd-fs2.out ${TEST_DIR}/sword-test.2d

prepare_data
${TEST_DIR}/tst-swd-fs3.out ${TEST_DIR}/rw-test.2d

prepare_data
${TEST_DIR}/tst-swd-fs4.out ${TEST_DIR}/rw-test.2d

prepare_data
${TEST_DIR}/tst-swd-fat.out

prepare_data
