// replace 'R = X; return R;' with 'return R;'

// remove assignment
@ removal @
identifier VAR;
expression E;
type T;
identifier F;
@@
 T F(...)
 {
     ...
-    T VAR;
     ... when != VAR
-    VAR = E;
-    return VAR;
+    return E;
     ... when != VAR
 }
