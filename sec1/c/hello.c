typedef unsigned short CHAR16;
typedef unsigned long long EFI_STATUS;
typedef void *EFI_HANDLE;

// 下のEFI_TEXT_STRINGのエイリアス定義で用いるので、
// 一旦、構造体の名前を宣言
struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// 関数ポインタの型エイリアスの作成
// ここではstruct _EFI_SIMPLE_... と CHAR16 を引数に取り、EFI_STATUSを返す
// 関数ポインタの型をEFI_TEXT_STRINGというエイリアスで登録している。
typedef EFI_STATUS (*EFI_TEXT_STRING)(
  struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
  CHAR16                                  *String);

// 上記、struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOLの定義。
// EFI_TEXT_STRINGと循環参照(?)の形になるため、一旦上で宣言を行っている
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
  void  *dummy;
  // 関数ポインタの格納
  EFI_TEXT_STRING OutputString;  
}  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
  char  dummy[52];
  EFI_HANDLE  ConsoleOutHandle;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
} EFI_SYSTEM_TABLE;

EFI_STATUS EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  // 文字列リテラルのLはワイド文字列を表す。
  // ワイド文字列は1文字を複数バイトで表す（バイト数はコンパイラ依存）
  SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello, world!\n");
  
  while(1);

  return 0;
}