# Doxygen input filter for the generated AdaOpsEnums.h.inc.
#
# mlir-tblgen re-emits the `symbolizeEnum` primary-template declaration
#
#     template <typename EnumType>
#     ::std::optional<EnumType> symbolizeEnum(::llvm::StringRef);
#
# once per enum (AdaBinaryOp, AdaRelationalOp, ...). With more than one enum the
# header declares it identically several times, and doxygen cannot match the
# duplicates to a unique member ("no matching file member found"). This filter
# keeps the first declaration and drops the later ones, so doxygen sees a single
# declaration. Everything else in the header is passed through unchanged.

# Hold back a `template <typename EnumType>` line: it may introduce a
# declaration we want to drop, so we cannot print it until we see the next line.
/^template <typename EnumType>$/ {
    pending = $0
    have_pending = 1
    next
}

# The line right after a held-back template line.
have_pending {
    have_pending = 0
    # Drop the second and later `symbolizeEnum` declarations (template + decl).
    if (/symbolizeEnum\(::llvm::StringRef\);$/ && seen++)
        next
    print pending
}

{ print }
