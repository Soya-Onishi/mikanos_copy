# メモリ管理

## UEFIメモリマップ

### メモ

`#ifdef __cplusplus`によってC++のコンパイル時のみコンパイル対象になる範囲を指定できる。

## セグメンテーションの設定

### TODO

#### `DescriptorType`の2重定義について

ほぼ同じ内容の`DescriptorType`が`interrupt.hpp`と`segment.hpp`の2箇所で定義されている。
`DescriptorType`はIDTとGDTで共通なのか調べる。

#### GDTのディスクリプタについて

GDTで定義可能なディスクリプタの種類を調べる

#### セグメントレジスタの指定値について

CSを`1 << 3`、SSを`2 << 3`としていた。
3ビットシフトする理由は？

### メモ

#### 参照するセグメントについて

`lgdt`命令で参照するGDTが変わったとき、
CSレジスタがGDTの領域を超える場所を参照するような値になっていれば、
不正な領域を参照したりするはず。

実際にはそのようなことは発生しない。
これはディスクリプタキャッシュと呼ばれる領域にディスクリプタの内容を写して、
普段はそこを参照しているから。

セグメントレジスタに変更が入ったとき、その参照先のディスクリプタの内容をキャッシュにコピーする。

## ページングの設定

### TODO

- ページングの各ページの構成がどうなっているのか調べる

### メモ

#### セグメントセレクタの設定時に3ビット左にシフトさせる理由

GDTのインデックス選択は3ビット目からだから。

```
0:1 : Request Privilege Level
2:2 : Table Indicator 0:GDT, 1:LDT
3:- : Index
```

#### 今回作成したページング

とりあえず今はリニアアドレスと物理アドレスが
そのままの対応関係になるアイデンティティマッピングになっている。
そのため、`page_directory[i_pdpt][i_pd]`には固定値を入れておけば良い。
