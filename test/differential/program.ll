define i32 @forge_compute(i32 %a, i32 %b, i1 %choose, i32 %n) {
entry:
  %root = add i32 %a, %b
  br i1 %choose, label %left, label %right
left:
  %l = add i32 %b, %a
  br label %merge
right:
  %r = add i32 %a, %b
  br label %merge
merge:
  %selected = phi i32 [ %l, %left ], [ %r, %right ]
  br label %loop
loop:
  %i = phi i32 [ 0, %merge ], [ %next, %loop ]
  %acc = phi i32 [ %selected, %merge ], [ %nextacc, %loop ]
  %invariant = mul i32 %a, %b
  %mixed = xor i32 %invariant, %root
  %nextacc = add i32 %acc, %mixed
  %next = add nuw i32 %i, 1
  %done = icmp eq i32 %next, %n
  br i1 %done, label %exit, label %loop
exit:
  ret i32 %nextacc
}

