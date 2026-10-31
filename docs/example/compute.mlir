module @compute {
  ada.type @standard.integer : i32 = #ada.int_info<range -2147483648 to 2147483647>
  ada.subp @compute(%x: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer> {
    %0 = ada.call @compute.double(%x) : (!ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
    ada.decls {
      ada.subp private @compute.double(%n: !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer> {
        %1 = ada.binop "+" %n, %n checks<overflow> : (!ada.qual<i32, @standard.integer>, !ada.qual<i32, @standard.integer>) -> !ada.qual<i32, @standard.integer>
        ada.return %1 : !ada.qual<i32, @standard.integer>
      }
    }
    ada.return %0 : !ada.qual<i32, @standard.integer>
  }
}
