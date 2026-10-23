// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 The LALVM Project

// At -O1, canonicalization collapses `ada.requalify` chain into one op, and
// removes a chain back to the starting type.

// RUN: %lalvm -O1 --emit=mlir %s | %FileCheck %s

module {
  // CHECK-LABEL: ada.subp @chain
  // CHECK-NEXT:    %[[R:.*]] = ada.requalify %arg0 : !ada.qual<i32, @integer> to !ada.qual<i32, @positive>
  // CHECK-NEXT:    ada.return %[[R]] : !ada.qual<i32, @positive>
  ada.subp @chain(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @positive> {
    %a = ada.requalify %v : !ada.qual<i32, @integer> to !ada.qual<i32, @natural>
    %b = ada.requalify %a : !ada.qual<i32, @natural> to !ada.qual<i32, @positive>
    ada.return %b : !ada.qual<i32, @positive>
  }

  // CHECK-LABEL: ada.subp @round_trip
  // CHECK-NEXT:    ada.return %arg0 : !ada.qual<i32, @integer>
  ada.subp @round_trip(%v: !ada.qual<i32, @integer>) -> !ada.qual<i32, @integer> {
    %a = ada.requalify %v : !ada.qual<i32, @integer> to !ada.qual<i32, @natural>
    %b = ada.requalify %a : !ada.qual<i32, @natural> to !ada.qual<i32, @integer>
    ada.return %b : !ada.qual<i32, @integer>
  }
}
