import lit.formats
import os
import shutil

config.name = 'lalvm'
config.test_format = lit.formats.ShTest(True)
config.suffixes = ['.adb', '.mlir']
config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = config.lalvm_obj_root

# %lalvm-make must precede %lalvm: substitutions apply in order, and %lalvm
# would otherwise rewrite the `%lalvm` prefix of `%lalvm-make`.
config.substitutions.append(('%lalvm-make',
    os.path.join(config.test_source_root, '..', 'tools', 'lalvm-make')))
config.substitutions.append(('%lalvm',
    os.path.join(config.lalvm_tools_dir, 'lalvm')))

# Execution tests bind and link with the GNAT toolchain; mark `gnat` available
# so they can `REQUIRES: gnat` and skip cleanly where it is not installed.
if shutil.which('gnatmake'):
    config.available_features.add('gnat')
config.substitutions.append(('%FileCheck',
    os.path.join(config.llvm_tools_dir, 'FileCheck')))
config.substitutions.append(('%not',
    os.path.join(config.llvm_tools_dir, 'not')))
config.substitutions.append(('%llc',
    os.path.join(config.llvm_tools_dir, 'llc')))
config.substitutions.append(('%llvm-dwarfdump',
    os.path.join(config.llvm_tools_dir, 'llvm-dwarfdump')))
