((c++-mode
  . ((eval . (setq flycheck-clang-include-path
                   (list (expand-file-name
                          "include"
                          (locate-dominating-file default-directory
                                                  ".dir-locals.el")))))
     (eglot-server-programs . ((c++-mode . ("clangd" "--compile-commands-dir=build")))))))
