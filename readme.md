
# scrsndcpy


## ■はじめに
このソフトは、USB(もしくはWi-Fi)で接続されたAndroidデバイスの画面と音声を、PC上でミラーリングするために作られました

実質的には[scrcpy](https://github.com/Genymobile/scrcpy)、[sndcpy](https://github.com/rom1v/sndcpy)を合体させただけです  

## ■動作環境
・Windows10 home 64bit バージョン 20H2  
※64bit版でしか動作しません

・音声のミラーリングにはAndroid 10以降が必要です

## ■使い方

事前に、Android側で[開発者向けオプション]を表示してから[USBデバッグ]を有効にしてください  
https://developer.android.com/studio/command-line/adb.html#Enabling

USBデバッグを有効にしたAndoridとPCをUSBケーブルで接続すると、  
自動的に"Device list"に接続したデバイスのシリアル番号が表示されます  
Device listからミラーリングしたいデバイスを選択した後に、  
"Screen Sound Copy"ボタンを押すとAndroidの画面と音声がPC側にミラーリングされます

・Wi-Fi経由での接続方法  
Configから"デバイスへWi-Fi経由での接続を行えるようにする"にチェックが入っている状態で、PCにデバイスを接続してください  
すると、デバイス側がWi-Fi経由での接続を待ち受けるようになるので、Device listから"192.168.0.x:5555"を選択した後に"Screen Sound Copy"ボタンを押せばミラーリングが開始されます  


## ■設定

・自動ログインパスワード  
おそらくAndroid11以降でないと動作しません

・バッファの最大サンプル数  
デバイスから送られてくる音声データの転送量が一定ではないので、音ズレを防ぐために、  
バッファにある音声サンプル数が、この数値を超えると音声サンプルを破棄して自動的に調整します  
この数値を増やすとバッファが安定しますが、音声のレイテンシが増えます(音が遅れる)

・ミラーリング開始時にデバイスの音声をミュートする  
おそらくAndroid11以降でないと動作しません


## ■アンインストールの方法
レジストリも何もいじっていないので、scrsndcpyフォルダ事削除すれば終わりです

## ■免責
作者は、このソフトによって生じた如何なる損害にも、  
修正や更新も、責任を負わないこととします。  
使用にあたっては、自己責任でお願いします。  
 
何かあれば[メールフォーム](https://ws.formzu.net/fgen/S37403840/)か、githubの[issues](https://github.com/amate/scrsndcpy/issues)にお願いします。  


## ■使用ライブラリ・素材

app  

- scrcpy  
https://github.com/Genymobile/scrcpy

- sndcpy  
https://github.com/rom1v/sndcpy

library

- Boost C++ Libraries  
https://www.boost.org/

- JSON for Modern C++  
https://github.com/nlohmann/json

- WTL  
https://sourceforge.net/projects/wtl/

icon
- ICON HOIHOI  
http://iconhoihoi.oops.jp/

## ■How to build
実行ファイルの生成には、Visual Studio 2019が必要です  

事前にboostをビルドして、パスを通しておいてください

ビルドに必要なライブラリは、[vcpkg](https://github.com/microsoft/vcpkg)によって自動的にダウンロード、ライブラリ(.lib)の生成が行われるようになっています

https://github.com/microsoft/vcpkg/archive/refs/tags/2021.04.30.zip  
まず上記URLから"vcpkg-2021.04.30.zip"をダウンロードして、適当なフォルダに解凍します

解凍したフォルダ内にある "bootstrap-vcpkg.bat" を実行すれば、同じフォルダ内に "vcpkg.exe"が生成されます

コマンドプロンプトを管理者権限で起動し

>cd vcpkg.exeのあるフォルダまでのパス

を入力しEnter、コマンドプロンプトのカレントディレクトリを変更します

次に
>vcpkg integrate install

と入力しEnter

>Applied user-wide integration for this vcpkg root.

と表示されたら成功です

Visual Studio 2019で "scrsndcpy.sln"を開き、  
デバッグ->デバッグの開始 を実行すれば、vcpkgがライブラリの準備をした後、実行ファイルが生成されます

## ■著作権表示
Copyright (C) 2021 amate

自分が書いた部分のソースコードは、MIT Licenseとします

## ■開発支援
https://www.kiigo.jp/disp/CSfGoodsPage_001.jsp?GOODS_NO=9

## ■更新履歴

<pre>

v1.0
・公開

</pre>
