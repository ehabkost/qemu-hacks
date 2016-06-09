// Replace unnecessary usage of local_err variable with
// direct usage of errp argument

@@
expression list ARGS;
expression F2;
identifier LOCAL_ERR;
expression ERRP;
idexpression V;
typedef Error;
expression I;
@@
 {
     ...
-    Error *LOCAL_ERR;
     ... when != LOCAL_ERR
(
-    F2(ARGS, &LOCAL_ERR);
-    error_propagate(ERRP, LOCAL_ERR);
+    F2(ARGS, ERRP);
|
-    V = F2(ARGS, &LOCAL_ERR);
-    error_propagate(ERRP, LOCAL_ERR);
+    V = F2(ARGS, ERRP);
)
     ... when != LOCAL_ERR
 }
