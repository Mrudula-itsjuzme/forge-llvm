; Flags are semantic: an unflagged add cannot reuse an nsw add.
; CHECK-LABEL: define i32 @different_flags(
; CHECK: %x = add nsw i32 %a, %b
; CHECK: %y = add i32 %a, %b
define i32 @different_flags(i32 %a, i32 %b) {
entry:
  %x = add nsw i32 %a, %b
  %y = add i32 %a, %b
  %r = xor i32 %x, %y
  ret i32 %r
}

; Equal flags permit reuse.
; CHECK-LABEL: define i32 @same_unsigned_flags(
; CHECK: %x = mul nuw i32 %a, %b
; CHECK-NOT: %y = mul
define i32 @same_unsigned_flags(i32 %a, i32 %b) {
entry:
  %x = mul nuw i32 %a, %b
  %y = mul nuw i32 %b, %a
  %r = add i32 %x, %y
  ret i32 %r
}

; Literal undef and poison are deliberately untouched.
; CHECK-LABEL: define i32 @undef_literal(
; CHECK: %x = add i32 %a, undef
; CHECK: %y = add i32 %a, undef
define i32 @undef_literal(i32 %a) {
entry:
  %x = add i32 %a, undef
  %y = add i32 %a, undef
  %r = xor i32 %x, %y
  ret i32 %r
}

; Calls and stores do not invalidate pure SSA arithmetic.
; CHECK-LABEL: define i32 @intervening_effects(
; CHECK: %x = add i32 %a, %b
; CHECK: call void @opaque()
; CHECK-NOT: %y = add
declare void @opaque()
define i32 @intervening_effects(i32 %a, i32 %b, ptr %p) {
entry:
  %x = add i32 %a, %b
  store i32 7, ptr %p
  call void @opaque()
  %y = add i32 %a, %b
  ret i32 %y
}

