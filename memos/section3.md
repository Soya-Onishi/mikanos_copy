# 第3章 画面表示の練習とブートローダ

## 初めてのカーネル

ブートローダからカーネルへのジャンプで`hlt`のところで止まらない。
`RIP`も`0x3fb73016`とカーネルのエントリポイントの`0x100120`から離れすぎている。
原因は以下のように、カーネルのelfファイル内の`.text`セクションにおいて、
`Address`と`Offset`の値がずれていて、無茶苦茶な命令を実行していたから。

```
Section Headers:
  [Nr] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  [ 0]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  [ 1] .text             PROGBITS         0000000000101120  00000120
       0000000000000013  0000000000000000  AX       0     0     16
  [ 2] .debug_abbrev     PROGBITS         0000000000000000  00000133
       000000000000002d  0000000000000000           0     0     1

```

### 対処方法1

[同様のissue](https://github.com/uchan-nos/os-from-zero/issues/134)があったため、そこによると使用しているコンパイラとリンカ（clangとld.lld）のバージョンが
本で使用しているものと異なるため。合わせるとうまく行くらしい。
本で使用しているバージョンはclangが10、ld.lldが7らしいが、
Ubuntu 22.04ではaptの検索で引っかかったのがclangで11以上だったため、
なにも考えずに`apt install`で解決とはいかない。

### 対処方法2

`ld.lld`コマンドのオプションに`--section-start=.text=0x100120`を追加すると一旦は対処できた。

が、その後すぐにこの対処方法は使えなくなった。
対処方法が使えなくなった理由はセクション位置を上記オプションで変更すると、
今度は`Offset`の値が変化してしまい、`Offset`の値を調整するオプションも見当たらなかったため。

### 対処方法3

[対処方法1で記載したissue](https://github.com/uchan-nos/os-from-zero/issues/134#issuecomment-1272229284)で記載の通り、
素直に第4章のELFローダの実装（mikanosリポジトリの`osbook_day04d`タグ）をやってしまったほうがいい。


## ブートローダからピクセルを描く

たまたまmikanosのissueで[FreePoolがimplicit declaration errorになる](https://github.com/uchan-nos/mikanos/issues/15)というものがあった。
本来は`FreePool()`ではなく`gBS->FreePool()`が正解。
