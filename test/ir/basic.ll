; CHECK-LABEL: define i32 @repeated(
; CHECK: %sum = add i32 %a, %b
; CHECK-NOT: add i32
; CHECK: %result = mul i32 %sum, %sum
define i32 @repeated(i32 %a, i32 %b) {
entry:
  %sum = add i32 %a, %b
  %again = add i32 %a, %b
  %result = mul i32 %sum, %again
  ret i32 %result
}

; CHECK-LABEL: define i32 @commutative(
; CHECK: %first = xor i32 %a, %b
; CHECK-NOT: xor i32
; CHECK: ret i32 %first
define i32 @commutative(i32 %a, i32 %b) {
entry:
  %first = xor i32 %a, %b
  %second = xor i32 %b, %a
  ret i32 %second
}

; CHECK-LABEL: define i32 @different_constants(
; CHECK: %x = add i32 %a, 1
; CHECK: %y = add i32 %a, 2
define i32 @different_constants(i32 %a) {
entry:
  %x = add i32 %a, 1
  %y = add i32 %a, 2
  %r = add i32 %x, %y
  ret i32 %r
}

