#!/bin/sh
# GDBによるリモートデバッグ
# 使用法:
#  gdb.sh sosバイナリ コマンドライン
# を実行すると, ポート番号1234で待ち受ける
#
gdbserver localhost:1234 $@
