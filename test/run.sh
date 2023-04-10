#!/bin/bash
TEST_DIR=$(cd $(dirname $0);pwd)
TPS="tst-swd-fs1.out tst-swd-fs2.out tst-swd-fs3.out tst-swd-fs4.out tst-swd-fs5.out tst-swd-fat.out"
prepare_data(){

    cp ${TEST_DIR}/back-sword-test.2d ${TEST_DIR}/sword-test.2d
    cp ${TEST_DIR}/back-rw-test.2d ${TEST_DIR}/rw-test.2d
}

for tp in ${TPS}
do
    echo "Run: ${tp}"
    if [ -f ${TEST_DIR}/${tp} ]; then
	prepare_data
	${TEST_DIR}/${tp} ${TEST_DIR}/sword-test.2d
    fi
done

prepare_data
