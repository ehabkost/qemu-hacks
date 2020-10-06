// Aliases to children properties can use object_property_add_path_alias()
@@
expression obj, childprop, childptr, childsize, typename, sourceprop, targetprop;
expression errp;
@@
(
 object_initialize_child(obj, childprop, childptr, typename);
|
 object_initialize_child_with_props(obj, childprop, childptr,
                                    childsize, typename, errp, ...);
|
 object_property_add_child(obj, childprop, childptr);
)
 ...
(
-object_property_add_alias(obj, sourceprop,
-                          OBJECT(childptr), targetprop);
+object_property_add_path_alias(obj, sourceprop,
+                               childprop, targetprop);
|
-object_property_add_alias(obj, sourceprop,
-                          childptr, targetprop);
+object_property_add_path_alias(obj, sourceprop,
+                               childprop, targetprop);
)

// Aliases to own properties can use object_property_add_path_alias() with NULL path
@@
expression obj, sourceprop, targetprop;
@@
-object_property_add_alias(obj, sourceprop,
-                          obj, targetprop);
+object_property_add_path_alias(obj, sourceprop,
+                               NULL, targetprop);


// virtio_instance_init_common() always register a "virtio-backend" child property
@@
expression obj, vdev, typename;
expression sourceprop, targetprop;
@@
 virtio_instance_init_common(obj, &vdev, sizeof(vdev),
                             typename);
 ...
-object_property_add_alias(obj, sourceprop,
-                          OBJECT(&vdev), targetprop);
+object_property_add_path_alias(obj, sourceprop,
+                               "virtio-backend", targetprop);
