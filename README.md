# UNIX版 S-OS

## 本リポジトリについて

UNIX版S-OSのソースコードです。
本リポジトリには, 楯岡 孝道氏が作成されたUNIX版 S-OSを元に現在のUNIX/Linux環境に向けた修正を実施したソースコードが格納されています。

UNIX版S-OSの詳細は, 同ディレクトリの`README`ファイルを参照ください。

## UNIX版 S-OSの構築方法

本ディレクトリに配置されている`autogen.sh`を以下のように実行することで, `configure`スクリプトが生成されます。

```shell
$ ./autogen.sh
```

上記実施後, 以下のコマンドにより`configure`を実行してください。

```
$ ./configure
```

`configure`には以下のオプションを指定することができます。

|オプション|意味|
|---|---|
|--with-delay=N|コンソールの遅延書き込み機能(試験中)を有効にします。Nに画面更新周期をus単位で指定してください。例えば, `--with-delay=20`とすることで, 20us毎に画面の描写を行います。|
|--with-rcfile=FILE|リソースファイルの名前を指定します。例えば, リソースファイル名を`sos.ini`に設定する場合は, `--with-rcfile=sos.ini`と指定します。未指定時は, `.sosrc`になります。|
|--with-forceansi|Termcapの`tgetenv`関数による端末種別獲得に失敗した場合, ANSI互換端末と見なして動作を継続するオプションです。|
|--with-wmkeymap|`Word Master`ライクなキー操作を行うように設定します。未指定時は, Emacsライクな操作になります。|


## S-OSとは

S-OSとは, Oh! Mz誌 1985年6月号で提唱されたZ80搭載マシン用の各機種共通入出力システム (Common Input/Output System - CIOS)の名称です。

S-OSの特徴や歴史については, Oh!石氏作成の[S-OSのページ～THE SENTINEL～](http://www.retropc.net/ohishi/s-os/)をご参照ください。
