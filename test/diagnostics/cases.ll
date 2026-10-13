define i32 @local_case(i32 %a, i32 %b) {
entry:
  %x = add i32 %a, %b
  %y = add i32 %b, %a
  %r = xor i32 %x, %y
  ret i32 %r
}

define i32 @global_case(i32 %a, i32 %b) {
entry:
  %x = mul i32 %a, %b
  br label %next
next:
  %y = mul i32 %b, %a
  ret i32 %y
}

define i32 @licm_case(i32 %a, i32 %b) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %invariant = add i32 %a, %b
  %unsafe = sdiv i32 %a, %b
  %next = add i32 %i, 1
  %done = icmp eq i32 %next, 8
  br i1 %done, label %exit, label %loop
exit:
  %r = xor i32 %invariant, %unsafe
  ret i32 %r
}

