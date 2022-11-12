# マルチタスク(2)

## イベントが来たら起床する

### TODO

#### メッセージキューについて

割り込み発生時に使用しているメッセージキューがメインタスク（カーネルのループ部分）にのみ投げているが、
本当にそれでいいのか？
Linuxカーネルではマウスなどの割り込み処理に対してどこに処理をオフロードするのかなど。

#### タスク（プロセス）の実行状態について

Linuxでは実行中や実行待機以外にどのような状態を持つのか。
また、どのように管理するのか調べてみる

#### 割り込み処理から通常の処理へリターンする流れについて

通常処理のスタックポインタに積み上げられる形になっている？
x86-64アーキテクチャで割り込み時の通常処理のデータがどうスワップされているのか調べる。
割り込み処理から抜けるときも工夫がありそうなので調べてみる。