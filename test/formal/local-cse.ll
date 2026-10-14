define i32 @commutative_add(i32 %a, i32 %b) {
entry:
  %x = add i32 %a, %b
  %y = add i32 %b, %a
  %r = xor i32 %x, %y
  ret i32 %r
}
