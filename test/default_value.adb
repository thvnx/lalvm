-- RUN: %lalvm --emit=mlir %s | %FileCheck %s
-- XFAIL: libadalang-26

-- An object of a scalar type with a Default_Value aspect (RM 3.5) and no
-- explicit initializer is default-initialized. The default is a type property,
-- so a subtype inherits it (Z). X (default 7) is then read to initialize W.

-- CHECK-LABEL: ada.subp @default_value
-- CHECK: %[[DIRECT:.*]] = ada.constant : !ada.qual<i8, @default_value.small> = 7
-- CHECK: %x = ada.object : !ada.ref<!ada.qual<i8, @default_value.small>> <- %[[DIRECT]]
-- CHECK: %[[DEFAULT:.*]] = ada.constant : !ada.qual<i8, @default_value.small> = 7
-- CHECK: %[[INHERITED:.*]] = ada.convert implicit %[[DEFAULT]] : !ada.qual<i8, @default_value.small> to !ada.qual<i8, @default_value.s3>
-- CHECK: %z = ada.object : !ada.ref<!ada.qual<i8, @default_value.s3>> <- %[[INHERITED]]
-- CHECK: %[[READ:.*]] = ada.read %x :
-- CHECK: %w = ada.object : !ada.ref<!ada.qual<i8, @default_value.small>> <- %[[READ]]

procedure Default_Value is
   type Small is range 1 .. 100 with Default_Value => 7;
   subtype S3 is Small;
   X : Small;      -- direct default
   Z : S3;         -- inherited default (subtype)
   W : Small := X; -- reads X
begin
   null;
end Default_Value;
