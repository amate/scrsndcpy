
# scrsndcpy

![](https://raw.githubusercontent.com/amate/scrsndcpy/images/images/ss2.png)

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

もしミラーリングウィンドウが表示されない場合、ソフトを終了した状態で同じフォルダ内にある"adb kill-server.bat"を実行してみてください

・Wi-Fi経由での接続方法  
Configから"デバイスへWi-Fi経由での接続を行えるようにする"にチェックが入っている状態で、PCにデバイスを接続してください  
すると、デバイス側がWi-Fi経由での接続を待ち受けるようになるので、Device listから"192.168.0.x:5555"を選択した後に"Screen Sound Copy"ボタンを押せばミラーリングが開始されます  

画面の位置を記憶するには、Ctrlキーを押しながら"Streaming"ボタンを押してください

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
Copyright (C) 2021-2022 amate

自分が書いた部分のソースコードは、MIT Licenseとします

## ■更新履歴

<pre>

v1.3
・[update] scrcpyを 1.24 に更新
・[update] sndcpyを 1.1 に更新
・[fix] delayFrameで scrcpyの更新に追従 (フックするdll名を変更 avutil-56.dll -> avutil-57.dll)
・[fix] 設定ダイアログで、"ミラーリング開始時にデバイスの音声をミュートする"設定が保存されなかったのを修正
・[misc] turnScreenOffのデフォルトを false に変更
・[change] 最初に [Screen Sound Copy]実行したときに表示されるウィンドウの位置を変更 (タイトルバーがモニター外に隠れないようにした)
・[fix] スリープ復帰時の再接続処理を二重実行しないように修正
・[fix] 起動時に"adb start-server"を実行しないようにした (scrcpyを更新すると起動しなくなってしまうので)
・[change] sndcpy実行時にAndroid側の確認ダイアログが出なくなった
・[fix] sndcpyへの接続時にリトライ処理を追加
・[fix] 音声再生停止時の画面点灯状態の確認に"Display Power: state"から"mWakefulness=Awake"かどう確認するように変更
・[change] 音声再生時の初回バッファ削除時間を２秒から１秒に変更
・[change] sndcpyのアンインストール処理を省いた
・[fix] Android11でも Mute/UnMute/Toggle Mute できるようにした
・[add] "adb kill-server.bat" を追加 (adbの調子悪い時に使う)

v1.2
・[update] 開発環境を VisualStudio 2019 から VisualStudio 2022 へ移行
・[update] scrcpy を 1.20 に更新
・[add] スリープからの復帰時に自動的にデバイスへ再接続する オプションを追加
・[add] 画面のリサイズを禁止する オプションを追加
・[add] toggle muteで、PCとデバイスのミュート状態を反転させる オプションを追加
・[add] ビデオバッファのオプションを追加
・[add] 一定時間ミュートが続く、かつ、画面がオフの時にPC側での音声再生を止めるようにした (PCのスリープ移行を妨げてしまうことへの対策)
・[fix] デバイスに接続したまま scrsndcpyを終了させると、強制終了するのを修正
・[fix] サウンドデバイスの共有モードで使用されるサンプルレートとビットの深さが、"16ビット、48000 Hz"ではないときに、音の再生に失敗する問題を修正
・[change] PUTLOGは、メインスレッドで実行するように変更
・[fix]  自動実行は２秒後に実行するように変更 (起動してすぐは adb.exeの起動などでデバイスの接続が切れる場合があるので)
・[fix] デバイスの切断時も Streaming状態を解除するようにした
・[add] デバイスが、縦画面から横画面になったときに、設定した画面サイズに復帰させるようにした
・[fix] 接続失敗時に、プッシュボタンが押された状態になっているのを修正
・[fix] Screen Sound Copy実行・終了時に、sndcpyを一度アンインストールする処理を追加 (認証の自動許可処理が早くなった)
・[add] funcStartSndcpy失敗時に adb.exe kill-server start-serverする処理を追加
・[add] WM_RUN_SCRSNDCPY = WM_APP + 10で外部からScreen Sound Copyを実行できるようにした

v1.0
・公開

</pre>
