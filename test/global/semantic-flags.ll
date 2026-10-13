; CHECK-LABEL: define i32 @flags_do_not_match(
; CHECK: %root = add nsw i32 %a, %b
; CHECK: next:
; CHECK: %plain = add i32 %a, %b
define i32 @flags_do_not_match(i32 %a, i32 %b) {
entry:
  %root = add nsw i32 %a, %b
  br label %next
next:
  %plain = add i32 %a, %b
  %r = xor i32 %root, %plain
  ret i32 %r
}

; CHECK-LABEL: define i32 @flags_match(
; CHECK: %root = mul nuw i32 %a, %b
; CHECK: next:
; CHECK-NOT: mul nuw i32
; CHECK: ret i32 %root
define i32 @flags_match(i32 %a, i32 %b) {
entry:
  %root = mul nuw i32 %a, %b
  br label %next
next:
  %duplicate = mul nuw i32 %b, %a
  ret i32 %duplicate
}

