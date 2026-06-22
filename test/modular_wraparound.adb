-- RUN: %lalvm --emit=llvm %s | %FileCheck %s

-- Modular wraparound for moduli the integer width does not realize on its own
-- (RM 3.5.4). The representation width byte-rounds up to a power of two, so the
-- width's wraparound alone is correct only when the modulus is exactly
-- 2**width. `+`/`-`/`*` on any other modulus are reduced explicitly. The body
-- calls each operation so the functions are not eliminated.

-- Pow2 (mod 128) is binary but not a byte power: mask the low 7 bits.
-- CHECK-DAG: and i8 %{{.*}}, 127

-- Kilo (mod 1000) is non-binary. `*` reaches (m-1)**2, so it widens and divides
-- with a `urem`.
-- CHECK-DAG: mul i32
-- CHECK-DAG: urem i32 %{{.*}}, 1000

-- `+`/`-` land at most one modulus outside the range, so they reduce with a
-- single conditional `- m` (GCC's division-free strategy), never a `urem`.
-- CHECK-DAG: icmp uge i32 %{{.*}}, 1000
-- CHECK-DAG: select i1

-- Subtraction forms `a + m - b` first to keep the intermediate non-negative.
-- CHECK-DAG: add i32 %{{.*}}, 1000

-- Every Kilo result truncates back from the widened type to i16.
-- CHECK-DAG: trunc i32 %{{.*}} to i16

-- Modular `/` is unsigned and stays in range, so it lowers to a bare unsigned
-- divide with no widening or reduction.
-- CHECK-DAG: udiv i16

-- Word (mod 65536 = 2**16) is a byte power: width wraparound is already exact,
-- so the `+` lowers to a plain wrapping add with no reduction.
-- CHECK-DAG: add i16

procedure Modular_Wraparound is
   type Pow2 is mod 128;
   type Kilo is mod 1000;
   type Word is mod 65536;

   function Add_P (X, Y : Pow2) return Pow2 is
   begin
      return X + Y;
   end Add_P;

   function Add_K (X, Y : Kilo) return Kilo is
   begin
      return X + Y;
   end Add_K;

   function Mul_K (X, Y : Kilo) return Kilo is
   begin
      return X * Y;
   end Mul_K;

   function Sub_K (X, Y : Kilo) return Kilo is
   begin
      return X - Y;
   end Sub_K;

   function Div_K (X, Y : Kilo) return Kilo is
   begin
      return X / Y;
   end Div_K;

   function Add_W (X, Y : Word) return Word is
   begin
      return X + Y;
   end Add_W;

   P : Pow2 := 100;
   K : Kilo := 700;
   W : Word := 60000;
begin
   P := Add_P (P, P);
   K := Add_K (K, K);
   K := Mul_K (K, K);
   K := Div_K (K, K);
   K := Sub_K (K, K);
   W := Add_W (W, W);
end Modular_Wraparound;
