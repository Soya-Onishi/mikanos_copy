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

#### なぜかマウスが動かない

原因は元プログラムと違いmain関数に直接ではなく、別の関数を定義してその中に処理を書いたため。
処理の途中でスタックのポインタをグローバル変数に入れる箇所があり、
これによってダングリングポインタが発生してしまっていた。
他のクラスのインスタンスのように、配置new用のバッファを作ってそこにインスタンスをおいてあげればOK。

```
  usb::xhci::Controller xhc{xhc_mmio_base};

  if(0x8086 == pci::ReadVendorId(*xhc_dev)) {
    SwitchEhci2Xhci(*xhc_dev);
  } else {
    auto err = xhc.Initialize();
  }

  xhc.Run();

  ::xhc = &xhc;
  __asm__("sti");
```

上記の`usb::xhci::Controller xhc{xhc_mmio_base};`で確保したxhcのポインタを
`::xhc = &xhc;`のようにグローバル変数に入れているのが原因。

## キューを使った割り込み高速化

### TODO

- 関数のシグネチャのあとに`const`が続くのはなんのため？

### メモ

#### テンプレートを使ったクラスを使うことができない

テンプレートを使ったメソッドなどの定義（宣言だけではない）はヘッダファイルに含めるか、
`.cpp`ファイルもインクルードするなりして、使う側でその定義が見えていないといけない。
理由はコンパイラがテンプレートに値を型や値を実際に当てはめた定義を見つけることができないから。