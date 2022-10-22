# 割り込みとFIFO

## 割り込みベクタ

### TODO

- DPL(Descriptor Privilege Level)の意味、0以外にするとどうなるのか。
- Interrupt Stack Tableの値の意味について

### メモ

#### `__attribute__((packed))``

`__attribute__((packed))`普通は構造体のフィールドはアラインメントされるが、
このコンパイラ拡張を用いることでフィールドを詰めて配置される。

#### `std::array` is not member of std

`#include <array>`が必要。

## MSI割り込み

### TODO

- `reinterpret_cast`と`static_cast`の違い
- `main.cpp`内の`::xhc`の意味
- `sti`命令の意味

### メモ

MSI(Message Singled Interrupts)登場以前のPCIでは
４本のみの割り込み信号線を複数のデバイスが共有する形になっていた。

MSIを用いることでメモリバスからの特定メモリへの書き込みにより
割り込みを発生させるという形に変化した。
その結果、１つのデバイスから複数の割り込みを通知することも可能になった。

メモリアドレスやメッセージのフォーマットはPCI規格の範疇外であり、
x86_64で独自に決定されている。




